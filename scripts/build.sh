#!/usr/bin/env bash
#
# Local dev helper for macOS. Wraps the CMake presets documented in
# docs/development/DEVELOPMENT.md so you don't have to remember the flags.
#
# Usage:
#   scripts/build.sh [command] [preset]
#
# Commands:
#   configure   Run cmake --preset <preset>            (only needed once, or after editing a CMakeLists.txt)
#   build       Run cmake --build --preset <preset>     (default command)
#   test        Run ctest --preset <preset>
#   run         Build then launch the AstroCapture binary
#   all         configure + build + test
#   clean       Remove build/<preset>
#
# Preset defaults to "dev". Pass "release" to target the release preset instead.
#
# Examples:
#   scripts/build.sh                # build with the dev preset
#   scripts/build.sh test           # run the unit tests
#   scripts/build.sh run            # build and launch the app
#   scripts/build.sh all release    # configure+build+test the release preset

set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")/.."

command="${1:-build}"
preset="${2:-dev}"

configure() { cmake --preset "$preset"; }
ensure_configured() {
    [[ -f "build/$preset/CMakeCache.txt" ]] || configure
}
build()     { ensure_configured; cmake --build --preset "$preset"; }
run_tests() { build; ctest --preset "$preset" --output-on-failure; }
run_app()   {
    build
    ./build/"$preset"/bin/AstroCapture
}

case "$command" in
    configure) configure ;;
    build)     build ;;
    test)      run_tests ;;
    run)       run_app ;;
    all)       run_tests ;;  # cascades: build -> ensure_configured -> configure
    clean)     rm -rf "build/$preset" ;;
    *)
        echo "Unknown command: $command" >&2
        echo "Usage: scripts/build.sh [configure|build|test|run|all|clean] [dev|release]" >&2
        exit 1
        ;;
esac
