#!/usr/bin/env python3
"""Isolated personal-use checks, including a bounded full filesystem and DMG boot.

Build astro_app_personal_use_test first. User preferences, recovery and sources
are never used as writable test destinations. No physical camera is connected.
"""
import argparse
import errno
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time


def run(command, *, environment=None, log=None):
    result = subprocess.run(command, env=environment, capture_output=True, text=True)
    output = result.stdout + result.stderr
    if log is not None:
        log.write_text(output)
    if result.returncode:
        raise RuntimeError(f"Command failed ({result.returncode}): {command[0]}\n{output}")
    return output


def attach(image, mount, *, readonly=False):
    command = ["hdiutil", "attach", "-nobrowse", "-mountpoint", str(mount)]
    if readonly:
        command.append("-readonly")
    run(command + [str(image)])


def workflows(runner, temporary, environment, output):
    work = temporary / "work"
    work.mkdir()
    for stage in ("save", "restore"):
        run([str(runner), "--" + stage, str(work)], environment=environment,
            log=output / (stage + ".log"))
        print(stage + ": passed", flush=True)

    image, volume = temporary / "bounded-full-disk.dmg", temporary / "full-volume"
    volume.mkdir()
    run(["hdiutil", "create", "-size", "64m", "-fs", "HFS+", "-volname",
         "AstroCapture Validation", str(image)])
    attach(image, volume)
    try:
        shutil.copytree(work / "recovery", volume / "recovery")
        for name in ("preserve.png", "preserve.fits", "preserve.astroproject"):
            (volume / name).write_bytes(b"Existing file: must remain unchanged")
        # Fill only this private 64 MiB volume. Never fill the host disk.
        assert shutil.disk_usage(volume).total <= 80 * 1024 * 1024
        filled = 0
        with (volume / "filler").open("wb", buffering=0) as filler:
            block = bytes(256 * 1024)
            try:
                while True:
                    count = filler.write(block)
                    assert count > 0
                    filled += count
                    assert filled <= 80 * 1024 * 1024
            except OSError as error:
                if error.errno != errno.ENOSPC:
                    raise
        assert shutil.disk_usage(volume).free < 256 * 1024
        faults_environment = dict(environment, ASTRO_PERSONAL_FAULT_DIRECTORY=str(volume))
        run([str(runner), "--faults", str(work)], environment=faults_environment,
            log=output / "disk-full.log")
        print("bounded full filesystem: passed", flush=True)
    finally:
        run(["hdiutil", "detach", str(volume)])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fits", type=Path, help="Read-only RGB FITS; use its central 1024 × 768 crop")
    parser.add_argument("--dmg", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=Path("build/personal-validation"))
    parser.add_argument("--startup-only", action="store_true", help="Repeat only the packaged application startup")
    args = parser.parse_args()
    repo = Path(__file__).resolve().parent.parent
    runner = repo / "build/release/bin/astro_app_personal_use_test"
    assert runner.is_file() and args.dmg.is_file()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    environment = dict(os.environ, QT_QPA_PLATFORM="offscreen", QT_QUICK_BACKEND="software",
                       ASTRO_PERSONAL_QA_DIR=str(output / "screenshots"))
    if args.fits:
        assert args.fits.is_file()
        environment["ASTRO_PERSONAL_FITS_PATH"] = str(args.fits.resolve())

    with tempfile.TemporaryDirectory(prefix="astrocapture-personal-validation-") as directory:
        temporary = Path(directory)
        if not args.startup_only:
            workflows(runner, temporary, environment, output)

        mount = temporary / "installer"
        mount.mkdir()
        attach(args.dmg.resolve(), mount, readonly=True)
        try:
            executable = mount / "AstroCapture.app/Contents/MacOS/AstroCapture"
            private_home = temporary / "bundle-data"
            private_home.mkdir()
            bundle_environment = dict(environment, CFFIXED_USER_HOME=str(private_home), QT_QPA_PLATFORM="cocoa")
            paths = run([str(runner), "--paths"], environment=bundle_environment).splitlines()
            assert len(paths) == 3 and all(str(private_home) in path for path in paths[1:])
            # Qt's native preferences filename still refers to the real user.
            # Deny writes and preferences-daemon access as an additional guard.
            library = str(Path.home() / "Library").replace('"', '\\"')
            profile = ('(version 1)(allow default)(deny file-write* (subpath "' + library + '"))'
                       '(deny mach-lookup (global-name "com.apple.cfprefsd.daemon")'
                       '(global-name "com.apple.cfprefsd.agent"))')
            with (output / "dmg-startup.log").open("wb") as log:
                process = subprocess.Popen(["sandbox-exec", "-p", profile, str(executable)],
                                           env=bundle_environment, stdout=log, stderr=log)
                try:
                    time.sleep(5)
                    assert process.poll() is None, "Packaged application exited during startup"
                finally:
                    if process.poll() is None:
                        process.terminate()
                        try:
                            process.wait(timeout=10)
                        except subprocess.TimeoutExpired:
                            process.kill()
                            process.wait()
            log = (output / "dmg-startup.log").read_text(errors="replace")
            for error in ("failed to load component", "is not installed", "ReferenceError",
                          "TypeError", "Binding loop", "sandbox-exec:"):
                assert error not in log, log
            print("read-only DMG startup with protected user preferences: passed", flush=True)
        finally:
            run(["hdiutil", "detach", str(mount)])
    print("Requested checks passed; temporary volumes detached.", flush=True)


if __name__ == "__main__":
    main()
