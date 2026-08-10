# Product Vision

## 1. Why AstroCapture exists

Astronomical capture software often exposes the complexity of cameras, drivers and image processing directly to the user. The result can be powerful but visually dense, platform-limited, inconsistent or difficult to approach.

AstroCapture aims to combine three qualities that are too rarely found together:

1. **capture reliability** suitable for demanding planetary imaging;
2. **excellent interaction design** that makes the software pleasant to use in the dark and under field conditions;
3. **intelligent assistance** that progressively removes repetitive tuning without hiding expert controls.

The project begins with a concrete pain: reliable color acquisition from an astronomy camera on macOS. The product should earn the right to grow by solving that problem exceptionally well first.

## 2. Primary users

### Enthusiast planetary imager

Needs high frame rates, ROI, gain/exposure control, raw recording, histogram feedback, reliable disk behavior and repeatable profiles.

### Beginner with a planetary camera

Needs immediate feedback, sane defaults, comprehensible camera settings and help avoiding saturated, underexposed or unfocused captures.

### Advanced amateur / power user

Needs transparency, precise controls, raw formats, keyboard efficiency, diagnostics, reproducibility and eventually scripting/automation.

### Hardware vendor / ecosystem partner — future

May want an official driver integration, validation matrix, configuration profiles or plugin extension point.

## 3. Product promise

Connect a supported camera and get from “device connected” to “recording useful astronomical data” with minimal friction.

AstroCapture should then help the user improve the data, not merely provide access to device controls.

## 4. Experience principles

- The image is the hero.
- Capture health is always understandable.
- Important controls are close to the task.
- Rare controls do not visually dominate.
- The product teaches by feedback rather than documentation walls.
- Automation explains itself.
- Expert users can bypass automation.
- A failed device does not crash the application.
- A missing Internet connection does not stop local imaging.
- A new version should not invalidate saved workflows without migration.

## 5. Positioning

AstroCapture should not initially compete by matching every checkbox of mature Windows-only suites. It should compete through:

- first-class macOS experience;
- deliberate cross-platform engineering;
- modern UI/UX;
- strong acquisition fundamentals;
- a path toward intelligent acquisition and processing;
- cleaner architecture for future hardware extensions.

## 6. Non-goals for the first product

The first commercially credible version is not:

- an observatory scheduler;
- a planetarium;
- a complete telescope mount controller;
- a social network;
- a cloud-only imaging platform;
- a general-purpose photo editor;
- an AI demo.

It is an excellent acquisition application.

## 7. Business model hypotheses

Do not hardcode business assumptions into core architecture.

Potential models to evaluate later:

- free core + paid Pro acquisition/processing features;
- one-time license + paid major upgrades;
- subscription for cloud/advanced AI while acquisition remains locally usable;
- vendor partnerships;
- paid professional support.

The licensing system should be an application service, never scattered through camera or processing code.

## 8. Product metrics to establish

Technical:

- successful camera connection rate;
- crash-free sessions;
- recording sessions without dropped frames attributable to the app;
- average time from launch to live preview;
- reconnect success rate;
- CPU/GPU/memory profiles by reference hardware.

Product:

- percentage of users who complete a first recording;
- repeat capture sessions;
- usage of profiles/assist tools;
- support requests by camera/backend;
- feature adoption versus UI complexity.

## 9. Long-term product story

A mature AstroCapture session could become:

1. connect camera;
2. application identifies capabilities;
3. user points telescope;
4. AstroCapture identifies/centers the target or helps the user do so;
5. focus guidance evaluates sharpness;
6. acquisition assistant recommends ROI/exposure/gain;
7. capture runs while quality and disk health are monitored;
8. frames are ranked or stacked locally;
9. user receives an editable processing result and a preserved raw capture;
10. all decisions remain reproducible and overrideable.

That future should emerge incrementally from a reliable acquisition core.
