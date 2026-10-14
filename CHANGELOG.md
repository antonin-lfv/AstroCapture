# Changelog

## 1.20.0 — 2026-10-14

- Cache object search keys, replace the full catalogue sort with bounded ranked results, debounce typing and require a number for bare catalogue prefixes. Show at most 25 local results, retaining exact matches first and explicit online search.
- Match equivalent catalogue numbers consistently, including spaces, letter case and leading zeroes (for example M42 and M 042).
- Add direct place favourite and delete controls. Keep saved places when their star is removed, support preset favourites and deletion, migrate existing favourites and preserve the selected position and UTC instant.
- Add optional website links to saved observing places, opened in the browser only on request; validate addresses and preserve links when renaming.
- Restore deleted built-in places from Advanced without changing personal places, favourites, coordinates or the selected UTC instant.
- Select the saved copy correctly when it shares a preset’s name and coordinates; reject malformed saved coordinates instead of silently moving them to zero.
- Reject incomplete or invalid restored object coordinates without overwriting the saved settings or adding an invalid favourite; allow recovery by selecting a valid catalogue object.
- Prevent saving a place with default coordinates when the initial observation calculation has failed.
- Report an unavailable saved object at startup and wait for an explicit catalogue or manual-coordinate choice, preserving its saved settings instead of silently selecting the Moon. Keep automatic suggestions paused and provide a recovery action at the top of Observation.
- Validate automatic-recovery workspace, asset and pixel-size limits before writing files; preserve the previous recovery across abrupt process exits during saving.
- Include images, previews and masks retained by Processing and Mosaic in the existing Deep Sky memory budget, counting shared buffers once and updating the resource estimate immediately.
- Reuse each topocentric solar position for altitude and day/night classification during Observation map updates, preserving the exact astronomical results while reducing cursor calculation time.
- Bound city searches to 15 seconds even when a remote response continues arriving slowly; preserve previous results on failure.
- Build the macOS DMG from a fresh deployment copy, verify it before replacement and retain previous installers. Generate and verify the Linux ZIP automatically, including launch instructions, camera access rules and an AppImage checksum.
- Update French labels, the Observation Guide and README.

## 1.19.0 — 2026-10-13

- Add a Linux x86-64 AppImage build for Mint 21/22 and Ubuntu 22.04+, with Qt/QML runtime deployment, desktop integration, dependency notices and optional USB access rules.
- Enable the vendored ZWO ASI SDK on Linux x86-64 and detect distro libuvc through its CMake or pkg-config targets. Keep CPU processing and the macOS Metal path intact; Linux hardware capture still requires physical validation.
- Install Qt image-format plugins in automated builds so TIFF reading and export verification use the same capabilities as the distributed application.
- Use computer-neutral observing clocks and help, French labels and platform-appropriate undo shortcut hints.
- Add an installation check that loads the real QML interface and reports required image codecs and compiled camera drivers, with optional private visual checks at the three supported window sizes.
- Document installation, updates and camera access on Linux Mint.

## 1.18.9 — 2026-10-12

- Make both home and observing-site clocks follow the selected instant after every date, time, slider or chart change. Keep both conversions visible above the charts while scrolling.
- Show recording windows first in the Mac's local date/time, with site time below when different. Remove GMT-offset suffixes from the primary clocks and preserve exact UTC/DST metadata.
- Show an explicit disabled-menu message when no target is recommended at the selected time or night; retain the previously displayed target and catalogue access.
- Stack the sky compass below place/time in the left quarter. Keep day/night, target selection and the large visibility map in the right three quarters, then equal height/cloud charts below both columns.
- Preserve explicit UTC warnings for unresolved map points, ongoing-night guards, continuous updates, scientific calculations and saved projects. Update French help, Guide, README and native validation.

## 1.18.8 — 2026-10-12

- Restore the last open project workspace when resuming a session, including sessions saved from Home and older sessions without workspace metadata.
- Stack the observing place and time controls to the left of the day/night map; move reference-date and time-zone options into a dialog and align the recommendation and object menus.
- Show the next sunset and sunrise in the Mac's local time, with the date and zone. Preserve the shared UTC instant and site time on chart axes.
- Remove the redundant lunar/solar detail and bottom target sections; keep instant and whole-night recommendations in the single object chooser, plus catalogue, favourites and recording windows.
- Fix saved-place removal when the menu sorts favourites before the built-in places.
- Match the native macOS title bar to the dark canvas, retain native window buttons, centre the AstroCapture title and display the version at the centre of the footer.
- Update French labels, Guide, README and native validation.

## 1.18.7 — 2026-10-12

- Remove the Observation sidebar and use the central page throughout: place/day-night map at 25/75, time and target controls, visibility map/sky compass at 75/25, then height/cloud curves at 50/50.
- Sort place favourites first and names alphabetically within each group. Open naming, city search, exact coordinates and advanced options in dialogs; apply coordinate drafts only on confirmation.
- Offer visible recommendations in a target menu and select the first one by default. Preserve explicit target choices while exploring, full catalogue/favourite search, recording windows and scientific details.
- Distinguish the observing place's displayed time from the Mac's actual local clock. Resolve map points' IANA time zones through Open-Meteo while preserving UTC; debounce map requests and report unresolved/offline states with a manual override.
- Keep continuous slider updates and both maps synchronized. Fit the compass labels at small widths, preserve missing-cloud and connection-loss states, and update French help, Guide, README and native validation.

## 1.18.6 — 2026-10-12

- Start Observation with the selected place: local clock, day/twilight/night, cloud forecast and upcoming solar events; put the place selector first and recommendations before the target curve.
- Recommend targets at the shared instant, including height and darkness criteria, or compare observing windows over the selected night. Show three suggestions initially and preserve catalogue search, favourites and geometric limits.
- Prepare the current dark night or the next one, including the interval between astronomical dawn and sunrise. Cache upcoming solar events beyond local noon for the morning summary.
- Load the generic Open-Meteo forecast on entry and place/date changes, retaining offline, stale, missing-data and failure states. Add T68 in Utah as a geographic preset only, usable alongside any custom place.
- Preserve UTC during city selection and normalize validated coordinates entered with decimal commas. Update the French Guide, README and native visual checks.

## 1.18.5 — 2026-10-12

- Combine the height curve, sky compass, world map and clouds on one scrolling Observation page, with a shared time control that stays visible. Keep recommendations and scientific details expandable.
- Add an offline day, twilight and astronomical-night map, independent of the target, alongside the existing object-visibility layer. Both share the same UTC instant; map clicks preserve it.
- Follow the real clock with Now, refreshing every 30 seconds while visible. Date/hour/slider edits pause live time; Now returns from another night to the local noon-to-noon period containing the actual instant. Preserve observing-night guards and home/telescope reference clocks.
- Use a common Observation card and responsive columns, retain cloud cache/source/network states and update French help, Guide and README.

## 1.18.4 — 2026-10-12

- Add a reference clock using the Mac’s time zone by default or the telescope site’s configured zone, with both local dates/times displayed when they differ.
- Let the user choose a reference calendar day and enter an hour to synchronize the cursor and all four views; reject nonexistent hours and offer both UTC offsets for repeated hours.
- Show the first remaining recording window in Night, converted to the reference zone, with a direct action to inspect its start. Round starts up and ends down to whole minutes inside the height/darkness window; retain precise intervals in expandable details.
- Preserve observing-site coordinates, the chosen night and UTC instant when switching reference zones, and persist the preferred reference. Clearly identify chart, solar and forecast times at the telescope.
- Preserve the exact UTC instant throughout city, favourite, coordinate, time-zone and map location changes; select the local noon-to-noon period containing it, without publishing a transient shifted instant.
- Verify timezone conversions, midnight dates, persistence, clock changes, recording-window bounds and synchronization; update French help, Guide and README.

## 1.18.3 — 2026-10-12

- Update the Observation time cursor continuously during dragging, coalescing intermediate changes without postponing every update until release; apply the final slider value immediately.
- Rename Horizon to Sky and explain its compass projection, theoretical horizon, zenith, selected point and nightly path; show the direction to look alongside object height.
- Explain that a first launch uses central Paris and subsequent launches restore the last location, without automatic geolocation.
- Make place naming and renaming explicit, retaining favourite coordinates and time zones, including map positions.
- Verify actual mouse drags in all four time views, preserve final cursor accuracy, and update French help, the guide and README.

## 1.18.2 — 2026-10-12

- Make the active Observation target explicit and group catalogue search and favourites in one chooser; filtering leaves the selected target unchanged until a result is chosen.
- Start with two clear actions and reveal charts, suggestions, visibility settings and detailed solar/lunar/weather information progressively. Show four suggestions initially, with more available on request.
- Reject ended nights when planning, using the observing site’s local time and sunrise; retain an ongoing night after midnight and support explicit past-observation review in Advanced.
- Give World a target-specific title, compact visibility legend and time slider; keep local observing windows and solar/lunar details in Night and Horizon.
- Update French help, the integrated guide and README; verify selection, past-date protection, time zones, sunrise boundaries, clock changes and native layouts.

## 1.18.1 — 2026-10-12

- Add an hourly cloud forecast view to Observation, using Open-Meteo total/low/mid/high cloud coverage for the selected coordinates, synchronized with the local/UTC time cursor.
- Show forecast instants, retrieval time, source attribution and model grid coordinates; summarize available hourly cloud values within each suggested object's windows without changing geometric ranking.
- Keep a validated local forecast cache on network failure; mark forecasts older than one hour, reject invalid responses and never treat missing data or another location's forecast as clear sky.
- Load forecasts on request, reuse fresh data and update after a selected place/date change. Clearly identify dates outside the up-to-16-day horizon. Forecast values are model predictions, not live measurements or a transparency/seeing estimate.
- Verify recorded real API data, zero versus null values, forecast-hour selection, invalid responses, location changes, stale/offline cache handling, live retrieval and native French layouts.

## 1.18.0 — 2026-10-11

- Add an independent Observation workspace before Guide, for the Moon, seven planets and fixed catalogue objects.
- Bundle the OpenNGC catalogue and Astronomy Engine; provide offline identifier search, optional CDS Sesame name lookup, city presets, optional Open-Meteo / GeoNames city search and saved local sites.
- Synchronize night altitude, horizon trajectory and regional world visibility with a local/UTC time slider. Show geometric centre rise/set, highest altitude, eligible intervals and lunar illumination/separation.
- Save favourite objects and places locally, including coordinates resolved online; keep them directly accessible and restore them across launches.
- Show solar rise/set (upper limb with standard refraction), daylight, civil twilight and astronomical-night intervals, plus the day/twilight/night state at the selected instant.
- Recommend common targets and favourites from location, local date and minimum height, with validated observing windows and explicit per-target darkness criteria; support both hemispheres and polar cases offline.
- Report absent, interrupted and restored connectivity and online search failures while preserving previous results.
- Apply IANA time zones and daylight-saving changes; explain height, darkness, polar cases, data sources and planning limits in French and English. Preserve processing and project formats.
- Verify five topocentric positions against independent NASA/JPL Horizons references, and cover hemispheres, polar conditions, date changes, 23/25-hour periods, invalid inputs, persistence and native QML composition.

## 1.17.0 — 2026-10-11

- Add RGB/LRGB/HOO/SHO palette selection with automatic integrated-filter assignment, explicit missing-filter checks and a preserved Custom mode.
- Blend the aligned luminance contribution from 0 to 100%, preserving the previous 100% default and linear channel ratios.
- Save composition choices in projects/recovery and distinguish displayed results from choices awaiting recomposition. Check memory/recovery budgets, preserve prior results on errors/cancellation and clear obsolete composed-image astrometry.
- Add French contextual help, a multifilter guide and native workflow/precision checks.

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
