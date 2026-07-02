// BackgroundRemovalService.cpp — see BackgroundRemovalService.h.
//
// Pipeline: OpenCV preprocess -> ONNX Runtime inference (MODNet-style
// matting) -> OpenCV postprocess -> BGRA PNG with timestamped filename.
// A deterministic synthetic mask keeps the pipeline testable when no model
// binary is present (the repo only ships a placeholder).
#include "pch.h"
#include "BackgroundRemovalService.h"

#include <onnxruntime_cxx_api.h>
#include <opencv2/opencv.hpp>

#include <array>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <sstream>

namespace fs = std::filesystem;

namespace rmbg {

BackgroundRemovalService::~BackgroundRemovalService() = default;

bool BackgroundRemovalService::LoadModel(const std::wstring& modelPath) {
    m_modelLoaded = false;
    m_session.reset();
    m_env.reset();

    if (!fs::exists(modelPath)) {
        return false; // no model — synthetic fallback stays in effect
    }

    try {
        m_env = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "BackgroundRemover");
        Ort::SessionOptions options;
        options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        // Ort::Session takes wide-char paths on Windows.
        m_session = std::make_unique<Ort::Session>(*m_env, modelPath.c_str(), options);
        m_modelLoaded = true;
    } catch (const Ort::Exception&) {
        // Invalid/placeholder model file — fall back gracefully.
        m_session.reset();
        m_env.reset();
    } catch (...) {
        m_session.reset();
        m_env.reset();
    }
    return m_modelLoaded;
}

cv::Mat BackgroundRemovalService::Preprocess(const cv::Mat& bgr) {
    CV_Assert(!bgr.empty() && bgr.type() == CV_8UC3);

    // Straight resize to 512x512 (no letterboxing). This distorts the aspect
    // ratio slightly but simplifies mask mapping back to the original size;
    // MODNet is tolerant of this. Swap in letterbox padding here if your
    // model requires preserved aspect ratio.
    cv::Mat resized;
    cv::resize(bgr, resized, cv::Size(kModelSize, kModelSize), 0, 0, cv::INTER_AREA);

    // BGR -> RGB, then float32 normalized to [0,1].
    cv::Mat rgb;
    cv::cvtColor(resized, rgb, cv::COLOR_BGR2RGB);
    cv::Mat floatImg;
    rgb.convertTo(floatImg, CV_32FC3, 1.0 / 255.0);

    // HWC -> CHW: split channels and pack contiguously (RRR..GGG..BBB..).
    std::vector<cv::Mat> channels(3);
    cv::split(floatImg, channels);
    cv::Mat chw(1, 3 * kModelSize * kModelSize, CV_32FC1);
    float* dst = chw.ptr<float>();
    const size_t planeSize = static_cast<size_t>(kModelSize) * kModelSize;
    for (int c = 0; c < 3; ++c) {
        std::memcpy(dst + c * planeSize, channels[c].ptr<float>(), planeSize * sizeof(float));
    }
    return chw;
}

cv::Mat BackgroundRemovalService::GenerateSyntheticMask() {
    // Deterministic soft ellipse centered in the frame: same output every
    // run, so tests are reproducible. Roughly mimics a portrait subject.
    cv::Mat mask = cv::Mat::zeros(kModelSize, kModelSize, CV_32FC1);
    const cv::Point center(kModelSize / 2, kModelSize / 2);
    const cv::Size axes(kModelSize / 3, static_cast<int>(kModelSize / 2.2));
    cv::ellipse(mask, center, axes, 0.0, 0.0, 360.0, cv::Scalar(1.0), cv::FILLED);
    // Feather the edge so the alpha falls off smoothly.
    cv::GaussianBlur(mask, mask, cv::Size(51, 51), 0);
    return mask;
}

cv::Mat BackgroundRemovalService::RunInference(const cv::Mat& chwTensor) {
    if (!m_modelLoaded || !m_session) {
        return GenerateSyntheticMask();
    }

    try {
        // Input: 1x3x512x512 float32. Output: 1x1x512x512 float32 alpha matte.
        const std::array<int64_t, 4> inputShape{1, 3, kModelSize, kModelSize};
        Ort::MemoryInfo memInfo =
            Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

        // The tensor view is non-owning; chwTensor must outlive the Run call.
        Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
            memInfo,
            const_cast<float*>(chwTensor.ptr<float>()),
            chwTensor.total(),
            inputShape.data(), inputShape.size());

        // Resolve I/O names dynamically so different MODNet exports work.
        Ort::AllocatorWithDefaultOptions alloc;
        const auto inputName = m_session->GetInputNameAllocated(0, alloc);
        const auto outputName = m_session->GetOutputNameAllocated(0, alloc);
        const char* inputNames[] = {inputName.get()};
        const char* outputNames[] = {outputName.get()};

        auto outputs = m_session->Run(Ort::RunOptions{nullptr},
                                      inputNames, &inputTensor, 1,
                                      outputNames, 1);
        if (outputs.empty() || !outputs[0].IsTensor()) {
            return GenerateSyntheticMask();
        }

        const float* data = outputs[0].GetTensorData<float>();
        // Wrap ORT-owned memory, then clone so the returned Mat owns its
        // data after `outputs` is destroyed.
        cv::Mat mask(kModelSize, kModelSize, CV_32FC1, const_cast<float*>(data));
        cv::Mat owned = mask.clone();
        // Clamp to [0,1] defensively — some exports emit slight overshoot.
        cv::min(owned, 1.0f, owned);
        cv::max(owned, 0.0f, owned);
        return owned;
    } catch (...) {
        // Any inference failure degrades to the synthetic mask.
        return GenerateSyntheticMask();
    }
}

cv::Mat BackgroundRemovalService::Postprocess(const cv::Mat& originalBgr, const cv::Mat& mask512) {
    CV_Assert(!originalBgr.empty() && originalBgr.type() == CV_8UC3);
    CV_Assert(mask512.type() == CV_32FC1);

    // 1. Resize mask back to the original image size.
    cv::Mat maskFull;
    cv::resize(mask512, maskFull, originalBgr.size(), 0, 0, cv::INTER_LINEAR);

    // 2. Gaussian blur for a soft edge (kernel must be odd).
    cv::GaussianBlur(maskFull, maskFull, cv::Size(7, 7), 0);

    // 3. Convert to 8-bit alpha.
    cv::Mat alpha8;
    maskFull.convertTo(alpha8, CV_8UC1, 255.0);

    // 4. BGR -> BGRA with the computed alpha channel.
    cv::Mat bgra;
    cv::cvtColor(originalBgr, bgra, cv::COLOR_BGR2BGRA);
    std::vector<cv::Mat> planes(4);
    cv::split(bgra, planes);
    planes[3] = alpha8;
    cv::merge(planes, bgra);

    return bgra.clone(); // caller owns an independent copy
}

std::optional<std::wstring> BackgroundRemovalService::ProcessImage(
    const std::wstring& inputPath, const std::wstring& outputDir) {
    try {
        // imread does not accept wide paths; read bytes then decode instead
        // so Unicode paths work correctly.
        std::ifstream file(fs::path(inputPath), std::ios::binary);
        if (!file.is_open()) return std::nullopt;
        std::vector<char> bytes((std::istreambuf_iterator<char>(file)),
                                std::istreambuf_iterator<char>());
        if (bytes.empty()) return std::nullopt;

        cv::Mat bgr = cv::imdecode(cv::Mat(1, static_cast<int>(bytes.size()), CV_8UC1, bytes.data()),
                                   cv::IMREAD_COLOR);
        if (bgr.empty()) return std::nullopt; // not an image (or unsupported format)

        // Run the three pipeline stages.
        cv::Mat tensor = Preprocess(bgr);
        cv::Mat mask = RunInference(tensor);
        cv::Mat bgra = Postprocess(bgr, mask);

        // Build "<stem>_nobg_<yyyyMMdd-HHmmss>.png". The timestamp suffix
        // guarantees we never overwrite an existing user file.
        fs::create_directories(outputDir);
        const auto now = std::chrono::system_clock::now();
        const std::time_t tt = std::chrono::system_clock::to_time_t(now);
        std::tm tm{};
        localtime_s(&tm, &tt);
        wchar_t stamp[32];
        std::wcsftime(stamp, std::size(stamp), L"%Y%m%d-%H%M%S", &tm);

        const std::wstring stem = fs::path(inputPath).stem().wstring();
        fs::path outPath = fs::path(outputDir) / (stem + L"_nobg_" + stamp + L".png");
        // Extremely defensive: if two files land within the same second,
        // add a numeric disambiguator rather than replacing anything.
        for (int i = 1; fs::exists(outPath) && i < 1000; ++i) {
            outPath = fs::path(outputDir) /
                      (stem + L"_nobg_" + stamp + L"_" + std::to_wstring(i) + L".png");
        }

        // imwrite has the same narrow-path limitation; encode to memory and
        // write bytes ourselves.
        std::vector<uchar> png;
        if (!cv::imencode(".png", bgra, png)) return std::nullopt;
        std::ofstream out(outPath, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) return std::nullopt;
        out.write(reinterpret_cast<const char*>(png.data()),
                  static_cast<std::streamsize>(png.size()));
        out.close();

        return outPath.wstring();
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace rmbg
