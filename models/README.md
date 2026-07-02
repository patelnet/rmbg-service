# Models

This folder must contain `modnet.onnx` for real background removal.
**No model binary is committed to this repository** — the checked-in
`modnet.onnx` is a zero-byte placeholder. Until you replace it, the app
runs with a deterministic synthetic mask (a soft centered ellipse) so the
full pipeline stays testable.

## Download

1. Obtain a MODNet portrait-matting ONNX export:
   - Official MODNet repo: `<INSERT URL: https://github.com/ZHKKKe/MODNet>`
   - Pretrained ONNX export: `<INSERT DIRECT DOWNLOAD URL HERE>`
2. Rename the file to `modnet.onnx` and place it in this folder
   (replacing the placeholder).
3. Rebuild (or copy it next to the built executable as `modnet.onnx`).

Expected model I/O (validated at load time):
- Input: `1×3×512×512` float32, RGB, normalized
- Output: `1×1×512×512` float32 alpha matte

## Checksum verification (recommended)

After downloading, verify integrity:

```powershell
Get-FileHash .\modnet.onnx -Algorithm SHA256
# Compare against the published hash: <INSERT EXPECTED SHA256 HERE>
```

## License checklist — complete BEFORE redistributing

- [ ] Read the model's license (MODNet is Apache-2.0; verify for the exact
      export you downloaded).
- [ ] Confirm the license permits your use case (commercial/internal/etc.).
- [ ] Confirm redistribution terms if you plan to ship the model with an
      installer — many licenses require attribution files.
- [ ] Record the model version, source URL, and SHA256 here:
  - Version: `<fill in>`
  - Source: `<fill in>`
  - SHA256: `<fill in>`
- [ ] Check training-data/usage restrictions (e.g., face-data clauses).
- [ ] Add required attribution to your product's third-party notices.

## Swapping in a different model

Any matting model with the same 1×3×512×512 → 1×1×512×512 contract works.
For other geometries, update `kModelSize` in
`src/BackgroundRemovalService.h` and the preprocess/postprocess stages.
