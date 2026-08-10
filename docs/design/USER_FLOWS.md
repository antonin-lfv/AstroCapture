# Core User Flows

## Flow 1 — First launch with no camera

1. Application opens immediately to main workspace.
2. Live area shows intentional no-camera state.
3. Device discovery starts asynchronously.
4. User receives one clear action: connect/rescan/choose camera.
5. If OS permission is needed, app explains why before/around system prompt.
6. If permission is denied, app provides recovery instructions without trapping user.

Success criterion: a first-time user understands what to do without reading a manual.

## Flow 2 — Camera detected

1. Camera appears with human-readable identity.
2. Single obvious camera may be auto-selected but not silently opened if that creates permission/device conflicts; product testing decides exact behavior.
3. User selects Connect.
4. App enters connecting state.
5. Valid default stream mode is selected.
6. Live preview starts.
7. Contextual controls populate from capabilities.

If connection fails, retain selected device and present Retry + technical details.

## Flow 3 — Tune a planet

1. User centers target in telescope.
2. Live image remains responsive.
3. User adjusts exposure/gain from main inspector.
4. Histogram updates at a useful but not acquisition-saturating cadence.
5. Clipping feedback is visible.
6. User zooms/pans without modifying camera ROI accidentally.
7. Future focus metric can be enabled contextually.

## Flow 4 — Define ROI

When supported:

1. User activates ROI tool.
2. Preview indicates sensor/full-frame context where useful.
3. User selects region or chooses preset.
4. Requested ROI is snapped/validated to camera constraints.
5. If applying ROI restarts stream, UI communicates brief transition.
6. Preview updates and effective FPS may change.

Do not imply arbitrary crop equals hardware ROI; label software crop distinctly if both exist.

## Flow 5 — Record

1. User chooses capture mode/format/duration if needed.
2. User presses Record.
3. Recorder prepares output before UI claims recording is active.
4. Recording state becomes unmistakable.
5. Counters update smoothly, not necessarily every frame.
6. Disk/queue warning appears only when relevant.
7. User stops or configured condition completes.
8. File finalizes.
9. Summary confirms output and integrity statistics.

## Flow 6 — Camera unplugged while previewing

1. Backend reports failure/disconnect.
2. Acquisition shuts down safely.
3. UI preserves workspace and shows disconnected state.
4. Reconnect scanning continues or is user-triggerable.
5. Once same camera returns, user can reconnect; later automatic reconnect may be optional.

No application crash.

## Flow 7 — Camera unplugged while recording

1. Stop accepting frames.
2. Finalize recording as partial if possible.
3. Mark session incomplete/interrupted.
4. Surface exact recorded/dropped frame counts available.
5. Preserve file rather than deleting useful data by default.
6. Offer reconnect/new capture.

## Flow 8 — Disk too slow

1. Recorder queue occupancy increases.
2. Application shows escalating warning before overflow when possible.
3. User can continue, stop, or potentially lower data rate.
4. Any lost frame is counted.
5. Summary records integrity issue.

Never report a capture as clean when app knows frames were lost.

## Flow 9 — Development without hardware

1. Developer starts app with mock backend enabled.
2. Selects synthetic Bayer camera.
3. Exercises all common controls.
4. Starts recording.
5. Simulates slow disk/disconnect/error.
6. Runs UI and integration tests deterministically.

## Flow 10 — Future assisted acquisition

1. User opts into Assistant mode.
2. AstroCapture observes histogram/target/quality.
3. App recommends changes with reasons.
4. User can Apply, Ignore, or enable Auto.
5. Automatic changes are visible in controls/history.
6. Manual input immediately returns control to user according to documented behavior.

Automation should feel collaborative, not mysterious.
