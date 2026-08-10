# Changelog

All notable product changes will be documented here once implementation begins.

The changelog should describe user-visible changes, not every commit.

## Unreleased

- Live preview: "Rescan for cameras" now tries a connected USB webcam or the
  Mac's built-in camera first, falling back to a synthetic test camera when
  none is found.
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
- Continuous integration builds and tests the app on macOS, Windows and
  Linux on every change.
