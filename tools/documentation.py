# Copyright (c) 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only
"""Build the genuine CyDoc reader used by the app and its website."""
from pathlib import Path
import os
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def build_documentation():
    workspace = ROOT / '.CyDocBuild'
    node = shutil.which('node')
    if not node:
        raise RuntimeError('Node.js 22+ is required to generate the manual. Installed readers need no Node.js.')
    version = subprocess.check_output([node, '--version'], text=True).strip()
    if int(version.lstrip('v').split('.')[0]) < 22:
        raise RuntimeError('The documentation build requires Node.js 22 or later.')
    if not (workspace / 'node_modules/cydoc/tools/cli.mjs').is_file():
        npm = shutil.which('npm.cmd' if os.name == 'nt' else 'npm')
        if not npm:
            raise RuntimeError('Run npm ci --prefix .CyDocBuild to install the pinned documentation compiler.')
        subprocess.run([npm, 'ci', '--prefix', str(workspace), '--cache', str(workspace / '.cache/npm'), '--no-audit', '--no-fund'], cwd=ROOT, check=True)
    subprocess.run([node, str(workspace / 'build.mjs')], cwd=ROOT, check=True)
    return ROOT / 'Documentation'


if __name__ == '__main__':
    print(build_documentation())
