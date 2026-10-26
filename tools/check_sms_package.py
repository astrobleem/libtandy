"""Verify the optional DOS download against its qualified source and EXEs."""
from pathlib import Path
import hashlib
import json
import zipfile

ROOT = Path(__file__).resolve().parents[1]
DIRECTORY = ROOT / 'examples/smsboot'
manifest = json.loads((DIRECTORY / 'DOS-BUILD.json').read_text())
native = json.loads((DIRECTORY / 'NATIVE.json').read_text())['stroke_logo_run']
sha = lambda data: hashlib.sha256(data).hexdigest()
for name, digest in manifest['source_lf_sha256'].items():
    assert sha((ROOT / name).read_bytes().replace(b'\r\n', b'\n')) == digest, name
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
print('PASS qualified DOS package: source, ZIP, EXE/native hashes and members')
