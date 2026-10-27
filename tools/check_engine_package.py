"""Verify the optional engine DOS download against source and native receipts."""
from pathlib import Path
import hashlib
import json
import zipfile

ROOT = Path(__file__).resolve().parents[1]
DIRECTORY = ROOT / 'examples/engine'
manifest = json.loads((DIRECTORY / 'DOS-BUILD.json').read_text())
native = json.loads((DIRECTORY / 'NATIVE.json').read_text())
sha = lambda data: hashlib.sha256(data).hexdigest()
for name, digest in manifest['source_lf_sha256'].items():
    assert sha((ROOT / name).read_bytes().replace(b'\r\n', b'\n')) == digest, name
assert sha((DIRECTORY / 'NATIVE.json').read_bytes().replace(b'\r\n', b'\n')) == \
    manifest['native_lf_sha256']
assert native['complete'] and native['owned_child_stopped'] and native['lease_released']
for run in ('CORE', 'VIEW', 'VIDEO'):
    assert 'FAIL' not in native[run] and ('passed' in native[run] or 'PASS' in native[run]), run
for run in ('G320', 'G160', 'T80', 'T40', 'SLOW'):
    assert 'ticks=300' in native[run] and 'debt=0' in native[run], run
    assert 'xy=330,176 PASS' in native[run], run
archive = DIRECTORY / 'ENGINE.ZIP'
assert sha(archive.read_bytes()) == manifest['zip_sha256'], 'ZIP hash'
with zipfile.ZipFile(archive) as package:
    assert package.testzip() is None
    assert set(package.namelist()) == set(manifest['members']), 'ZIP members'
    for name, expected in manifest['members'].items():
        data = package.read(name)
        assert len(data) == expected['bytes'] and sha(data) == expected['sha256'], name
    for name in ('ENGINE.EXE', 'ENGVIEW.EXE', 'ENGDOS.EXE', 'ENGDEMO.EXE', 'TXTDEMO.EXE'):
        data = package.read(name)
        assert data[:2] == b'MZ', name
        assert sha(data) == native[name]['sha256'], name + ': native hash'
        assert len(data) == native[name]['bytes'], name + ': native size'
    for name, source in manifest['document_members'].items():
        assert package.read(name).replace(b'\r\n', b'\n') == \
            (ROOT / source).read_bytes().replace(b'\r\n', b'\n'), name
    for name in ('README.TXT', 'RUNCHECK.BAT'):
        remainder = package.read(name).replace(b'\r\n', b'')
        assert b'\r' not in remainder and b'\n' not in remainder, name + ': DOS CRLF'
print('PASS qualified engine DOS package: source, native receipt, ZIP, EXEs and docs')
