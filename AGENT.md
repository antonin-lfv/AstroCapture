# AstroCapture — Engineering & Product Charter

> **Status:** Foundational document / source of truth for contributors and coding agents  
> **Scope:** Desktop astrophotography acquisition software, initially planetary imaging  
> **Targets:** macOS, Windows, Linux  
> **Primary stack:** C++ + Qt 6/QML, CMake, OpenCV; Python for R&D; ONNX Runtime for production AI inference  
> **Distribution model:** downloadable native desktop application; cloud services optional and non-blocking

---

## 0. Purpose of this document

This file is the mandatory entry point for every developer, reviewer, coding agent, designer, QA engineer, ML engineer, or future technical contributor working on AstroCapture.

It does **not** attempt to duplicate every specification. It defines the non-negotiable product and engineering principles, the architecture boundaries, the release philosophy, the current scope, and the order in which the product must evolve. Detailed specifications live under `docs/`.

Before making a meaningful architectural change, contributors must read:

- `docs/product/VISION.md`
- `docs/product/ROADMAP.md`
- `docs/architecture/ARCHITECTURE.md`
- `docs/architecture/CAMERA_AND_PIPELINE.md`
- `docs/design/UX_UI.md`
- `docs/development/DEVELOPMENT.md`
- `docs/development/RELEASE_AND_PACKAGING.md`
- `docs/development/QUALITY.md`
- `docs/ai/AI_STRATEGY.md` when touching AI/automation
- relevant ADRs in `docs/decisions/`

The project must remain understandable to a developer joining years later. Architectural intent is part of the product and must be preserved in documentation, tests, code boundaries and reviews.

---

# 1. Product mission

AstroCapture aims to become a modern, reliable, enjoyable desktop application for astronomical image acquisition and eventually intelligent processing, with first-class support for macOS and equally serious support for Windows and Linux.

The initial user pain is simple: a user connects a planetary astronomy camera and wants to see a responsive live image, in the correct color, tune the relevant camera parameters and record high-quality raw data without fighting the software.

The long-term ambition is larger: AstroCapture should progressively assist users with acquisition, focusing, frame quality assessment, planetary tracking, recommended camera settings, automatic ROI management, stacking and post-processing while preserving expert control.

The project must avoid two failure modes:

1. building a huge astrophotography suite before basic acquisition is trustworthy;
2. building a disposable single-camera prototype that must later be rewritten to support a commercial product.

Therefore AstroCapture is designed as a modular product from day one, but implemented incrementally.

---

# 2. Product principles

## 2.1 Desktop-first and local-first

AstroCapture is a native desktop application. Core acquisition and processing must not depend on an Internet connection.

The application must be able to:

- enumerate locally connected cameras;
- stream frames at camera-native rates;
- preview frames;
- control supported camera parameters;
- record files locally;
- perform core processing locally;
- perform production AI inference locally when feasible.

Optional cloud services may later provide accounts, license management, synchronization, downloadable models, presets, documentation, telemetry with consent, galleries, collaboration or marketplace functions. Loss of Internet access must never make local imaging impossible unless a future commercial licensing model explicitly requires periodic entitlement checks, in which case an offline grace period is mandatory.

## 2.2 Reliability before feature count

A short feature list that never drops frames unexpectedly is better than a long list of unreliable controls.

The acquisition path is sacred. UI animation, analytics, AI, previews and background work must never block or destabilize camera acquisition.

## 2.3 Progressive complexity

The default interface must expose the controls needed by most users. Advanced controls remain discoverable but must not dominate the first experience.

Beginners should understand the current state of the application. Advanced users must retain access to raw formats, precise controls, diagnostics and automation overrides.

## 2.4 Expert control is never removed by automation

Automation must be explainable, optional and reversible.

Examples:

- Auto exposure may recommend or apply values, but manual exposure remains available.
- AI frame scoring may rank frames, but users must be able to choose classical metrics or disable AI scoring.
- Automatic ROI may suggest a crop, but users may override it.

## 2.5 Cross-platform does not mean lowest-common-denominator

Core product behavior must be consistent across macOS, Windows and Linux, while platform-native conventions are respected where appropriate.

The codebase should share the vast majority of business logic, acquisition abstractions, processing and UI structure. Platform-specific code must be isolated behind explicit interfaces.

## 2.6 No premature AI

Do not use ML where deterministic image processing is simpler, faster, easier to explain and sufficiently accurate.

Each AI feature must answer:

- What problem does the model solve?
- What baseline does it beat?
- How is quality measured?
- What is its runtime and memory cost?
- What hardware does it require?
- What happens when the model fails?
- Can the user understand or override the result?

---

# 3. Scope by product phase

## Phase 0 — Technical proof

Goal: prove reliable acquisition from the initial target camera on macOS and understand its raw/color path.

Required outcomes:

- enumerate the camera;
- open a stream;
- inspect frame formats and metadata;
- identify Bayer pattern or decoded format correctly;
- display a responsive color preview;
- expose basic exposure/gain controls where supported;
- record sample data;
- instrument dropped frames, throughput and latency.

The first reference device is the Celestron NexImage 5 used by the project originator. This device is an initial validation target, not a permanent architectural special case.

## Phase 1 — Acquisition MVP

Goal: a usable desktop application for planetary capture.

Minimum feature family:

- camera discovery and connection;
- live preview;
- debayer/color pipeline;
- exposure and gain;
- camera format/resolution selection;
- ROI where hardware/backend permits;
- histogram;
- recording to a suitable astronomy format, with SER prioritized;
- still image capture where appropriate;
- session metadata;
- basic white-balance support where the camera provides it;
- capture duration/frame-count limits;
- recording status, FPS, dropped-frame count and disk-throughput health;
- robust error handling and camera reconnect behavior.

## Phase 2 — Professional acquisition tools

Potential features:

- focus assistance;
- configurable quality metrics;
- overlays;
- planet centering and tracking;
- capture profiles/presets;
- sequence planning;
- auto ROI;
- richer camera support;
- advanced histogram/clipping guidance;
- metadata/session organization;
- optional mount/focuser abstractions where justified.

## Phase 3 — Intelligent acquisition

Potential features:

- automatic object recognition;
- auto-exposure recommendation;
- saturation-aware gain optimization;
- seeing estimation;
- frame quality scoring;
- tracking assistance;
- intelligent focus guidance;
- acquisition recipe recommendation;
- anomaly detection for clouds, severe motion, connection problems or unusable sequences.

## Phase 4 — Processing

Potential features:

- frame selection;
- registration/alignment;
- stacking;
- live stacking where technically valuable;
- wavelets/sharpening;
- denoising;
- color balance;
- derotation integrations or workflows if justified;
- batch processing;
- reproducible processing recipes.

## Phase 5 — Ecosystem

Potential features, only after product-market validation:

- plugin SDK;
- third-party camera drivers;
- downloadable processing modules/models;
- cloud preset synchronization;
- community profiles;
- remote-control API;
- marketplace or extension catalogue.

The roadmap is directional. Do not promise version numbers or dates solely because they appear in brainstorming. See `docs/product/ROADMAP.md` for gates.

---

# 4. Architecture at a glance

The intended high-level architecture is:

```text
┌──────────────────────────────────────────────────────────────┐
│                       AstroCapture UI                        │
│                         Qt Quick / QML                       │
├──────────────────────────────────────────────────────────────┤
│                 Presentation / Application Layer             │
│      ViewModels / Commands / Session orchestration           │
├──────────────────────────────────────────────────────────────┤
│                         Domain/Core                          │
│ Camera API │ Frame types │ Capture session │ Processing API  │
├──────────────────┬──────────────────┬────────────────────────┤
│ Camera backends  │ Image pipeline   │ Recording / storage    │
│ UVC / vendor SDK │ debayer, hist.   │ SER, metadata, images  │
├──────────────────┴──────────────────┴────────────────────────┤
│                  Platform integration layer                  │
│             macOS │ Windows │ Linux specific code            │
├──────────────────────────────────────────────────────────────┤
│      Optional production inference: ONNX Runtime / native    │
└──────────────────────────────────────────────────────────────┘

R&D sidecar / repository tools:
Python + NumPy + OpenCV + PyTorch/Jupyter for experimentation,
training, benchmarking and dataset work. Production models are exported.
```

Core boundaries:

- QML must never speak directly to a vendor SDK.
- Camera drivers must never depend on QML.
- Domain camera abstractions must not expose vendor-specific implementation details unless wrapped in an extensible capability system.
- Recording must consume frames independently from preview rendering.
- Preview rendering must be allowed to drop/skip visual frames without implying recording loss.
- Recording must never depend on the display refresh rate.
- AI must not sit synchronously on the acquisition-critical path unless a future feature explicitly requires it and can prove bounded latency.

See `docs/architecture/ARCHITECTURE.md` and `docs/architecture/CAMERA_AND_PIPELINE.md`.

---

# 5. Technology direction

## 5.1 C++

The production core is implemented in modern C++.

Default language level: C++20 unless a dependency or compiler support decision documented in an ADR changes this. Use newer language features selectively when all supported toolchains are validated.

C++ owns:

- camera abstractions and native SDK bridges;
- frame buffers and memory-sensitive code;
- acquisition orchestration;
- performance-critical image processing;
- recording;
- production inference integration;
- cross-platform domain logic.

## 5.2 Qt 6 / QML

Qt is the cross-platform application framework. Qt Quick/QML is the preferred user-interface technology.

The UI should feel premium and modern, but the project deliberately avoids rigid pixel-by-pixel design rules in this charter. Design quality is specified in terms of hierarchy, consistency, behavior, accessibility, readability and interaction, not arbitrary fixed dimensions.

## 5.3 OpenCV

OpenCV may be used for validated image-processing primitives such as color conversion, debayering, filtering, feature extraction, transforms and prototyping of algorithms.

Do not allow OpenCV's data structures to leak everywhere through the domain API. Prefer project-owned frame/image abstractions at subsystem boundaries.

## 5.4 Python

Python is a first-class **development and R&D language**, not the mandatory runtime of the shipping desktop application.

Python is appropriate for:

- notebooks and exploratory analysis;
- evaluation of quality metrics;
- model training;
- dataset tooling;
- benchmarking algorithms;
- rapid prototypes;
- offline conversion/export scripts;
- comparison against the native implementation.

A temporary local Python sidecar is acceptable for experimental features behind development flags. It must not become an invisible permanent dependency without an explicit architectural decision.

## 5.5 ONNX Runtime

Preferred production path for compatible ML models:

```text
Python/PyTorch training -> validated model -> ONNX export -> native inference
```

Native inference enables packaging without asking users to install Python environments.

## 5.6 CMake

CMake is the canonical build system. Shared presets are stored in `CMakePresets.json`; developer-specific overrides belong in `CMakeUserPresets.json` and must not be committed.

Build commands must be scriptable and CI-compatible. The project must never require a specific IDE to build.

---

# 6. Repository layout

Target layout:

```text
AstroCapture/
├── AGENT.md
├── README.md
├── LICENSE
├── CHANGELOG.md
├── CONTRIBUTING.md
├── CMakeLists.txt
├── CMakePresets.json
├── cmake/
├── app/
│   ├── main.cpp
│   ├── application/
│   └── resources/
├── core/
│   ├── camera/
│   ├── capture/
│   ├── frame/
│   ├── processing/
│   ├── recording/
│   ├── session/
│   └── diagnostics/
├── drivers/
│   ├── mock/
│   ├── uvc/
│   ├── celestron_or_legacy/
│   ├── zwo/
│   ├── qhy/
│   └── playerone/
├── platform/
│   ├── macos/
│   ├── windows/
│   └── linux/
├── ui/
│   ├── qml/
│   ├── components/
│   ├── screens/
│   ├── panels/
│   └── theme/
├── inference/
│   ├── runtime/
│   └── models/
├── research/
│   ├── pyproject.toml
│   ├── notebooks/
│   ├── datasets/
│   ├── experiments/
│   └── export/
├── tests/
│   ├── unit/
│   ├── integration/
│   ├── camera/
│   ├── golden/
│   └── performance/
├── tools/
│   ├── frame_inspector/
│   ├── camera_probe/
│   └── dataset_tools/
├── packaging/
│   ├── macos/
│   ├── windows/
│   └── linux/
├── scripts/
├── docs/
└── .github/workflows/
```

This layout is a target, not a reason to create empty directories prematurely. Create a directory when the subsystem exists or when it materially clarifies ownership.

---

# 7. Development vs production modes

There are not two divergent codebases.

There is one product with different build configurations and runtime capabilities.

## 7.1 Development mode

A developer must be able to clone, configure, build and run AstroCapture locally **without producing an installer/package**.

Expected workflow:

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
./path/to/AstroCapture
```

Platform helper scripts may simplify this further:

```bash
./scripts/dev-run.sh
```

or on Windows:

```powershell
./scripts/dev-run.ps1
```

Development mode may expose:

- verbose logging;
- camera/backend details;
- live FPS counters;
- acquisition queue depth;
- dropped-frame counters;
- frame timestamps;
- memory pool stats;
- processing timings;
- recording throughput;
- simulated cameras;
- prerecorded frame sources;
- synthetic Bayer test patterns;
- debug overlays;
- optional experimental features;
- local model hot reload where safe;
- developer tools not intended for end users.

Developer diagnostics must be either hidden or disabled in production by default.

## 7.2 Mock/simulated camera

A mock camera backend is mandatory early in the project.

It must allow frontend, recording and pipeline work without physical hardware. Sources should eventually include:

- generated color bars/test patterns;
- generated Bayer patterns;
- static images replayed as frames;
- SER/file replay;
- configurable frame rate;
- deliberate dropped/corrupted frame scenarios;
- changing exposure metadata;
- disconnection/reconnection simulation.

A robust mock backend is one of the highest-leverage investments in the project.

## 7.3 Production mode

Production builds must:

- disable development-only controls by default;
- contain version/build metadata;
- include only approved resources/models;
- use optimized compiler settings appropriate for release;
- pass the production test suite;
- package runtime dependencies correctly;
- include required legal/license notices;
- be code-signed where the platform requires or strongly expects it;
- be notarized on macOS for direct distribution outside the Mac App Store;
- be packaged into user-friendly installers/images;
- be reproducible from a tagged source revision as far as practical.

See `docs/development/RELEASE_AND_PACKAGING.md`.

---

# 8. Camera subsystem rules

The camera abstraction must model **capabilities**, not pretend every camera has identical controls.

Avoid designs such as:

```cpp
camera.setGain(...); // assumed universally available
```

Prefer discovery:

```text
CameraDevice
  - identity
  - stream modes
  - capabilities
      - Exposure
      - Gain
      - WhiteBalance
      - Gamma
      - HardwareROI
      - Temperature
      - Cooler
      - Trigger
      - etc.
```

Each control requires metadata where meaningful:

- supported/not supported;
- read-only/read-write;
- min/max;
- step;
- default;
- current value;
- auto/manual modes;
- units;
- integer/float/enum/bool type;
- whether changes are safe while streaming;
- whether restart is required.

Do not encode UI labels into the driver core. Drivers expose semantic capabilities; presentation decides labels.

Frame format descriptors must distinguish:

- dimensions;
- bit depth;
- packed/unpacked representation;
- monochrome vs Bayer vs RGB/BGR/YUV or other formats;
- Bayer pattern when applicable;
- endianness when applicable;
- stride;
- timestamp source;
- camera sequence number if available.

Every backend must document ownership/lifetime rules for SDK buffers.

---

# 9. Frame pipeline rules

The system distinguishes at least three conceptual paths:

1. **Acquisition path** — obtains camera frames with minimum latency and predictable memory behavior.
2. **Recording path** — preserves requested raw/scientific data and metadata.
3. **Preview/analysis path** — transforms data for human display and non-critical analysis.

These paths may share buffers through safe ownership mechanisms, but must not create hidden synchronous dependencies.

Preferred conceptual flow:

```text
Camera backend
    ↓
Acquisition worker
    ↓
Bounded frame transport / pool
    ├──→ Recorder queue → disk
    ├──→ Preview sampler → debayer/display
    ├──→ Histogram/metrics
    └──→ Optional analysis/AI
```

Rules:

- Queues are bounded.
- Overflow policy is explicit.
- Preview may discard superseded frames.
- Recording overflow must be visible and treated as a capture-health event.
- No unbounded accumulation of frames in RAM.
- Avoid per-frame heap allocations in high-rate loops where practical.
- Preserve original raw data when recording raw formats.
- Debayering for preview must not silently alter recorded raw data.
- Timestamps must be propagated.
- Performance counters are designed in, not bolted on later.

---

# 10. UI/UX direction

AstroCapture should feel like a contemporary professional creative/scientific application, not a hardware test utility.

The UI must communicate three things instantly:

1. what camera/device is active;
2. what the user is seeing;
3. whether a capture is safe/healthy/running.

## 10.1 Desired personality

- premium;
- calm;
- technical without being intimidating;
- dark-environment friendly;
- visually focused on the astronomical image;
- information-dense only where valuable;
- fast and tactile;
- consistent;
- unobtrusive during capture.

## 10.2 No rigid pixel specification

Do **not** turn design guidelines into arbitrary fixed pixel commandments.

Do specify:

- visual hierarchy;
- relative importance;
- alignment and rhythm;
- minimum readability/accessibility constraints;
- consistent component behavior;
- responsive layout priorities;
- interaction states;
- error/warning semantics;
- keyboard behavior;
- density modes if later needed.

The design system should enable iteration rather than freeze an early mockup.

## 10.3 Main workspace concept

The default workspace should prioritize the live image.

A plausible structure:

```text
┌───────────────────────────────────────────────────────────────────┐
│ Toolbar / session / camera / capture state                       │
├──────────────┬──────────────────────────────────┬─────────────────┤
│ Navigation   │                                  │ Camera controls │
│ / tools      │          LIVE VIEW               │ / contextual   │
│              │                                  │ inspector      │
│              │                                  │                │
├──────────────┴──────────────────────────────────┴─────────────────┤
│ Context panel: histogram / timeline / capture metrics / logs      │
└───────────────────────────────────────────────────────────────────┘
```

This is a compositional direction, not a fixed layout mandate.

## 10.4 Capture state must be unmistakable

Idle, streaming and recording must be visually distinguishable without relying on color alone.

During recording show meaningful status such as:

- elapsed time;
- captured frame count;
- effective FPS;
- dropped frames;
- output file/session;
- disk health/throughput warning where relevant;
- remaining configured duration/frames if finite.

Do not flood the user with diagnostics unless there is a problem or they explicitly request advanced telemetry.

## 10.5 Progressive disclosure

Common camera controls first. Rare controls under an advanced section or contextual inspector.

Do not expose every vendor SDK property in a flat wall of sliders.

## 10.6 Empty states

No camera connected is a designed state, not an error screen.

The application should explain:

- no camera detected;
- how to connect/rescan;
- unsupported device/backend information when known;
- how to use a simulated camera in development builds;
- links to troubleshooting/help when appropriate.

## 10.7 Errors

Errors must be actionable.

Bad:

> Error -19.

Better:

> Camera stream stopped. The device was disconnected. Reconnect the camera and choose “Reconnect”.

Technical details may be expandable/copyable for support.

See `docs/design/UX_UI.md`.

---

# 11. AI and automation rules

AI features live outside the MVP critical path.

Production AI architecture should generally be:

```text
Dataset / research
      ↓
Python training + evaluation
      ↓
Versioned model artifact
      ↓
Export / compatibility tests
      ↓
ONNX Runtime/native inference
      ↓
Feature API
      ↓
UI recommendation or automation
```

Each model must have a model card including:

- intended use;
- out-of-scope use;
- training/validation data description;
- supported objects/cameras if restricted;
- input preprocessing;
- output meaning;
- metrics;
- known failure cases;
- model version;
- compatible app versions;
- license/provenance;
- runtime benchmarks.

AI output must not masquerade as certainty. Confidence and fallback behavior must be considered where relevant.

See `docs/ai/AI_STRATEGY.md`.

---

# 12. Performance and concurrency principles

Do not define unrealistic universal FPS promises detached from camera, USB, storage and machine constraints.

Instead define measurable internal service-level objectives and benchmark them on reference hardware.

Key measurements:

- camera input FPS;
- frames received;
- frames recorded;
- preview FPS;
- dropped-frame count by cause;
- frame queue occupancy;
- capture-to-preview latency;
- bytes/sec from camera;
- bytes/sec to disk;
- debayer time/frame;
- histogram time/frame;
- inference time/frame;
- memory use and allocation rate.

Concurrency model should favor:

- dedicated acquisition execution context/thread where required by SDK;
- asynchronous recording;
- decoupled processing workers;
- UI thread used only for UI-facing work;
- bounded queues/backpressure;
- explicit shutdown/cancellation.

Do not add threads casually. Every thread/task has ownership, lifecycle and shutdown semantics.

---

# 13. Data integrity and astronomy formats

For scientific/planetary workflows, distinguish **raw capture data** from **display data**.

A preview may be debayered, stretched, gamma-adjusted or white-balanced. These display operations must never silently overwrite raw acquisition intent.

Recording metadata should eventually include, when available and appropriate:

- application version;
- camera manufacturer/model/id;
- selected stream mode;
- pixel format;
- dimensions;
- bit depth;
- Bayer pattern;
- exposure;
- gain;
- white balance/control values;
- timestamps;
- frame counts and dropped-frame indicators;
- capture start/end time;
- optional object/session notes;
- processing/display settings separately from raw camera settings.

SER support is a priority for planetary imaging. Exact format implementation must be validated against the specification and reference tools before production release.

---

# 14. Testing strategy

No physical camera may be required for the majority of CI tests.

Test layers:

## Unit tests

- frame descriptors;
- capability validation;
- buffer ownership utilities;
- color conversion wrappers;
- histogram functions;
- recorder metadata;
- state machines;
- settings validation.

## Golden image tests

Known Bayer inputs and expected decoded outputs.

Test all supported Bayer patterns and relevant bit depths/packing strategies.

## Integration tests

- mock camera -> acquisition -> recorder;
- mock camera -> preview pipeline;
- disconnection/reconnection;
- bounded queue behavior;
- disk failure simulation where practical;
- malformed frame handling.

## Hardware tests

A curated camera matrix runs manually or on dedicated hardware rigs before releases.

Each supported camera must have a compatibility record.

## Performance tests

Use deterministic synthetic streams to detect regressions in:

- frame throughput;
- allocations;
- debayer time;
- recording throughput;
- latency.

Performance tests should produce trends; not every microbenchmark should block PRs initially.

## UI tests

Cover critical workflows, not every visual pixel.

Prioritize:

- no camera state;
- connect;
- start/stop stream;
- control changes;
- start/stop recording;
- error states;
- persistence of settings where expected.

---

# 15. Logging and diagnostics

Use structured logging categories such as:

- `app`
- `camera.discovery`
- `camera.stream`
- `camera.control`
- `pipeline`
- `recording`
- `rendering`
- `ai`
- `platform`
- `update`

Logs must avoid leaking secrets, license tokens or unnecessary personal file information.

A support bundle feature is desirable later. It may collect:

- app/build version;
- OS version;
- backend versions;
- detected camera metadata;
- non-sensitive relevant settings;
- recent logs;
- performance diagnostics.

Collection and submission must be transparent.

---

# 16. Configuration and persistence

Separate:

- global application preferences;
- UI/workspace preferences;
- per-camera settings;
- capture profiles;
- session-specific overrides.

Configuration schemas must be versionable/migratable. Do not assume today's JSON shape can remain forever.

Never store passwords/tokens in plain-text configuration when secure OS credential stores are available.

---

# 17. Packaging and releases

AstroCapture is distributed as a downloadable application.

Expected production artifacts:

- macOS: signed/notarized application, typically delivered in DMG or another appropriate user-friendly package;
- Windows: signed installer/package appropriate to the selected distribution strategy;
- Linux: one or more practical formats, likely AppImage/Flatpak/deb/rpm depending on validated audience needs; avoid supporting every format prematurely.

Release flow concept:

```text
PR -> CI -> main -> release candidate -> hardware validation
   -> version tag -> clean builds per OS -> tests
   -> signing/notarization -> package smoke tests
   -> publication -> update metadata
```

Do not commit secrets or signing credentials.

CI should use protected environments/secrets and least privilege.

See `docs/development/RELEASE_AND_PACKAGING.md`.

---

# 18. Dependency and licensing discipline

AstroCapture may become proprietary/commercial. Dependency licensing is therefore an architectural concern.

Before adding a dependency, record:

- package/project name;
- version policy;
- license;
- static vs dynamic linking implications;
- transitive dependencies;
- supported OS/architectures;
- maintenance activity;
- binary distribution impact;
- security/update story;
- reason it is needed.

Qt licensing strategy must be reviewed before commercial distribution. Do not make assumptions that “open source” means “no obligations”.

Maintain a machine-readable dependency inventory/SBOM when release maturity warrants it.

---

# 19. Security principles

Desktop camera software is not a high-risk cloud service, but commercial software still needs discipline.

Rules:

- validate untrusted files before parsing;
- assume camera SDKs and devices can fail unexpectedly;
- avoid arbitrary code execution through plugins unless a secure plugin model is intentionally designed;
- verify downloaded updates/models;
- use TLS for network services;
- sign release artifacts;
- store credentials in OS-appropriate secure storage;
- avoid collecting telemetry by default without clear product/legal decisions;
- keep third-party dependencies updated under a controlled process;
- use code review for update/licensing/networking/security-sensitive code.

---

# 20. Source control and change management

Recommended branch model:

- `main` is always releasable or close to releasable;
- feature/fix branches are short-lived;
- pull requests required for meaningful changes;
- protected main branch;
- CI required before merge;
- architectural changes require an ADR.

Commit messages should be understandable and scoped. Conventional Commits may be adopted if the team values automated changelog/release tooling, but the useful information matters more than ritual.

A PR touching a subsystem should include relevant tests and documentation changes.

---

# 21. Definition of Done

A feature is not done because it “works on the author's machine”.

For a normal production feature, Done generally means:

- acceptance behavior implemented;
- architecture boundaries respected;
- error paths considered;
- tests added/updated;
- no known serious performance regression;
- UI states complete enough for success/loading/error/disabled cases;
- relevant documentation updated;
- works in development mode;
- does not break production packaging assumptions;
- cross-platform impact reviewed;
- telemetry/logging/support diagnostics considered where useful;
- no new unresolved dependency/license issue.

For camera support, Done additionally requires a compatibility checklist and real-device validation before claiming official support.

---

# 22. What not to build yet

Unless the roadmap explicitly promotes them, do not distract the MVP with:

- user accounts;
- cloud galleries;
- marketplace;
- remote observatory orchestration;
- complete mount control suite;
- a full Photoshop-like editor;
- social networking;
- dozens of half-tested camera SDKs;
- generative AI features unrelated to acquisition quality;
- automatic stacking before raw acquisition is trustworthy.

Design extension points, but build only what validates the next product assumption.

---

# 23. Initial development sequence

The recommended first engineering sequence is:

1. Repository/build skeleton.
2. Qt/QML shell with a high-quality empty state.
3. Core frame types and camera abstraction.
4. Mock camera backend.
5. Camera diagnostic/probe utility.
6. NexImage/macOS acquisition spike isolated behind a backend.
7. Raw frame inspection and format identification.
8. Debayer/color correctness tests.
9. Live preview with decoupled acquisition/rendering.
10. Exposure/gain capability controls.
11. Recording pipeline and SER implementation/validation.
12. Histogram and capture health metrics.
13. Reconnect/error recovery.
14. CI across macOS/Windows/Linux even before all physical camera backends are available there.
15. First packaged alpha for macOS.
16. Extend hardware/backend support deliberately.

At every step, keep mock-device workflows functional.

---

# 24. ADR policy

Create an Architecture Decision Record when a decision is:

- expensive to reverse;
- cross-cutting;
- likely to be questioned later;
- dependent on important trade-offs;
- licensing/security/distribution sensitive.

ADR template:

```md
# ADR-NNN — Title

## Status
Proposed | Accepted | Superseded | Rejected

## Context
What problem are we deciding?

## Decision
What are we doing?

## Consequences
Positive and negative consequences.

## Alternatives considered
What else was evaluated and why not chosen?
```

Initial ADRs are provided under `docs/decisions/`.

---

# 25. Rules for coding agents and automated contributors

Automated coding tools must:

1. read this file before broad changes;
2. inspect relevant subsystem documentation;
3. avoid inventing vendor SDK behavior;
4. avoid creating platform-specific coupling in domain code;
5. add tests for parsing, image formats and state-machine logic;
6. avoid replacing a bounded concurrent design with convenient unbounded queues;
7. preserve raw-vs-preview semantics;
8. avoid introducing Python as an end-user prerequisite without an ADR;
9. update docs when public interfaces or architecture change;
10. not claim hardware support without real validation evidence;
11. prefer small coherent patches over unrelated refactors;
12. mark assumptions clearly if hardware documentation is missing.

If a request conflicts with this charter, the contributor should flag the conflict and either update the architecture through an explicit decision or preserve the charter.

---

# 26. Quality bar

The desired user reaction is not merely “it works”. It should be closer to:

- connection feels immediate;
- controls feel predictable;
- recording feels trustworthy;
- the live image remains the focus;
- advanced functionality is available without visual clutter;
- the application behaves like a polished commercial product rather than a developer tool.

The desired developer reaction is:

- architecture boundaries are obvious;
- local setup is straightforward;
- tests can run without specialist hardware;
- hardware-specific complexity is contained;
- performance can be measured;
- releases are reproducible and documented;
- long-term features have extension points without polluting current code.

---

# 27. Reference documentation

Official references that motivated key choices and should be revalidated periodically:

- Qt supported platforms: https://doc.qt.io/qt-6/supported-platforms.html
- Qt licensing: https://www.qt.io/development/qt-framework/qt-licensing
- CMake presets: https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html
- ONNX Runtime C++: https://onnxruntime.ai/docs/get-started/with-cpp.html
- OpenCV color/Bayer conversions: https://docs.opencv.org/
- Apple Developer ID / notarization: https://developer.apple.com/developer-id/
- Microsoft code signing guidance: https://learn.microsoft.com/windows/apps/package-and-deploy/code-signing-options
- GitHub Actions matrix builds: https://docs.github.com/actions/writing-workflows/choosing-what-your-workflow-does/running-variations-of-jobs-in-a-workflow

These links are references, not eternal constraints. Reassess versions and platform requirements during release planning.
