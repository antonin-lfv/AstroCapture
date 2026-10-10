#!/usr/bin/env python3
"""Include matching upstream sources and build recipes for GPL/LGPL helpers."""
from pathlib import Path
import hashlib
import os
import re
import shutil
import subprocess
import sys
import urllib.request

repo = Path(__file__).resolve().parent.parent
destination = (Path(sys.argv[1]) / 'Contents/Resources/ThirdPartySources') if sys.argv[1] != '--prepare-wcslib' else None
cache = repo / 'build/third-party-sources'
if destination is not None:
    destination.mkdir(parents=True, exist_ok=True)
cache.mkdir(parents=True, exist_ok=True)
WCS_VERSION = '8.10'
WCS_URL = 'https://www.atnf.csiro.au/computing/software/wcs/wcslib-releases/wcslib-8.10.tar.bz2'
WCS_SHA256 = 'b394d3306da9a52410f056136911496e599b5c48f1cff2f221563697077bbd8d'


def fetch_archive(name, version, url, checksum):
    suffix = '.tar.bz2' if '.tar.bz2' in url else '.tar.gz'
    archive = cache / (name + '-' + version + suffix)
    if not archive.exists() or hashlib.sha256(archive.read_bytes()).hexdigest() != checksum:
        print('Fetching corresponding sources:', name, version, file=sys.stderr, flush=True)
        temporary = archive.with_suffix('.download')
        try:
            with urllib.request.urlopen(url, timeout=45) as response, temporary.open('wb') as output:
                size = 0
                while block := response.read(1024 * 1024):
                    size += len(block)
                    if size > 100 * 1024 * 1024:
                        raise RuntimeError('Source archive exceeds the packaging size limit')
                    output.write(block)
            if hashlib.sha256(temporary.read_bytes()).hexdigest() != checksum:
                raise RuntimeError('Source archive checksum mismatch: ' + name)
            temporary.replace(archive)
        finally:
            temporary.unlink(missing_ok=True)
    return archive


def prepare_wcslib():
    # Build this exact archive privately: the upstream 8.10 tarball was repacked
    # after the installed Homebrew bottle. Never claim that bottle corresponds
    # to a source archive with a different checksum.
    archive = fetch_archive('wcslib', WCS_VERSION, WCS_URL, WCS_SHA256)
    work = repo / 'build/packaging-wcslib'
    prefix = work / 'installed'
    library = prefix / 'lib/libwcs.8.dylib'
    marker = prefix / 'source.sha256'
    if library.exists() and marker.exists() and marker.read_text().strip() == WCS_SHA256:
        return library
    work.mkdir(parents=True, exist_ok=True)
    source = work / ('wcslib-' + WCS_VERSION)
    if source.exists():
        shutil.rmtree(source)
    subprocess.run(['tar', '-xjf', str(archive), '-C', str(work)], check=True, stdout=sys.stderr)
    for directory in source.rglob('RCS'):
        if directory.is_dir():
            shutil.rmtree(directory)
    cfitsio = subprocess.check_output(['brew', '--prefix', 'cfitsio'], text=True).strip()
    env = dict(os.environ, CFLAGS='-O2 -include stdio.h', MACOSX_DEPLOYMENT_TARGET='13.0')
    commands = [
        ['./configure', '--prefix=' + str(prefix), '--disable-fortran', '--enable-shared',
         '--without-pgplot', '--with-cfitsiolib=' + cfitsio + '/lib',
         '--with-cfitsioinc=' + cfitsio + '/include'],
        ['make', '-j4'], ['make', 'check'], ['make', 'install']]
    for command in commands:
        subprocess.run(command, cwd=source, env=env, check=True, stdout=sys.stderr, stderr=sys.stderr)
    if not library.exists():
        raise RuntimeError('WCSLIB shared library was not produced')
    marker.write_text(WCS_SHA256 + '\n')
    return library


if sys.argv[1] == '--prepare-wcslib':
    print(prepare_wcslib())
    sys.exit(0)

for name in ('astrometry-net', 'gsl'):
    prefix = Path(subprocess.check_output(['brew', '--prefix', name], text=True).strip()).resolve()
    recipe = prefix / '.brew' / (name + '.rb')
    source = recipe.read_text()
    url = re.search(r'^  url "([^"]+)"', source, re.M).group(1)
    checksum = re.search(r'^  sha256 "([a-f0-9]{64})"', source, re.M).group(1)
    archive = fetch_archive(name, prefix.name, url, checksum)
    shutil.copy2(archive, destination)
    shutil.copy2(recipe, destination)
    (destination / (name + '-source.txt')).write_text(f'{name} {prefix.name}\nUpstream source: {url}\nSHA256: {checksum}\nBuild recipe: {recipe.name}\n')
archive = fetch_archive('wcslib', WCS_VERSION, WCS_URL, WCS_SHA256)
shutil.copy2(archive, destination)
shutil.copy2(Path(__file__), destination / 'package_astrometry_sources.py')
shutil.copy2(repo / 'scripts/package_macos.sh', destination / 'package_macos.sh')
(destination / 'wcslib-source.txt').write_text(
    f'WCSLIB {WCS_VERSION}\nUpstream source: {WCS_URL}\nSHA256: {WCS_SHA256}\n'
    'Built from this archive, not the Homebrew WCSLIB bottle.\n'
    'Build recipe: python3 package_astrometry_sources.py --prepare-wcslib\n'
    'See prepare_wcslib() for configure flags and build/check/install steps.\n')
shutil.copy2(repo / 'third_party/notices/Astrometry-SOURCES.txt', destination / 'README.txt')
