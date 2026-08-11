# Product & Technical Roadmap

This roadmap uses **gates**, not calendar promises. A phase advances only when the previous layer is reliable enough.

## Gate A — Buildable application skeleton

Exit criteria:

- project builds through CMake on primary development machine;
- Qt/QML shell launches;
- development preset documented;
- CI skeleton exists for macOS, Windows and Linux;
- mock camera interface defined;
- no installer required for local development.

## Gate B — Frame model + mock acquisition

Exit criteria:

- canonical `Frame`/`FrameDescriptor` concepts exist;
- bounded acquisition transport is implemented;
- mock camera streams deterministic frames;
- preview can display simulated frames;
- acquisition statistics visible in dev diagnostics;
- lifecycle start/stop/restart tests pass.

## Gate C — Initial real camera on macOS

Reference validation: Celestron NexImage 5.

Exit criteria:

- device can be discovered/opened;
- supported stream modes can be enumerated or safely configured;
- frame pixel format understood;
- color correctness demonstrated with Bayer test + real camera evidence;
- exposure/gain available if backend exposes them;
- unplug/replug does not require application restart under normal recoverable conditions;
- diagnostics capture backend errors.

Important: this gate may involve a research spike. Do not contaminate the general camera API with undocumented one-off hacks without an ADR.

## Gate D — Acquisition MVP

Exit criteria:

- polished camera selector/empty state (a picker now appears when more than one real camera is found, plus a Disconnect action; still not "polished" in the visual-design sense the phrase implies);
- stable live preview;
- common camera controls;
- histogram;
- still image capture: save the currently previewed frame to disk (PNG, or TIFF/FITS — see the feature-parity backlog below);
- video recording: record the live stream to a suitable astronomy format — SER is implemented and the default (`astro::recording::SerRecorder`), with the interim `ACRAWSEQ` recorder kept as a selectable fallback; see IMPLEMENTATION_BLUEPRINT.md section 22.9 for what validation was and wasn't possible without a Windows/AutoStakkert/PIPP/Registax environment to cross-validate against;
- capture duration/frame targets (frame-count and/or duration auto-stop);
- clear recording status;
- no silent dropped frames;
- file/session metadata (an optional target name is threaded into saved filenames; no observer/notes fields or a session concept beyond that yet);
- basic settings persistence;
- smoke-tested production package on macOS.

### Feature-parity backlog — completed

Gaps against reference planetary-capture software (FireCapture, SharpCap,
ASICap), found by comparing against their common feature sets rather than
derived from any specific hardware test. All nine below are implemented,
each testable and tested against the mock camera alone (none needed Gate
C). See IMPLEMENTATION_BLUEPRINT.md section 22 for the plan/detail behind
each and, for SER specifically, section 22.9's note on what could and
couldn't be validated without a Windows/AutoStakkert/PIPP/Registax
environment.

1. ROI/crop capture — a sub-region of the sensor can be streamed instead
   of the full frame.
2. Preview overlays — zoom/loupe (pixel peeping), center crosshair,
   histogram over/under-exposure (clipping) indicators.
3. Additional camera controls — gamma, white balance (R/B gain),
   brightness/offset, frame rate limit, alongside exposure/gain.
4. Advanced capture statistics — live min/max/mean pixel value; disk-write
   fps shown next to capture fps in the Recording section.
5. Real (bilinear) debayer for the live preview, replacing the former
   nearest-neighbor placeholder.
6. Per-frame timestamps in recordings, recoverable as absolute UTC time
   for derotation tools such as WinJUPOS.
7. Keyboard shortcuts (C/R) for capture/record start-stop.
8. Additional still-image save formats — TIFF16 and FITS, alongside PNG.
9. SER recording format — the actual target format, now the default
   recording format (ACRAWSEQ remains selectable).

## Gate E — Cross-platform productization

Exit criteria:

- application shell and mock pipeline validated on Windows/Linux;
- at least one real camera/backend validated on each supported platform before claiming official support;
- platform-specific packaging pipeline implemented;
- signing/notarization strategy documented;
- crash diagnostics/support workflow established;
- dependency/license inventory reviewed.

## Gate F — Multi-camera ecosystem

Prioritize cameras based on user demand and vendor SDK quality, not brand count.

For each new backend:

- documented SDK/license/distribution rights;
- device enumeration;
- capability mapping;
- format mapping;
- lifecycle/error behavior;
- hardware test matrix;
- support status classified as Experimental/Beta/Supported.

## Gate G — Acquisition assistance

Candidate features:

- focus score;
- histogram clipping guidance;
- automatic planet centering;
- ROI recommendation;
- capture presets;
- capture sequences;
- seeing/quality indicators.

Classical algorithms should be evaluated before ML.

## Gate H — AI-assisted acquisition

Candidate features must each have an evaluation plan.

Examples:

- target/object classifier;
- quality estimator trained against expert/stack outcome labels;
- exposure/gain recommendation;
- bad-condition detection;
- focus quality model.

Required before release:

- baseline comparison;
- model card;
- representative datasets;
- calibration/failure behavior;
- performance on target hardware;
- offline inference;
- user override.

## Gate I — Processing pipeline

Possible order:

1. frame quality ranking;
2. alignment/registration;
3. stacking;
4. sharpening/wavelets;
5. color and denoising tools;
6. live preview of stack where performance allows;
7. batch/reproducible processing recipes.

Raw source data remains preserved by default.

## Gate J — Ecosystem/cloud

Only after the desktop product is valuable by itself.

Potential additions:

- accounts and entitlement;
- preset synchronization;
- remote model/profile delivery;
- optional telemetry;
- gallery/share;
- plugin SDK;
- extension signing/sandboxing;
- marketplace.

## Release channel concept

Eventually use explicit channels:

- **Nightly/Development** — automated, unsupported, diagnostics enabled;
- **Preview/Beta** — signed where practical, feature validation;
- **Stable** — hardware-tested, packaged, supported.

Never auto-upgrade stable users to experimental builds.
