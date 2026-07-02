# Changelog

All notable changes to this project are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); versions follow
[SemVer](https://semver.org/).

## [1.0.0] - 2026-07-02

### Added
- Background-removal pipeline (`BackgroundRemovalService`): OpenCV
  preprocess (512×512, RGB, CHW float32), ONNX Runtime inference
  (MODNet-style 1×3×512×512 → 1×1×512×512), postprocess to BGRA PNG with
  soft alpha edges.
- Deterministic synthetic-mask fallback when no model is present, keeping
  the full pipeline testable without shipping model binaries.
- Directory watching via overlapped `ReadDirectoryChangesW`
  (`DirectoryWatcher`) with safe stop semantics.
- WinUI 3 (Windows App SDK 2.2.0) app: NavigationView shell, HomePage
  (start/stop, status, log, drag-and-drop), SettingsPage.
- System tray icon with Start/Stop/Open Output Folder/Exit menu, tooltip
  status, and balloon notifications (`TrayController`).
- JSON settings persisted to `%LOCALAPPDATA%\BackgroundRemover\settings.json`
  (`Settings`), loaded at startup and saved on change.
- Safe output naming: `<name>_nobg_<timestamp>.png`, never overwrites.
- Build tooling: vcpkg manifest (onnxruntime 1.23.2, opencv 4.12.0,
  nlohmann-json 3.12.0, vcpkg 2026.06.24), CMake core + console test,
  `build.ps1` bootstrap script.
- WiX 5.0.2 per-user MSI installer (no elevation, Start Menu shortcut,
  Add/Remove Programs entry, major-upgrade support).
- GitHub Actions CI: pinned-vcpkg build, synthetic-fallback smoke test,
  MSI packaging, artifact upload, optional code signing.

### Notes
- No third-party model binaries are included; see `models/README.md` for
  download and license guidance.
- The MSI packages the console test binary as a stand-in executable; the
  packaged WinUI app is built from Visual Studio (see README).

[1.0.0]: https://github.com/patelnet/rmbg-service/releases/tag/v1.0.0
