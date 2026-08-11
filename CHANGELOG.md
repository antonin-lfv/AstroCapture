# Changelog

All notable product changes will be documented here once implementation begins.

The changelog should describe user-visible changes, not every commit.

## Unreleased

- Live preview: "Rescan for cameras" connects to a synthetic test camera
  until a real astronomy camera backend exists (Gate C). An earlier build
  briefly used the Mac's built-in webcam as a stand-in; it was removed
  since generic webcams don't expose exposure/gain, making them unsuitable
  for developing or testing those controls.
- Exposure and gain controls, shown only for cameras that actually support
  them.
- Live histogram of the current frame.
- Save the current frame to disk as a PNG image.
- Record the live stream to disk, with a live status readout (elapsed time,
  frames recorded, dropped frames — recording is designed to never drop a
  frame silently). Recordings are written in AstroCapture's own interim
  format, not yet the planned SER format.
- Exposure/gain are remembered per camera and restored automatically next
  time that camera connects.
- Settings panel (gear icon): choose a custom save location for images/
  videos, and save/load/delete named exposure+gain presets per camera.
- "Disconnect" button so a different camera can be selected without
  restarting the app.
- Recordings can auto-stop after a target frame count and/or duration.
- Optional target name (e.g. "Jupiter") is included in saved file names.
- When more than one real camera is connected, a picker lets you choose
  which one instead of always guessing the first one found.
- Continuous integration builds and tests the app on macOS, Windows and
  Linux on every change.
