# Contributing

1. Read [README.md](README.md) for the user overview.
2. Build and test using the configurations in [CMakePresets.json](CMakePresets.json).
3. Prefer mock/replay camera tests for reproducibility.
4. Keep platform/vendor dependencies behind interfaces.
5. Add tests for behavior and regression fixtures where appropriate.
6. Update documentation when architecture/public behavior changes.
7. Use an ADR for costly cross-cutting decisions.
8. Never claim hardware support without physical validation evidence.

Keep completed milestones in [CHANGELOG.md](CHANGELOG.md), and include relevant
validation evidence with the pull request.

Builds, tests and installer packaging are run manually. Distribute finished
installers through [GitHub Releases](https://github.com/antonin-lfv/AstroCapture/releases).

Pull requests should explain:

- problem being solved;
- architecture impact;
- test strategy;
- platform impact;
- hardware used for validation if relevant;
- screenshots/video for UI changes where useful.

## Linux builds

The AppImage targets x86-64 Linux Mint 21/22 and Ubuntu 22.04 or newer. Use Qt 6.5+
with Qt Quick, Network, LinguistTools and **qtimageformats** (required for TIFF).
The AppImage distribution was validated with Qt 6.9.3 on Ubuntu 22.04.

On Mint/Ubuntu, install the build dependencies:

```sh
sudo apt-get install build-essential cmake ninja-build pkg-config libcfitsio-dev libraw-dev libusb-1.0-0-dev libuvc-dev libgl1-mesa-dev libfontconfig1 libdbus-1-3 desktop-file-utils python3 file
```

Install a suitable Qt SDK and put its `bin` directory on `PATH`, then run:

```sh
cmake --preset release-linux
cmake --build --preset release-linux --parallel 2
ctest --preset release-linux
scripts/package_linux.sh --skip-build
```

The AppImage and ready-to-extract `AstroCapture-<version>-Linux-x86_64.zip` are created in `build/release-linux`. The ZIP includes bilingual installation instructions, optional camera access rules and the AppImage SHA-256 checksum. Python 3 is required for archive verification. Packaging tools are downloaded
from pinned releases and verified with SHA-256. Check the installed AppImage over
X11 with its own QML and image plugins, including TIFF:

```sh
APPIMAGE_EXTRACT_AND_RUN=1 ./build/release-linux/AstroCapture-*-x86_64.AppImage --check-installation
```

Camera drivers are compiled; physical capture must be validated separately on
Linux hardware.
