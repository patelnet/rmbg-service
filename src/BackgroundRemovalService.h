// BackgroundRemovalService.h — image background removal via ONNX Runtime
// (MODNet-style matting model) with a deterministic synthetic fallback.
#pragma once

#include <memory>
#include <optional>
#include <string>

#include <opencv2/core.hpp>

// Forward-declare ORT types so headers stay light.
namespace Ort {
class Env;
class Session;
} // namespace Ort

namespace rmbg {

// Runs the background-removal pipeline:
//   preprocess (OpenCV) -> inference (ONNX Runtime) -> postprocess -> BGRA PNG.
//
// If no valid model is available the service produces a deterministic
// synthetic mask (centered soft ellipse) so the full pipeline can be
// exercised end-to-end without shipping model binaries.
class BackgroundRemovalService {
public:
    // Model input/output geometry for MODNet-style models: 1x3x512x512 in,
    // 1x1x512x512 out.
    static constexpr int kModelSize = 512;

    BackgroundRemovalService() = default;
    ~BackgroundRemovalService();

    BackgroundRemovalService(const BackgroundRemovalService&) = delete;
    BackgroundRemovalService& operator=(const BackgroundRemovalService&) = delete;

    // Attempts to load the ONNX model. Returns true on success. On failure
    // the service remains usable via the synthetic fallback.
    bool LoadModel(const std::wstring& modelPath);

    bool IsModelLoaded() const noexcept { return m_modelLoaded; }

    // Processes `inputPath` and writes a BGRA PNG to `outputDir`.
    // The output filename is "<stem>_nobg_<timestamp>.png" — timestamps
    // avoid ever overwriting existing user files.
    // Returns the full output path, or std::nullopt on failure.
    std::optional<std::wstring> ProcessImage(const std::wstring& inputPath,
                                             const std::wstring& outputDir);

    // --- Pipeline stages, public for testing --------------------------------

    // Resize to 512x512, BGR->RGB, float32, normalize to [-1,1], HWC->CHW.
    // Returns a CHW float tensor packed in a 1xN Mat (N = 3*512*512).
    static cv::Mat Preprocess(const cv::Mat& bgr);

    // Runs inference; returns a cloned 512x512 CV_32FC1 mask in [0,1].
    // Falls back to GenerateSyntheticMask() when no model is loaded or
    // inference throws.
    cv::Mat RunInference(const cv::Mat& chwTensor);

    // Deterministic fallback: soft centered ellipse. Same input size ->
    // same output, which makes tests reproducible.
    static cv::Mat GenerateSyntheticMask();

    // Resize mask to `originalSize`, Gaussian-blur the edges, convert to
    // 8-bit, and merge with the original BGR image into a BGRA result.
    // Returns a cloned Mat.
    static cv::Mat Postprocess(const cv::Mat& originalBgr, const cv::Mat& mask512);

private:
    std::unique_ptr<Ort::Env> m_env;
    std::unique_ptr<Ort::Session> m_session;
    bool m_modelLoaded = false;
};

} // namespace rmbg
