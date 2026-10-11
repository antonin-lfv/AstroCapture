# Changelog

## 1.16.0 — 2026-10-11

- Support scientific / still images and saved results up to 128 MP, with a
  32,700-pixel dimension limit and explicit memory/recovery budgets.
- Accumulate mono/RGB and native Bayer drizzle projections in 64 MiB strips,
  preserving double sums, source order, footprint geometry and Float32 outputs.
  Store strips in the existing private archive; roll back abandoned reservations.
- Read/write FITS in bounded blocks and export immutable samples without a full
  planar copy. Stream TIFF pixel encoding without a complete encoded-file buffer.
- Check expensive finishing/wavelet combinations and decoded session previews
  against the local memory budget before calculation/loading; preserve prior
  results on refusal. Keep the 4 GiB recovery limit and existing project formats.
- Update shared image bounds, PNG/TIFF decoder allowance, French help and guide.
  Full scientific results remain in RAM; this is not an unlimited out-of-core
  engine, and actual shifted-series/Siril qualification remains separate.

## 1.15.1 — 2026-10-10

- Restore French workspace names in the save-before-quitting dialog.
- Add an isolated full-window test with cross-process session recovery, mock
  recording, lossless caption exports and cancellation checks.
- Add a macOS personal-use validation runner for a bounded full filesystem
  and native startup from the packaged DMG with protected user preferences.

## 1.15.0 — 2026-10-10

- Review and build calibration masters in sequential batches by recorded sensor
  configuration, with explicit flat correction choices and metadata review.
- Verify source hashes and preserve per-master provenance; stop at cancellation
  or first failure, keeping completed masters and the previous image result.
- Search/filter the master library and rank compatible entries before use;
  preserve original normalization units when importing Float32 masters.
- Share retained-image memory estimates across integration and batch planning;
  update French help, guide and dark checkbox states.

## 1.14.0 — 2026-10-10

- Estimate stacking memory, temporary disk space and recovery limits before
  computation; expose a local temporary-folder choice and clear refusal reasons.
- Bound rejection sample buffers to 64 MiB using horizontal and vertical tiles;
  retain the existing 32 MP output limit and Float32 science conventions.
- Use one private temporary archive per integration/robust master, with disk
  checks during writes and cleanup on cancellation, failure or process closure.
- Release unused master accumulation buffers before robust rejection; update
  French help and guide with the scope and limits of resource estimates.

## 1.2.0 — 2026-10-08

- Introduce Studio navigation with persistent workspace tabs, version, settings
  and activity indicators; launch on a new Home workspace.
- Resume work in memory or open recent saved deep-sky projects and SER videos.
- Move deep-sky tools and the editable linear histogram into the right inspector.
- Preserve controller state and background calculations when switching tabs.
- Document the new navigation and test recording locks and recent project storage.

## 1.1.2 — 2026-10-08

- Show a rotating activity indicator during deep-sky operations without changing
  preview geometry.
- Display a brief completion toast, with separate cancellation and error messages;
  queued adjustments notify only after the final calculation.

## 1.1.1 — 2026-10-08

- Replace boxed project gear with a larger vector icon; show contextual help on
  hover in dark tooltips and use dark, legible application dialogs.
- Start stacking only from Stack settings, group advanced options in a labelled
  panel and separate the launch action. Disable Finish until a result exists.
- Preserve zoom and pan across raw comparison, including different frame sizes.
- Reserve progress and status space to prevent preview height jumps while editing.
- Package a single versioned DMG and remove older installers only after success.


## 1.1.0 — 2026-10-08

- Save, load, duplicate, replace and delete named deep-sky finishing presets locally.
- Add independent parameter-only undo/redo histories for two A/B versions of the
  same linear stack; group slider drags and limit each history to 100 changes.
- Add an enlarged image view for A/B inspection, keeping histograms accessible.
- Reject corrupt finishing snapshots before replacing the current project.
- Save both versions and the selected version in projects; recreate saved finishes
  after integration. A new stack or channel composition resets the pair.
- Validate finishing parameters and prevent final export or transfer when live
  preview is off and settings have not been applied. Add contextual help and guide.


## 1.0.3 — 2026-10-08

- Add a bilingual guide topic for evaluating processing quality and update the
  explanation of project finishing restoration.

- Restore saved deep-sky finishing controls and automatically reapply them after
  recreating the stack, including when live preview is disabled.
- Clear pending restoration when clearing a project; suppress automatic preview
  requests while restoring controls. Verify restored settings and output pixels.


## 1.0.2 — 2026-10-08

- Show the application version in the window title and settings.
- Add an email bug report button with the version and reproduction template.


## 1.0.1 — 2026-10-08

- Accelerate large-image deep-sky stretching, color, saturation and Gaussian
  smoothing, plus B3-spline wavelets, with precise float32 Metal compute.
- Fuse full-resolution RGB8 previews with GPU processing. Preserve standalone
  CPU conversion and remove an unnecessary full-image float preview copy.
- Bound shared GPU scratch memory with overlapping strips; cache pipelines,
  propagate cancellation safely and fall back to CPU on unavailable/failed Metal.
- Preserve CPU calibration, registration, stacking and scientific exports.
- Add CPU/Metal precision, seam, rounding, concurrency, cancellation and fallback
  regressions plus a reproducible Siril benchmark. Metal can be disabled at build
  time or with ASTRO_DISABLE_METAL for runtime checks.

## 1.0.0 — 2026-10-07

- Refine deep-sky sections, parameter helpers, expandable advanced settings and
  project gear menu. Import folders with editable image roles and filter groups.
- Rename final export to Image finale and preserve exposure count, total exposure
  and filters when transferring a deep-sky stack to Processing and its caption.

- Compare a selected original light exposure against the finished deep-sky stack
  using a hold-to-view action. Load the first group reference automatically, label
  the source framing and independent automatic stretch, and preserve exports.

- Interactive linear histogram with stretch curve, black/midtone/white dragging,
  graph zoom and sampled clipping estimates; optional live finishing preview
  coalesces requests and keeps the latest settings. Provide automatic/linear reset
  actions and show Apply only when live preview is disabled.

- Simplify deep-sky workflow to Images, Stack settings, Finish and export; use raw
  calibration by default and reject conflicting calibration selections. Distinguish
  source previews from the stacked result, with a restore-result action.
- Add adaptive initial stretch, estimated background color gains, color-ratio
  preserving asinh and saturation with dark-background protection.

- Fix macOS worker-stack overflow when opening astronomical images in optimized
  builds by allocating the LibRaw decoder on the heap. Processing explicitly
  directs FITS files to Deep sky while preserving the current result.
- Deep-sky workspace with an APP-inspired configurable workflow, FITS/compressed
  FITS and Bayer RAW import, frame inspection and per-filter reference selection.
- Bias/dark/flat/dark-flat calibration before demosaicing, bad-pixel map support,
  star-based similarity registration, optional stellar-flux/background
  normalization, disk-backed integration and robust sample rejection.
- RGB/LRGB and narrowband channel composition, planar gradient correction,
  midtones stretch, color gains, smoothing and displayed RGB/luminance histogram.
- Linear float FITS exports, finished 16-bit TIFF with sRGB ICC profile, JPEG,
  protected originals, cancellation and source-referencing project files.
- Individual raw FITS acquisition sequences with metadata; fixed FITS/TIFF
  captures no longer lose sensor precision through an 8-bit preview.
- Validated ACRAW random access and Processing import; mono/Bayer 16-bit camera
  preview and histogram support. Camera parameters lock during recordings.
- CFITSIO and LibRaw build dependencies, Mac bundle notices and CI setup.

- Offline Guide tab in English and French, with capture, formats, processing,
  mosaic and export lessons.
- Direct PNG and TIFF processing, preserving 16-bit samples, with original-file
  overwrite protection and image drops scoped to the Processing tab.
- Native macOS mosaic image picker with image type filters.
- Per-image selection checkboxes; automatic exclusion keeps the largest
  connected group and leaves unmatched images visible and unchecked.
- Optional coverage-edge smoothing, preserved through mosaic finishing;
  noise-reduced feature detection for low-contrast surface images.
- Wavelet level 1 now reaches 50; other level limits are unchanged.

- Independent best-frame selection for each Moon/Sun alignment area, with
  visible area-size guidance and per-area selection counts.
- Mosaic thumbnail grid with asynchronous loading of visible previews.
- Optional caption titles above the caption text in Processing and Mosaic.
- Transfer a mosaic to Processing for finishing while preserving image
  precision, the number of assembled images and an editable caption.
- Removed hover explanations from the before/after comparison buttons.

- Automatic Moon and Sun mosaics from overlapping TIFF, PNG or JPEG images,
  with brightness matching, preview, cancellation and 16-bit TIFF export.
- Moon/Sun stacking mode with local surface alignment and a choice of
  64, 96 or 160-pixel alignment areas.
- Drag the blue sharpness-chart boundary to change the number of frames kept.
- Wavelet levels 1–3 now reach 20; levels 4–6 remain limited to 10.

- Captures made within the same second get distinct filenames instead of
  overwriting the preceding image or recording.
- Faster Bayer8 bilinear conversion with unchanged pixel results; avoid
  launching many worker threads for small previews and reject malformed
  frame dimensions/strides before reading pixels.
- Recording shutdown now seals the frame queue before draining it. Disk
  write errors stop recording with a visible failure; duration limits also
  stop capture when no new frame arrives, and statistics update at the
  preview cadence instead of queuing GUI updates for every camera frame.
- SER recordings now use the little-endian flag expected by Siril and
  SER Player, strip row padding, and reject format changes mid-recording.
- NexImage USB shutdown and partial startup failures wait for transfer
  cancellation callbacks before releasing buffers; failed resubmissions
  no longer leave a transfer permanently pending.
- Fixed the macOS development launch helper to run the executable inside
  the application bundle.

- Real camera support (Gate C, macOS): AstroCapture now drives actual
  astronomy cameras instead of only the mock. A Celestron NexImage 5 has
  been live-tested end-to-end, including color capture. Two USB backends
  cover this and similar hardware — standard UVC (`libuvc`) and a
  reverse-engineered proprietary protocol (`drivers/euvc`) for the older
  TIS/Celestron camera family that macOS's `UVCAssistant` system
  extension otherwise blocks from using standard UVC at all. See
  `docs/development/NEXIMAGE5_HARDWARE_NOTES.md` for what was learned.
- ZWO ASI camera backend (`drivers/zwo`), wrapping ZWO's official SDK —
  broadens support to the most common camera brand among the target
  audience. Implemented against ZWO's documented API and compiles/links
  correctly, but **not yet verified against real ZWO hardware** (none
  was available to test with).
- macOS Camera permission handling: the app now requests/shows up for
  the standard Camera privacy prompt (`System Settings > Privacy &
  Security > Camera`), required for raw USB video access to be
  authorized at all.
- Major live-preview performance fix when connected to a real
  high-resolution camera: debayering is now parallelized and the
  on-screen preview uses a decimated (lower-resolution) copy, while
  saved images still use the full-resolution frame — fixes severe UI
  lag/freezing that occurred with real (non-mock) high-megapixel
  cameras.
- Exposure control changed from a linear to a logarithmic slider, so
  the useful low-exposure range isn't compressed into a few pixels of
  travel; also fixes a rare out-of-range value that could freeze the UI
  for a second or two at high exposure settings.
- Connecting to the mock camera now requires an explicit confirmation
  dialog instead of happening automatically, so the home screen doesn't
  silently connect to fake hardware.
- First macOS packaging pipeline (`scripts/package_macos.sh`): produces
  a self-contained `.app` + DMG that runs without Homebrew/Qt/libusb
  installed on the target Mac. Ad-hoc signed only for now (no Apple
  Developer ID yet), so Gatekeeper will still warn on first launch —
  see `docs/development/RELEASE_AND_PACKAGING.md`.
- Live preview: "Rescan for cameras" now discovers real connected
  cameras (UVC/EUVC/ZWO, see above) alongside the mock. An earlier build
  briefly used the Mac's built-in webcam as a stand-in for a real camera;
  it was removed since generic webcams don't expose exposure/gain, making
  them unsuitable for developing or testing those controls.
- Exposure, gain, gamma, white balance, brightness and frame rate limit
  controls, each shown only for cameras that actually support it.
- Live histogram of the current frame, with min/max/mean and highlighted
  edges when highlights are blown or shadows are crushed.
- ROI/crop: stream a sub-region of the sensor instead of the full frame.
- Zoom loupe (hover the preview to pixel-peep a magnified crop) and a
  center crosshair overlay.
- Save the current frame to disk as PNG, 16-bit TIFF or FITS.
- Record the live stream to disk in SER — the standard planetary-video
  format read by AutoStakkert, PIPP, Registax and FireCapture — with a
  live status readout (elapsed time, frames recorded, write fps next to
  capture fps, dropped frames — recording is designed to never drop a
  frame silently). AstroCapture's own earlier interim format remains
  available as a fallback.
- Keyboard shortcuts: C to capture, R to start/stop recording.
- Exposure/gain/gamma/white balance/brightness/frame rate limit are
  remembered per camera and restored automatically next time that camera
  connects.
- Settings panel (gear icon): choose a custom save location for images/
  videos, image and video save formats, and save/load/delete named
  exposure+gain presets per camera.
- "Disconnect" button so a different camera can be selected without
  restarting the app.
- Recordings can auto-stop after a target frame count and/or duration.
- Optional target name (e.g. "Jupiter") is included in saved file names.
- When more than one real camera is connected, a picker lets you choose
  which one instead of always guessing the first one found.
- Continuous integration builds and tests the app on macOS, Windows and
  Linux on every change.
- ROI/crop is now selected by clicking and dragging directly on the
  preview instead of typing X/Y/Width/Height — drag again inside an
  existing selection to narrow it further, or double-click (or use the
  sidebar's "Full frame" button) to reset.
- Clickable "?" help icons next to less-obvious controls (frame counter,
  Recording, Sensor ROI, Image/Video format), explaining what they do.
- "Open" button next to the save location in Settings, to open it
  directly in the file manager.
- Selecting a saved preset shows a brief "applied" toast confirmation.
- Large frame counts are abbreviated (e.g. "10.1k" instead of "10123")
  so the counter stays compact.
- Various fit-and-finish fixes: the sidebar's scrollbar no longer
  overlaps its content; placeholder text (target name, preset name) is
  now readable; the Settings panel is wider and its format buttons
  (PNG/TIFF16/FITS, SER/ACRAW) split width evenly; saving a preset over
  an existing name now says so; the "Disconnect" button is plain
  centered text instead of an unnecessary icon; confirmation dialogs no
  longer render a button outside the modal's edge; the error message
  shown on the home screen no longer persists stale after disconnecting.
