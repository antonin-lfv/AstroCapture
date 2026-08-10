# AstroCapture

[![CI](https://github.com/antonin-lfv/AstroCapture/actions/workflows/ci.yml/badge.svg)](https://github.com/antonin-lfv/AstroCapture/actions/workflows/ci.yml)

AstroCapture is a planned modern cross-platform desktop application for astronomical camera acquisition, initially focused on planetary imaging.

The project is desktop-first and local-first, with a production core in C++, a Qt/QML interface, Python for research/AI development and native model inference for shipping AI features.

## Start here

Read [`AGENT.md`](AGENT.md). It is the engineering and product charter.

Key documents:

- [`docs/product/VISION.md`](docs/product/VISION.md)
- [`docs/product/ROADMAP.md`](docs/product/ROADMAP.md)
- [`docs/architecture/ARCHITECTURE.md`](docs/architecture/ARCHITECTURE.md)
- [`docs/architecture/CAMERA_AND_PIPELINE.md`](docs/architecture/CAMERA_AND_PIPELINE.md)
- [`docs/design/UX_UI.md`](docs/design/UX_UI.md)
- [`docs/development/DEVELOPMENT.md`](docs/development/DEVELOPMENT.md)
- [`docs/development/RELEASE_AND_PACKAGING.md`](docs/development/RELEASE_AND_PACKAGING.md)
- [`docs/development/QUALITY.md`](docs/development/QUALITY.md)
- [`docs/development/IMPLEMENTATION_BLUEPRINT.md`](docs/development/IMPLEMENTATION_BLUEPRINT.md)
- [`docs/design/USER_FLOWS.md`](docs/design/USER_FLOWS.md)
- [`docs/development/GOVERNANCE.md`](docs/development/GOVERNANCE.md)
- [`docs/ai/AI_STRATEGY.md`](docs/ai/AI_STRATEGY.md)

## First engineering target

The initial reference target is reliable color acquisition from a Celestron NexImage 5 on macOS, behind a camera abstraction designed for future hardware backends.

The first success criterion is not “many features”. It is a trustworthy path:

```text
camera discovery -> stream -> correct pixel interpretation -> color preview
-> exposure/gain -> recording -> capture health diagnostics
```
