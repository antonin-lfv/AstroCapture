#!/usr/bin/env bash
set -euo pipefail

# Packages a release-configured AstroCapture.app into a self-contained
# bundle + DMG. See docs/development/RELEASE_AND_PACKAGING.md section 4.
#
# This script gets the app to "self-contained and installable" — no
# Homebrew/Qt/libusb install required on the target Mac. It does NOT get
# it to "Gatekeeper-trusted": signing here is ad-hoc (identity "-"), since
# that's all that's possible without an Apple Developer ID (see
# docs/decisions — real distribution needs a paid Developer Program
# membership this project doesn't have yet). Recipients will need to
# right-click > Open (or clear the quarantine attribute) the first time;
# `spctl --assess` on the result is expected to fail until this script's
# ad-hoc `codesign` is swapped for a real Developer ID + notarization.

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${REPO_ROOT}/build/release"
APP_NAME="AstroCapture"
APP_BUNDLE="${BUILD_DIR}/bin/${APP_NAME}.app"
EXECUTABLE="${APP_BUNDLE}/Contents/MacOS/${APP_NAME}"
FRAMEWORKS_DIR="${APP_BUNDLE}/Contents/Frameworks"

echo "==> Configuring + building the release preset"
cmake --preset release "${REPO_ROOT}"
cmake --build "${BUILD_DIR}" --target "${APP_NAME}"

if ! command -v macdeployqt >/dev/null 2>&1; then
    echo "macdeployqt not found on PATH (expected alongside the Homebrew Qt6 install)" >&2
    exit 1
fi

echo "==> Running macdeployqt (bundles Qt frameworks/plugins)"
# -qmldir points macdeployqt at our QML sources so it can scan the actual
# imports (QtQuick.Layouts, QtQuick.Controls.Basic, ...) and bundle the
# matching Qt-provided QML modules — without it, those never get copied
# in at all and the app fails to load its own UI at launch with "module
# ... is not installed", even though our own qrc:/-embedded QML is found
# fine.
macdeployqt "${APP_BUNDLE}" -qmldir="${REPO_ROOT}/app/qml"

echo "==> Bundling native dependencies (Qt frameworks macdeployqt missed, libusb, libuvc, ASICamera2, ...)"
# macdeployqt walks the main executable's own dependencies but doesn't
# reliably follow *transitive* ones, and Homebrew's own Qt frameworks
# reference sibling Qt frameworks two different ways depending on
# whether macdeployqt already touched them: @rpath/QtXxx.framework/...
# for ones it fixed up, or an absolute Cellar path
# (/opt/homebrew/opt/qtbase/lib/QtXxx.framework/Versions/A/QtXxx) for
# ones it never saw at all (e.g. QtGui -> QtDBus, or any Qt3D/
# QtVirtualKeyboard/etc. module only pulled in transitively). A naive
# "copy whatever this absolute path points at" treats a framework
# binary like a plain dylib — copying just the flat Mach-O file, losing
# the Versions/A/... structure codesign requires, and leaving its own
# sub-dependencies (e.g. QtCore's icu4c/pcre2/double-conversion) never
# vendored. vendor_binary() below handles both reference styles the
# same way: recognize anything shaped like ".../<Name>.framework/
# Versions/.../<Name>" as a framework (regardless of whether it arrived
# as @rpath or absolute) and vendor the whole .framework directory,
# never just the inner binary.
QT_LIB_DIR="$(brew --prefix qt6 2>/dev/null || true)/lib"
if [[ ! -d "${QT_LIB_DIR}" ]]; then
    echo "Could not resolve the Homebrew Qt6 lib directory (tried '${QT_LIB_DIR}')" >&2
    exit 1
fi
mkdir -p "${FRAMEWORKS_DIR}"

declare -A bundled_framework
declare -A bundled_flat

# Vendors the whole `fw_name.framework` directory from `fw_root_src`
# (an absolute path to the source .framework, symlink or real) into
# Contents/Frameworks/, once, then recurses into its own dependencies.
vendor_framework() {
    local fw_name="$1" fw_root_src="$2"
    [[ -n "${bundled_framework[${fw_name}]:-}" ]] && return
    local dest="${FRAMEWORKS_DIR}/${fw_name}.framework"
    if [[ ! -e "${dest}" ]]; then
        if [[ ! -e "${fw_root_src}" ]]; then
            echo "    WARNING: can't find source framework ${fw_root_src} for ${fw_name} — skipping" >&2
            bundled_framework[${fw_name}]=1
            return
        fi
        echo "    vendoring framework: ${fw_name}"
        # Homebrew's lib/QtXxx.framework entries are themselves a symlink
        # (one hop) to the real Cellar location; plain `cp -R` copies that
        # symlink-to-a-directory as just the symlink, not its contents,
        # which then resolves relative to the wrong place once it's
        # inside the bundle. Resolve that one outer hop ourselves, but
        # copy WITHOUT further dereferencing (`cp -RL` recursively
        # flattens the framework's own internal symlink structure too —
        # Versions/Current -> A, and top-level Headers/Resources/<Binary>
        # -> Versions/Current/... — into duplicated real files, which
        # makes codesign reject the framework as "bundle format is
        # ambiguous").
        local real_src
        real_src="$(cd "${fw_root_src}" && pwd -P)"
        cp -R "${real_src}" "${FRAMEWORKS_DIR}/"
        chmod -R u+w "${dest}"
    fi
    # A previous deployment may have copied this framework without normalizing its ID.
    install_name_tool -id "@rpath/${fw_name}.framework/Versions/A/${fw_name}" "${dest}/Versions/A/${fw_name}"
    bundled_framework[${fw_name}]=1
    vendor_binary "${dest}/Versions/A/${fw_name}"
}

# Vendors a single flat (non-framework) dylib into Contents/Frameworks/,
# once, then recurses into its own dependencies.
vendor_flat_dylib() {
    local dep="$1"
    local name="$(basename "${dep}")"
    [[ -n "${bundled_flat[${name}]:-}" ]] && return
    local dest="${FRAMEWORKS_DIR}/${name}"
    if [[ ! -e "${dest}" ]]; then
        echo "    vendoring ${dep}"
        cp -L "${dep}" "${dest}"
        chmod u+w "${dest}"
    fi
    install_name_tool -id "@rpath/${name}" "${dest}"
    bundled_flat[${name}]=1
    vendor_binary "${dest}"
}

# Walks `binary`'s dependencies (via otool -L) and, for each one that
# isn't already a system/@executable_path/@loader_path reference,
# vendors it (framework or flat dylib, see above) and rewrites `binary`'s
# reference to it to the @rpath form pointing at the vendored copy.
vendor_binary() {
    local binary="$1"
    local deps
    # `dep` MUST be local: vendor_framework()/vendor_flat_dylib() below can
    # recurse back into vendor_binary() for a dependency's own dependencies,
    # and without `local` that inner call's loop clobbers this same
    # (otherwise global-by-default) variable out from under this frame's
    # loop — breaking the install_name_tool -change calls below that still
    # need this iteration's original `${dep}` after the recursive call
    # returns.
    local dep
    deps="$(otool -L "${binary}" 2>/dev/null | tail -n +2 | awk '{print $1}')"
    while IFS= read -r dep; do
        [[ -z "${dep}" ]] && continue
        case "${dep}" in
            /usr/lib/*|/System/*|@executable_path/*|@loader_path/*) continue ;;
        esac
        if [[ "${dep}" == *.framework/* ]]; then
            local fw_root fw_name
            fw_root="${dep%%.framework/*}.framework"
            fw_name="$(basename "${fw_root}")"
            fw_name="${fw_name%.framework}"
            if [[ "${dep}" == @rpath/* ]]; then
                # Already rpath-relative: only needs vendoring if
                # macdeployqt never bundled it (the QtGui -> QtDBus-style
                # gap) — the known source for that is Homebrew's Qt lib
                # dir, since every framework reachable this way is Qt's.
                vendor_framework "${fw_name}" "${QT_LIB_DIR}/${fw_name}.framework"
            else
                vendor_framework "${fw_name}" "${fw_root}"
                install_name_tool -change "${dep}" "@rpath/${fw_name}.framework/Versions/A/${fw_name}" "${binary}"
            fi
        elif [[ "${dep}" != @rpath/* ]]; then
            vendor_flat_dylib "${dep}"
            local name="$(basename "${dep}")"
            install_name_tool -change "${dep}" "@rpath/${name}" "${binary}"
        fi
    done <<< "${deps}"
}

# Blind solving runs separate native Astrometry.net helpers. No Python runtime
# or user image upload is needed for FITS extraction and solving.
ASTROMETRY_PREFIX="$(brew --prefix astrometry-net)"
HELPERS_DIR="${APP_BUNDLE}/Contents/Helpers/astrometry"
mkdir -p "${HELPERS_DIR}"
for source_helper in "${ASTROMETRY_PREFIX}"/bin/*; do
    if file -b "${source_helper}" | grep -q "Mach-O"; then
        helper="${HELPERS_DIR}/$(basename "${source_helper}")"
        cp -L "${source_helper}" "${helper}"
        chmod u+w "${helper}"
        vendor_binary "${helper}"
        install_name_tool -add_rpath "@loader_path/../../Frameworks" "${helper}" 2>/dev/null || true
    fi
done
cp -R "${REPO_ROOT}/third_party/notices" "${APP_BUNDLE}/Contents/Resources/ThirdPartyNotices"

echo "  fixing ${EXECUTABLE}"
vendor_binary "${EXECUTABLE}"

# Also walk every framework/plugin macdeployqt already placed in the
# bundle — vendor_binary()/vendor_framework()/vendor_flat_dylib() all
# dedupe via the bundled_* maps, so re-scanning things already handled
# is a cheap no-op; this just guarantees coverage of binaries the
# EXECUTABLE-rooted recursion above wouldn't otherwise reach (e.g. a
# QML plugin macdeployqt copied that nothing else references).
for existing in "${FRAMEWORKS_DIR}"/*.framework/Versions/A/* "${APP_BUNDLE}"/Contents/PlugIns/*/*.dylib; do
    [[ -f "${existing}" ]] || continue
    vendor_binary "${existing}"
done

# Existing @rpath dylibs can be skipped by the dependency walk. Normalize
# their own install IDs too, so repeated packaging remains self-contained.
for existing in "${FRAMEWORKS_DIR}"/*.dylib; do
    [[ -f "${existing}" ]] || continue
    vendor_flat_dylib "${existing}"
done

# macdeployqt already adds an @executable_path/../Frameworks rpath for
# Qt's own libraries; add it again defensively in case that ever changes
# (install_name_tool -add_rpath fails harmlessly if it's already there).
install_name_tool -add_rpath "@executable_path/../Frameworks" "${EXECUTABLE}" 2>/dev/null || true

echo "==> Re-signing everything (ad-hoc — see this script's header comment)"
codesign --force --deep --sign - "${APP_BUNDLE}"

echo "==> Verifying"
codesign --verify --deep --strict "${APP_BUNDLE}"
# Scan every Mach-O in the bundle, not just the main executable — this is
# exactly the check that would have caught the QtGui->QtDBus gap fixed
# above before it ever reached a live launch test.
homebrew_refs_found=0
while IFS= read -r -d '' candidate; do
    if file -b "${candidate}" 2>/dev/null | grep -q "Mach-O"; then
        if otool -L "${candidate}" 2>/dev/null | tail -n +2 | grep -q "/opt/homebrew\|/usr/local"; then
            echo "WARNING: $(basename "${candidate}") still references a Homebrew-absolute path:" >&2
            otool -L "${candidate}" | grep "/opt/homebrew\|/usr/local" >&2
            homebrew_refs_found=1
        fi
    fi
done < <(find "${APP_BUNDLE}" -type f -print0)
if [[ "${homebrew_refs_found}" -eq 1 ]]; then
    echo "Packaging is incomplete — see warnings above." >&2
    exit 1
fi
spctl --assess --type execute "${APP_BUNDLE}" || echo "  (expected without a Developer ID — see this script's header comment)"

echo "==> Creating DMG"
DMG_STAGING="$(mktemp -d)"
trap 'rm -rf "${DMG_STAGING}"' EXIT
cp -R "${APP_BUNDLE}" "${DMG_STAGING}/"
ln -s /Applications "${DMG_STAGING}/Applications"
APP_VERSION="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' "${APP_BUNDLE}/Contents/Info.plist")"
[[ "${APP_VERSION}" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || { echo "Invalid app version for packaging" >&2; exit 1; }
DMG_PATH="${BUILD_DIR}/${APP_NAME}-${APP_VERSION}.dmg"
rm -f "${DMG_PATH}"
hdiutil create -volname "${APP_NAME}" -srcfolder "${DMG_STAGING}" -ov -format UDZO "${DMG_PATH}"

# Keep the previous installer until creation succeeds, then retain only this one.
for previous_dmg in "${BUILD_DIR}/${APP_NAME}.dmg" "${BUILD_DIR}/${APP_NAME}-"*.dmg; do
    [[ -f "${previous_dmg}" && "${previous_dmg}" != "${DMG_PATH}" ]] || continue
    rm -f "${previous_dmg}"
done

echo "==> Done: ${DMG_PATH}"
