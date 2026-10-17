"""End-to-end CLI contract, using real FITS pixels and only the Python stdlib."""
import hashlib
import json
import math
from pathlib import Path
import signal
import struct
import subprocess
import sys
import tempfile

binary = str(Path(sys.argv[1]).resolve())


def invoke(*args, code=0):
    result = subprocess.run([binary, '--agent', *map(str, args)], capture_output=True, text=True, timeout=120)
    assert result.returncode == code, (result.returncode, result.stdout, result.stderr)
    events = [json.loads(line) for line in result.stdout.splitlines()]
    assert events and all(event['schemaVersion'] == 1 for event in events), result.stdout
    return events


def fits(path, shift=0):
    cards = [('SIMPLE', 'T'), ('BITPIX', '-32'), ('NAXIS', '2'), ('NAXIS1', '128'), ('NAXIS2', '128'), ('EXPTIME', '30')]
    header = ''.join(f'{k:<8}= {v:>20}'.ljust(80) for k, v in cards) + 'END'.ljust(80)
    header = header.ljust((len(header) + 2879) // 2880 * 2880)
    stars = [(12 + (i * 31) % 102, 12 + (i * 47) % 102) for i in range(20)]
    pixels = [0.01 + sum(0.4 * math.exp(-((x-sx-shift)**2+(y-sy)**2)/3.0) for sx, sy in stars)
              for y in range(128) for x in range(128)]
    data = struct.pack('>' + 'f' * len(pixels), *pixels)
    path.write_bytes(header.encode('ascii') + data + b'\0' * (-len(data) % 2880))


with tempfile.TemporaryDirectory(prefix='astro-agent-test-') as directory:
    root = Path(directory)
    sources = root / 'images with spaces'
    sources.mkdir()
    for i in range(3):
        fits(sources / f'light-{i}.fits', i)
    fingerprints = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in sources.iterdir()}
    assert invoke('help')[-1]['scope'] == 'deep-sky'
    inspect = invoke('inspect', sources)[-1]
    assert inspect['event'] == 'inspection' and len(inspect['frames']) == 3
    recipe = {'version': 1, 'inputs': [{'path': sources.name, 'role': 'Light'}],
              'calibrated': True, 'output': 'results', 'finish': {'saturation': 0.9}}
    recipe_file = root / 'recipe.json'

    def run_recipe(value, code=0):
        recipe_file.write_text(json.dumps(value))
        return invoke('run', recipe_file, code=code)

    for change in ({'calibrated': 'true'}, {'sigma': 0}, {'unknown': True}, {'finish': {'denoise': '0'}},
                   {'finish': {'unknown': 1}}, {'finish': {'white': 0}}):
        event = run_recipe(recipe | change, code=2)[-1]
        assert event['report']['error']['code'] == 'invalid_input'
        assert not (root / 'results').exists()
    assert run_recipe(recipe | {'inputs': recipe['inputs'] * 2}, code=2)[-1]['event'] == 'error'
    result = run_recipe(recipe)[-1]['report']
    assert result['state'] == 'completed' and len(result['results']) == 1
    output = root / 'results'
    stored = json.loads((output / 'report.json').read_text())
    assert stored == result
    group = output / 'group-001'
    for name in ('linear.fits', 'finished.tiff', 'preview.jpg', 'result.astroproject'):
        assert (group / name).stat().st_size > 0
    assert b'BITPIX  =                  -32' in (group / 'linear.fits').read_bytes()[:2880]
    loaded = invoke('inspect', group / 'result.astroproject')[-1]
    assert loaded['event'] == 'project' and loaded['hasResult']
    assert loaded['finish']['saturation'] == 0.9
    checkpoint = invoke('inspect', output / 'stacked.astroproject')[-1]
    assert checkpoint['hasResult'] and checkpoint['groups'] == loaded['groups']
    # Multiple filters stay separate and names cannot escape the output folder.
    multi = recipe | {'output': 'multi-results', 'inputs': [
        {'path': str(sources / 'light-0.fits'), 'role': 'Light', 'group': '../R'},
        {'path': str(sources / 'light-1.fits'), 'role': 'Light', 'group': 'G'}]}
    multi_result = run_recipe(multi)[-1]['report']
    assert len(multi_result['results']) == 2
    for result_group in multi_result['results']:
        assert Path(result_group['directory']).parent == (root / 'multi-results').resolve()
        assert invoke('inspect', Path(result_group['directory']) / 'result.astroproject')[-1]['hasResult']
    original_report = (output / 'report.json').read_bytes()
    run_recipe(recipe, code=2)
    assert (output / 'report.json').read_bytes() == original_report
    assert {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in sources.iterdir()} == fingerprints
    # Corruption must fail the entire import, not silently produce a partial stack.
    (sources / 'broken.fits').write_bytes(b'broken')
    failure = run_recipe(recipe | {'output': 'broken-results'}, code=1)[-1]['report']
    assert failure['error']['code'] == 'import_failed'
    assert not (root / 'broken-results' / 'stacked.astroproject').exists()
    (sources / 'broken.fits').unlink()
    # Interrupt only once the JSON protocol confirms that an operation started.
    recipe_file.write_text(json.dumps(recipe | {'output': 'cancelled-results'}))
    process = subprocess.Popen([binary, '--agent', 'run', str(recipe_file)], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    first = json.loads(process.stdout.readline())
    assert first['event'] == 'started'
    process.send_signal(signal.SIGINT)
    stdout, stderr = process.communicate(timeout=30)
    assert process.returncode == 130, (stdout, stderr)
    assert json.loads(stdout.splitlines()[-1])['report']['state'] == 'cancelled'
print('Agent mode: FITS processing, project round-trip, validation, source preservation, cancellation OK')
