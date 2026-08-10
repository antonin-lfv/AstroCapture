# ADR-001 — C++ + Qt/QML for the desktop product

## Status
Accepted as initial direction; revalidate before first commercial release.

## Context

AstroCapture must support macOS, Windows and Linux, integrate native/vendor camera SDKs, sustain high-rate frame capture and deliver a polished desktop UI.

## Decision

Use modern C++ for the production core and Qt 6 with Qt Quick/QML for the cross-platform UI/application framework.

## Consequences

Positive:

- native integration with C/C++ camera SDKs;
- one major UI codebase across desktop platforms;
- mature desktop framework;
- high-performance native processing;
- ability to package standalone applications.

Negative:

- Qt licensing must be actively managed for proprietary distribution;
- QML/C++ bridge requires architectural discipline;
- native packaging still differs by OS;
- vendor SDK compatibility remains platform-specific.

## Alternatives considered

### SwiftUI
Excellent macOS experience but poor fit for Windows/Linux without major rewrite.

### Python + Qt
Very fast prototyping but less desirable as mandatory shipping runtime for high-throughput hardware product; packaging/native SDK integration becomes more complex.

### Rust + web/Tauri
Attractive safety/UI ecosystem, but camera vendors commonly expose C/C++ SDKs and the project would carry FFI complexity. Can be reconsidered if ecosystem needs change.

### Electron
Cross-platform UI, but unnecessary browser/runtime footprint and less direct fit for high-throughput native image/hardware application.
