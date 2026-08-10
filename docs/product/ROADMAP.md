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

- polished camera selector/empty state;
- stable live preview;
- common camera controls;
- histogram;
- still image capture: save the currently previewed frame to disk (PNG/TIFF/FITS);
- video recording: record the live stream to a suitable astronomy format, SER prioritized;
- capture duration/frame targets;
- clear recording status;
- no silent dropped frames;
- file/session metadata;
- basic settings persistence;
- smoke-tested production package on macOS.

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
