# AstroCapture

[![CI](https://github.com/antonin-lfv/AstroCapture/actions/workflows/ci.yml/badge.svg)](https://github.com/antonin-lfv/AstroCapture/actions/workflows/ci.yml)

AstroCapture is a planned modern cross-platform desktop application for astronomical camera acquisition, initially focused on planetary imaging.

The project is desktop-first and local-first, with a production core in C++, a Qt/QML interface, Python for research/AI development and native model inference for shipping AI features.

## Build and test

With CMake, a C++20 compiler and Qt 6.5 or newer installed:

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev --output-on-failure
```

On macOS, `scripts/build.sh run` builds and launches the application.
The simulated camera allows development without physical hardware.

## First engineering target

The initial reference target is reliable color acquisition from a Celestron NexImage 5 on macOS, behind a camera abstraction designed for future hardware backends.

The first success criterion is not “many features”. It is a trustworthy path:

```text
camera discovery -> stream -> correct pixel interpretation -> color preview
-> exposure/gain -> recording -> capture health diagnostics
```
