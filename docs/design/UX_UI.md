# UX / UI Direction

## 1. Design objective

AstroCapture must look and behave like a premium modern desktop application while remaining credible as scientific/technical software.

The goal is **not** to imitate Apple literally. The goal is the same class of perceived care: clear hierarchy, restrained visual noise, coherent interactions, polished empty/error states, responsive feedback and excellent typography/layout decisions.

This document intentionally avoids rigid global pixel dimensions. The design system should define relationships and reusable tokens while leaving room for iteration, platform adaptation, accessibility and user testing.

## 2. Core visual principle: image first

The live astronomical image is the primary content.

Controls should frame the image rather than compete with it. During capture, transient UI can become calmer while critical state remains visible.

## 3. Information hierarchy

Suggested levels:

### Primary

- live image;
- connect/camera identity;
- record/start-stop state;
- exposure/gain when actively tuning;
- critical warning such as dropped frames or disk failure.

### Secondary

- histogram;
- FPS;
- ROI/resolution;
- white balance;
- focus metric;
- session/capture target.

### Tertiary / advanced

- backend details;
- raw pixel format;
- SDK version;
- USB tuning;
- debug metrics;
- rarely used camera controls.

## 4. Workspace concept

Use a stable workspace with contextual panels rather than many independent modal windows.

Potential regions:

- top application/capture bar;
- central live viewport;
- contextual right inspector for camera/capture controls;
- optional left navigation/tool rail if feature count justifies it;
- bottom expandable analysis area for histogram/timeline/metrics.

Panels may become resizable/dockable later if power users demand it. Do not prematurely create an IDE-like docking system.

## 5. First launch / no camera

This is a marketing-quality state.

It should feel intentional:

- subtle illustration/iconography;
- concise “Connect a camera to begin” message;
- rescan action;
- supported-camera/help entry point;
- permission guidance if macOS/OS denies camera access;
- recent session/replay option in future;
- development builds may offer “Open simulated camera”.

Avoid an empty gray canvas with disabled controls.

## 6. Camera connection flow

When multiple cameras exist, selection should show useful identity:

- vendor/model;
- device identifier/serial where appropriate;
- backend if ambiguous;
- support status if experimental.

Connection progress should be visible but brief. Errors remain near the relevant device.

## 7. Camera inspector

Organize controls into semantic groups, not vendor order.

Example:

### Capture
- Exposure
- Gain
- Frame rate/readout where controllable

### Color
- White balance
- Debayer/display color mode

### Sensor / framing
- Resolution
- ROI
- Binning

### Advanced
- Gamma
- offset/black level
- vendor-specific parameters

Only show controls that exist for the selected camera.

## 8. Control behavior

Every numeric camera control should support efficient precise interaction where useful:

- slider/drag for rapid adjustment;
- editable numeric value for precision;
- reset/default;
- auto/manual toggle if supported;
- units visible;
- keyboard adjustment where practical.

Do not force users to drag a slider to achieve exact astronomical settings.

Camera writes may be asynchronous; UI should reflect pending/rejected values when needed instead of pretending every set succeeds instantly.

## 9. Live view interactions

Potential interactions:

- zoom to cursor;
- fit-to-view;
- 100% pixel view;
- pan;
- ROI selection;
- crosshair/reticle;
- pixel/value inspection later;
- focus area selection later.

The app should preserve predictable zoom behavior. Avoid accidental wheel interactions changing camera exposure while pointer is over image.

## 10. Capture workflow

Recording is a deliberate mode.

Before capture, user configures:

- output location/profile;
- duration OR frame count OR manual stop;
- format;
- optional object/session metadata.

During capture:

- strong but elegant recording indicator;
- elapsed/progress;
- frame count;
- effective receive/record rate;
- dropped frames;
- file/session name;
- health warning.

After capture:

- confirmation;
- resulting file/session location;
- key stats;
- “show in Finder/Explorer/file manager”;
- future: inspect/process capture.

Avoid modal confirmation for routine Start Recording unless an actual risk exists.

## 11. Histogram UX

Histogram is an acquisition tool, not decorative graphing.

It should help answer:

- Am I clipping highlights?
- Is exposure too low?
- Are color channels badly imbalanced?

Future guidance can annotate clipping thresholds without obscuring raw data.

## 12. Status and warnings

Use severity consistently:

- informational;
- attention/warning;
- critical/capture integrity.

Examples:

Informational: camera changed resolution.

Warning: recording queue is consistently high; disk may be too slow.

Critical: frame writes failed; recording is incomplete.

Never encode severity by color alone. Use icon/text/placement as well.

## 13. Dark environment

Astronomers often operate at night. Default visual design should minimize glare.

Consider later:

- dark theme as primary;
- optional deep-red/night mode;
- dimmable non-image UI;
- preservation of image color accuracy despite UI night mode.

A red UI mode must not accidentally alter recorded data or scientific preview settings without clear indication.

## 14. Motion

Animation should reinforce state and continuity:

- panel transitions;
- connected/recording state changes;
- expanding advanced sections;
- transient success feedback.

Avoid decorative motion that consumes GPU/CPU during high-rate acquisition or becomes distracting in darkness.

Respect reduced-motion accessibility settings where available.

## 15. Typography and density

Use a small coherent typography hierarchy. Technical values require excellent numeric legibility.

Allow the design system to evolve. Prefer semantic tokens like:

- text-primary;
- text-secondary;
- surface-raised;
- accent;
- warning;
- critical;
- spacing-compact/normal/relaxed;

rather than hardcoding arbitrary values across QML files.

## 16. Platform adaptation

Common product structure should remain recognizable across platforms.

Adapt where users expect OS conventions:

- window chrome;
- file pickers;
- shortcuts;
- system menus;
- modifier keys;
- Finder/Explorer reveal actions;
- macOS permissions flow.

Do not create three completely different user experiences.

## 17. Accessibility

At minimum:

- keyboard-reachable critical actions;
- readable contrast;
- labels for icons;
- no color-only communication;
- scalable typography/layout consideration;
- focus states;
- reduced-motion awareness;
- sensible screen-reader semantics where Qt permits.

## 18. Power-user efficiency

Potential shortcuts:

- start/stop recording;
- fit/100% preview;
- exposure nudging;
- gain nudging;
- toggle histogram;
- toggle overlays.

Shortcuts must be discoverable in menus/tooltips/help.

## 19. Settings

Keep global Preferences distinct from camera session controls.

Preferences might eventually include:

- default recording location;
- filename pattern;
- theme/night mode;
- update channel;
- performance options;
- diagnostic logging;
- privacy/telemetry choices.

## 20. UI quality checklist

Before calling a screen complete:

- Is the primary action obvious?
- Is the dominant visual element correct?
- Are empty/loading/error/disabled states designed?
- Is technical jargon necessary and explained?
- Are values and units unambiguous?
- Can the workflow be completed with keyboard/mouse naturally?
- Does resizing degrade gracefully?
- Does the UI remain usable while streaming/recording?
- Are warnings actionable?
- Is advanced complexity progressively disclosed?
