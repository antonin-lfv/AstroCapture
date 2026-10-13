#!/usr/bin/env bash
set -euo pipefail

if [[ "$(uname -s)" != Linux || "$(uname -m)" != x86_64 ]]; then
    echo "Build this package on Linux x86-64 (Mint 21/22 or Ubuntu 22.04+)." >&2
    exit 1
fi
if [[ $# -gt 1 || ( $# -eq 1 && "$1" != --skip-build ) ]]; then
    echo "Usage: $0 [--skip-build]" >&2
    exit 1
fi

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${REPO_ROOT}/build/release-linux"
APPDIR="${BUILD_DIR}/AppDir"
TOOLS_DIR="${REPO_ROOT}/build/linux-packaging-tools"
cd "$REPO_ROOT"
if [[ $# -eq 0 ]]; then
    cmake --preset release-linux
    cmake --build --preset release-linux --parallel "${CMAKE_BUILD_PARALLEL_LEVEL:-2}"
    ctest --preset release-linux
fi
[[ -x "${BUILD_DIR}/bin/AstroCapture" ]] || { echo "Build release-linux first." >&2; exit 1; }
ASTRO_PACKAGE_VERSION="$(sed -n 's/^CMAKE_PROJECT_VERSION:STATIC=//p' "${BUILD_DIR}/CMakeCache.txt")"
[[ "$ASTRO_PACKAGE_VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || { echo "Missing build version." >&2; exit 1; }
mkdir -p "$TOOLS_DIR"

fetch_tool() {
    local name="$1" url="$2" digest="$3"
    local target="${TOOLS_DIR}/${name}"
    if [[ ! -f "$target" ]] || ! echo "$digest  $target" | sha256sum --check --status; then
        curl --fail --location --retry 3 --silent --show-error "$url" --output "${target}.download"
        echo "$digest  ${target}.download" | sha256sum --check --status
        mv "${target}.download" "$target"
    fi
    chmod +x "$target"
}
fetch_tool linuxdeploy-x86_64.AppImage \
    https://github.com/linuxdeploy/linuxdeploy/releases/download/1-alpha-20251107-1/linuxdeploy-x86_64.AppImage \
    c20cd71e3a4e3b80c3483cef793cda3f4e990aca14014d23c544ca3ce1270b4d
fetch_tool linuxdeploy-plugin-qt-x86_64.AppImage \
    https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/1-alpha-20250213-1/linuxdeploy-plugin-qt-x86_64.AppImage \
    15106be885c1c48a021198e7e1e9a48ce9d02a86dd0a1848f00bdbf3c1c92724

# Only our install component: do not ship OpenCV's build headers/static archives.
rm -rf "$APPDIR"
DESTDIR="$APPDIR" cmake --install "$BUILD_DIR" --prefix /usr --component AstroCapture
# Preserve the distribution's copyright and license notices for the image/USB
# libraries that linuxdeploy collects from system packages.
NOTICES_DIR="${APPDIR}/usr/share/licenses/AstroCapture"
for package in libcfitsio10 libraw20 libusb-1.0-0 libuvc0 libjpeg-turbo8; do
    notice="/usr/share/doc/${package}/copyright"
    if [[ -f "$notice" ]]; then
        cp "$notice" "${NOTICES_DIR}/${package}-copyright.txt"
    fi
done
desktop-file-validate "${APPDIR}/usr/share/applications/astrocapture.desktop"
export QMAKE="${QMAKE:-$(command -v qmake6 || command -v qmake)}"
QT_LIB_DIR="$("$QMAKE" -query QT_INSTALL_LIBS)"
QT_PLUGINS_DIR="$("$QMAKE" -query QT_INSTALL_PLUGINS)"
export LD_LIBRARY_PATH="${BUILD_DIR}/drivers/zwo:${QT_LIB_DIR}${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export PATH="${TOOLS_DIR}:${PATH}"
export QML_SOURCES_PATHS="${REPO_ROOT}/app/qml"
export EXTRA_PLATFORM_PLUGINS="libqoffscreen.so"
for plugin in libqwayland-egl.so libqwayland-generic.so; do
    if [[ -f "${QT_PLUGINS_DIR}/platforms/${plugin}" ]]; then
        EXTRA_PLATFORM_PLUGINS+=";${plugin}"
    fi
done
# Packaging works in CI/containers without FUSE. The delivered AppImage also
# supports --appimage-extract-and-run on machines without FUSE.
export APPIMAGE_EXTRACT_AND_RUN=1
export VERSION="$ASTRO_PACKAGE_VERSION"
export OUTPUT="${BUILD_DIR}/AstroCapture-${ASTRO_PACKAGE_VERSION}-x86_64.AppImage"
LIBUSB_RUNTIME="$(pkg-config --variable=libdir libusb-1.0)/libusb-1.0.so.0"
OPENGL_RUNTIME="$(ldconfig -p | awk '$1 == "libOpenGL.so.0" && !found {print $NF; found=1}')"
[[ -f "$LIBUSB_RUNTIME" && -f "$OPENGL_RUNTIME" ]] || { echo "USB/OpenGL runtime libraries are missing." >&2; exit 1; }
# linuxdeploy normally excludes these as system libraries. They are not
# guaranteed on a desktop without other Qt/camera applications installed.
"${TOOLS_DIR}/linuxdeploy-x86_64.AppImage" --appdir "$APPDIR" \
    --desktop-file "${APPDIR}/usr/share/applications/astrocapture.desktop" \
    --icon-file "${APPDIR}/usr/share/icons/hicolor/256x256/apps/astrocapture.png" \
    --executable "${APPDIR}/usr/bin/AstroCapture" \
    --library "$LIBUSB_RUNTIME" --library "$OPENGL_RUNTIME" --plugin qt --output appimage

ACCESS_DIR="${BUILD_DIR}/linux-camera-access"
mkdir -p "$ACCESS_DIR"
cp "${REPO_ROOT}/packaging/linux/70-astrocapture-cameras.rules" "$ACCESS_DIR/"
cp "${REPO_ROOT}/scripts/install_linux_camera_rules.sh" "$ACCESS_DIR/"
chmod +x "${ACCESS_DIR}/install_linux_camera_rules.sh"
(
    cd "$BUILD_DIR"
    sha256sum "$(basename "$OUTPUT")" > SHA256SUMS
)
echo "Linux package ready: $OUTPUT"
