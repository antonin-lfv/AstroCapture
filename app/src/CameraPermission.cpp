#include "CameraPermission.hpp"

namespace astro::app {

// Non-Apple platforms have no TCC-style gate on raw USB Video-class access
// (see header comment) — nothing to check or request.
CameraAuthStatus cameraAuthorizationStatus() { return CameraAuthStatus::Authorized; }

void requestCameraAuthorization() {}

}  // namespace astro::app
