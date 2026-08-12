#include "CameraPermission.hpp"

#import <AVFoundation/AVFoundation.h>

namespace astro::app {

CameraAuthStatus cameraAuthorizationStatus() {
    switch ([AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeVideo]) {
        case AVAuthorizationStatusAuthorized:
            return CameraAuthStatus::Authorized;
        case AVAuthorizationStatusDenied:
            return CameraAuthStatus::Denied;
        case AVAuthorizationStatusRestricted:
            return CameraAuthStatus::Restricted;
        case AVAuthorizationStatusNotDetermined:
        default:
            return CameraAuthStatus::NotDetermined;
    }
}

void requestCameraAuthorization() {
    [AVCaptureDevice requestAccessForMediaType:AVMediaTypeVideo
                              completionHandler:^(BOOL){
                                  // Intentionally empty: callers observe the
                                  // outcome via cameraAuthorizationStatus()
                                  // on their own time (see header comment).
                              }];
}

}  // namespace astro::app
