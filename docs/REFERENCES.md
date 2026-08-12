# Technical references

These references were current when the initial architecture charter was authored. Revalidate versions, licenses and distribution requirements before release engineering decisions.

- Qt supported platforms: https://doc.qt.io/qt-6/supported-platforms.html
- Qt licensing: https://www.qt.io/development/qt-framework/qt-licensing
- Qt open-source licensing FAQ: https://www.qt.io/faq/qt-open-source-licensing
- CMake Presets: https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html
- ONNX Runtime C++: https://onnxruntime.ai/docs/get-started/with-cpp.html
- ONNX Runtime C/C++ APIs: https://onnxruntime.ai/docs/api/c/
- OpenCV Bayer/color conversions: https://docs.opencv.org/4.13.0/de/d25/imgproc_color_conversions.html
- Apple Developer ID: https://developer.apple.com/developer-id/
- Apple notarization: https://developer.apple.com/documentation/security/notarizing-macos-software-before-distribution
- Microsoft code signing options: https://learn.microsoft.com/en-us/windows/apps/package-and-deploy/code-signing-options
- SignTool: https://learn.microsoft.com/en-us/windows/win32/seccrypto/signtool
- GitHub Actions matrix jobs: https://docs.github.com/actions/writing-workflows/choosing-what-your-workflow-does/running-variations-of-jobs-in-a-workflow
- GitHub hosted runners: https://docs.github.com/actions/using-github-hosted-runners/about-github-hosted-runners
- oaCapture / liboacam source (EUVC driver reference for TIS/NexImage cameras): https://github.com/openastroproject/openastro/tree/master/liboacam/euvc
- The Imaging Source USB2 firmware mode-switch protocol source: https://github.com/TheImagingSource/tcam-firmware-update
- libuvc (USB Video Class library): https://github.com/libuvc/libuvc
- See also `docs/development/NEXIMAGE5_HARDWARE_NOTES.md` for what was learned applying these to the Celestron NexImage 5.
- ZWO ASICamera2 SDK, vendored for `drivers/zwo` (MIT-licensed): https://github.com/indilib/indi-3rdparty/tree/master/libasi — see `third_party/zwo-sdk/README.md` and `docs/decisions/ADR-007-zwo-asi-backend.md`.
