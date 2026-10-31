# Copyright (c) 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only
"""Compile a per-user installer from a complete, hash-verified release tree."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'updater'))
from common import read_json, verify_tree


def build(product, output, compiler, test=False):
    product, output, compiler = product.resolve(), output.resolve(), compiler.resolve()
    version = read_json(product / 'RELEASE.json')['version']
    verify_tree(product, version)
    expected = set(read_json(product / 'UPDATE-MANIFEST.json')['files']) | {'UPDATE-MANIFEST.json'}
    names = (product / 'INSTALL-FILES.txt').read_text(encoding='utf-8-sig').splitlines()
    if len(names) != len(set(names)) or set(names) != expected:
        raise RuntimeError('Uninstall file inventory does not match the verified payload')
    if version != (ROOT / 'VERSION').read_text().strip():
        raise RuntimeError('Product version does not match installer sources')
    output.mkdir(parents=True, exist_ok=True)
    target = output / f'CyVR_QuestDev-{version}-Windows-x64-Setup.exe'
    if target.exists():
        raise RuntimeError('Installer already exists; use a new output directory')
    numeric = version.split('-', 1)[0] + '.0'
    command = [str(compiler), '/Qp', f'/DProductDir={product}', f'/DAppVersion={version}', f'/DNumericVersion={numeric}', f'/DOutputDir={output}']
    if test:
        command.append('/DTestBuild=1')
    subprocess.run([*command, str(ROOT / 'installer/questdev.iss')], cwd=ROOT, check=True)
    if not test:
        assets = sorted([*output.glob('*.zip'), target])
        (output / 'SHA256SUMS.txt').write_text(''.join(hashlib.sha256(p.read_bytes()).hexdigest() + '  ' + p.name + '\n' for p in assets), encoding='ascii')
    print(json.dumps({'file': target.name, 'bytes': target.stat().st_size, 'sha256': hashlib.sha256(target.read_bytes()).hexdigest()}))
    return target


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--product', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--iscc', type=Path, default=ROOT / 'build/toolchain/inno-6.7.4/ISCC.exe')
    parser.add_argument('--test', action='store_true', help='Separate test registration and shortcuts')
    args = parser.parse_args()
    build(args.product, args.output, args.iscc, args.test)
