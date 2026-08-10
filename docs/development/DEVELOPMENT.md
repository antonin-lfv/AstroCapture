# Development Environment & Local Workflow

## 1. Goal

A contributor must be able to work on AstroCapture without creating installers and, for most tasks, without owning astronomy hardware.

## 2. Toolchain

Baseline categories:

- modern C++ compiler supported by chosen Qt release;
- Qt 6 development environment;
- CMake;
- chosen dependency manager or vendoring strategy;
- Git;
- Python 3 for `research/` only;
- optional IDE: Qt Creator, CLion, VS Code, Visual Studio/Xcode as convenient.

The canonical build is command-line reproducible even if developers use IDE integrations.

## 3. Presets

Maintain shared presets such as conceptually:

```text
dev
release
asan          (where supported)
ci-macos
ci-windows
ci-linux
```

`CMakeUserPresets.json` is untracked and may reference local Qt paths.

Example workflow:

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev --output-on-failure
```

Do not require developers to modify committed CMake files merely to point at a local SDK path.

## 4. Dependency strategy

Choose one primary C/C++ dependency management approach early (e.g. vcpkg/Conan or carefully controlled FetchContent). Record decision in ADR.

Requirements:

- deterministic versions;
- workable on three target OSes;
- CI cache support;
- dependency license traceability;
- local override for proprietary vendor SDKs;
- no downloading arbitrary latest versions during production builds.

Vendor camera SDK binaries may need a dedicated `third_party` acquisition process because licenses can restrict redistribution.

## 5. Development application run

The normal loop is:

```text
edit -> incremental build -> run local executable -> test
```

No package, code signing or installation required.

Development runtime chooses config/log paths that do not corrupt production user state, e.g. separate application identifier/profile where practical.

## 6. Feature flags

Use compile-time or runtime development feature gates thoughtfully.

Possible development-only features:

- simulated camera;
- frame inspector;
- experimental model;
- diagnostics dashboard;
- raw packet dump.

Avoid a jungle of permanent flags. Remove stale flags after a feature graduates or is abandoned.

## 7. Mock camera

The mock backend should be usable from launch parameters or a dev menu.

Potential CLI:

```bash
AstroCapture --camera mock --mock-source bayer-test --mock-fps 120
```

Future replay:

```bash
AstroCapture --camera replay --source capture.ser --realtime
```

This is invaluable for deterministic bug reproduction.

## 8. Developer diagnostics

Provide an optional diagnostics overlay/panel showing:

- build/version;
- active backend;
- stream mode/pixel format;
- acquisition FPS;
- preview FPS;
- recorded FPS;
- queue levels;
- dropped frames by reason;
- per-stage timing;
- memory pool pressure;
- disk writer throughput;
- optional inference latency.

Do not force this visual noise on normal users.

## 9. Logging

Development default may be verbose. Production default should be concise.

Log files rotate and have bounded storage.

Support runtime category filtering, e.g. conceptually:

```bash
ASTRO_LOG="camera.*,recording=debug"
```

Exact mechanism can leverage Qt logging categories or a structured logger selected by ADR.

## 10. Sanitizers and analysis

Use where supported:

- AddressSanitizer;
- UndefinedBehaviorSanitizer;
- ThreadSanitizer selectively;
- compiler warnings at a strong level;
- clang-tidy/static analysis;
- QML linting;
- format/lint scripts.

Not every sanitizer is practical with every vendor SDK. Isolate exclusions and document them.

## 11. Python R&D environment

`research/` has an independent Python project.

Recommended approach:

```text
research/
  pyproject.toml
  src/
  notebooks/
  tests/
```

Use a lock mechanism appropriate to the team's Python toolchain.

R&D code may consume exported frame datasets and metadata from AstroCapture.

Never import random notebook code directly into production.

## 12. ML experiment lifecycle

1. define baseline and metric;
2. create versioned dataset manifest;
3. train/evaluate in Python;
4. save experiment metadata;
5. export candidate model;
6. validate ONNX/native output parity;
7. benchmark native runtime;
8. add production model artifact through controlled process.

## 13. Secrets

Local vendor SDK credentials, signing keys and API tokens must never be committed.

Provide `.env.example` or documented environment variable names only when needed.

## 14. Development database/cloud

No database is required for core MVP.

If future cloud services emerge, keep local desktop development able to run against mock/local endpoints without sharing production credentials.

## 15. Onboarding target

A new developer with prerequisites installed should be able to reach a simulated live preview using documented commands without asking a teammate for hidden steps.
