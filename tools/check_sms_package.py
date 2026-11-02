"""Verify the optional DOS download against its qualified source and EXEs."""
from pathlib import Path
import hashlib
import json
import re
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[1]
DIRECTORY = ROOT / 'examples/smsboot'
manifest = json.loads((DIRECTORY / 'DOS-BUILD.json').read_text())
native = json.loads((DIRECTORY / 'NATIVE.json').read_text())['stroke_logo_run']
sha = lambda data: hashlib.sha256(data).hexdigest()
# This download is an immutable historical qualification, not a promise that
# its binaries were rebuilt whenever an unrelated/current library changed.
# Check the recorded commit's exact source; never rewrite its hashes to match
# an untested current tree. CI fetches history so this check stays offline.
revision = manifest['qualified_source_commit']
assert re.fullmatch(r'[0-9a-f]{40}', revision), 'qualified source commit'
current_differences = []
for name, digest in manifest['source_lf_sha256'].items():
    assert not Path(name).is_absolute() and '..' not in Path(name).parts, name
    try:
        qualified = subprocess.check_output(
            ['git', 'show', revision + ':' + name], cwd=ROOT,
            stderr=subprocess.PIPE)
    except subprocess.CalledProcessError as error:
        raise RuntimeError('Historical package check needs git history for ' +
                           revision + '; fetch that commit or use a full clone') from error
    assert sha(qualified.replace(b'\r\n', b'\n')) == digest, name
    if sha((ROOT / name).read_bytes().replace(b'\r\n', b'\n')) != digest:
        current_differences.append(name)
archive = DIRECTORY / 'SMSBOOT.ZIP'
assert sha(archive.read_bytes()) == manifest['zip_sha256'], 'ZIP hash'
with zipfile.ZipFile(archive) as package:
    assert package.testzip() is None
    assert set(package.namelist()) == set(manifest['members']), 'ZIP members'
    for name, expected in manifest['members'].items():
        data = package.read(name)
        assert len(data) == expected['bytes'] and sha(data) == expected['sha256'], name
    for name in ('SMSBOOT.EXE', 'TANDYLOG.EXE'):
        data = package.read(name)
        assert data[:2] == b'MZ', name
        assert sha(data) == native[name]['sha256'], name + ': native receipt'
        assert len(data) == native[name]['bytes'], name + ': size'
print('PASS qualified DOS package: pinned source ' + revision + ', ZIP, EXE/native hashes and members')
if current_differences:
    print('Current sources differ from that historical binary: ' + ', '.join(current_differences))
