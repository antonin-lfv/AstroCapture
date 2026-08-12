#pragma once

namespace astro::app {

// macOS-only concern (see ADR-005-macos-camera-permission.md): TCC gates
// raw USB access to Video-class interfaces the same way it gates
// AVFoundation, but only registers an app for that gate — and only shows
// the system prompt — once it goes through AVFoundation's own
// authorization API at least once. On every other platform there is no
// such gate, so this always reports Authorized there without asking
// anything (see CameraPermission.cpp).
enum class CameraAuthStatus { Authorized, Denied, Restricted, NotDetermined };

CameraAuthStatus cameraAuthorizationStatus();

// Fires the async system permission prompt when status is NotDetermined.
// Non-blocking by design: blocking the calling (Qt/AppKit) thread on the
// completion handler would prevent the system dialog itself from ever
// rendering, since it needs that same thread's run loop. Callers re-check
// cameraAuthorizationStatus() on a later call (e.g. the user clicking
// Rescan again) to see the outcome once they respond to the system prompt.
void requestCameraAuthorization();

}  // namespace astro::app
