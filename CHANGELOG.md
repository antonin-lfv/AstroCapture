# Changelog

All notable product changes will be documented here once implementation begins.

The changelog should describe user-visible changes, not every commit.

## Unreleased

- Live preview: "Rescan for cameras" connects to a synthetic test camera
  until a real astronomy camera backend exists (Gate C). An earlier build
  briefly used the Mac's built-in webcam as a stand-in; it was removed
  since generic webcams don't expose exposure/gain, making them unsuitable
  for developing or testing those controls.
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
- Various fit-and-finish fixes: the sidebar's scrollbar no longer
  overlaps its content; placeholder text (target name, preset name) is
  now readable; the Settings panel is wider and its format buttons
  (PNG/TIFF16/FITS, SER/ACRAW) split width evenly; saving a preset over
  an existing name now says so; the "Disconnect" button is plain
  centered text instead of an unnecessary icon.
