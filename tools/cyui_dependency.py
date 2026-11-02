# Copyright (c) 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only
"""Pinned CyUI provenance and complete corresponding UI sources, without a copy in the repository."""
import hashlib
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
REVISION = re.search(r'QUESTDEV_CYUI_REVISION "([0-9a-f]{40})"', (ROOT/'cmake/CyUI.cmake').read_text())[1]


def materialize(checkout, destination):
    """Read the immutable Git object; never depend on or alter a developer's worktree."""
    import io
    import zipfile
    checkout, destination = Path(checkout).resolve(), Path(destination).resolve()
    if destination.exists():
        source_files(destination)
        return destination
    git = ['git','-c','safe.directory='+checkout.as_posix(),'-C',str(checkout)]
    content = subprocess.check_output([*git,'archive','--format=zip',REVISION])
    destination.mkdir(parents=True, exist_ok=False)
    hashes = {}
    with zipfile.ZipFile(io.BytesIO(content)) as archive:
        for entry in archive.infolist():
            if entry.is_dir():
                continue
            name = entry.filename
            target = (destination/name).resolve()
            if not target.is_relative_to(destination):
                raise RuntimeError('Invalid CyUI archive path')
            if not (name.startswith(('src/','include/','data/','third_party/')) or name in {'CMakeLists.txt','LICENSE','README.md','THIRD_PARTY_NOTICES.md'}):
                continue
            data = archive.read(entry)
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
            hashes[name] = hashlib.sha256(data).hexdigest()
    (destination/'.cyui-source-revision').write_text(REVISION+'\n')
    (destination/'.cyui-source-manifest.json').write_text(json.dumps({'revision':REVISION,'files':hashes},indent=2))
    return destination


def source_files(folder):
    folder = Path(folder).resolve()
    provenance = folder/'.cyui-source-manifest.json'
    if provenance.exists():
        metadata = json.loads(provenance.read_text())
        if metadata['revision'] != REVISION or (folder/'.cyui-source-revision').read_text().strip() != REVISION:
            raise RuntimeError('CyUI source revision mismatch')
        names = list(metadata['files'])
        observed = {p.relative_to(folder).as_posix() for p in folder.rglob('*') if p.is_file()}
        observed -= {'.cyui-source-revision', '.cyui-source-manifest.json'}
        if observed != set(names):
            raise RuntimeError('CyUI source snapshot file list changed')
        for name, expected in metadata['files'].items():
            path = (folder/name).resolve()
            if not path.is_relative_to(folder) or hashlib.sha256(path.read_bytes()).hexdigest() != expected:
                raise RuntimeError('CyUI source snapshot changed: '+name)
        return names
    git = ['git', '-c', 'safe.directory='+folder.as_posix(), '-C', str(folder)]
    actual = subprocess.check_output([*git, 'rev-parse', 'HEAD'], text=True).strip()
    if actual != REVISION or subprocess.check_output([*git, 'status', '--porcelain', '--untracked-files=all']):
        raise RuntimeError('Use a clean checkout of the pinned CyUI revision '+REVISION)
    names = subprocess.check_output([*git, 'ls-files', '-z']).decode().split('\0')
    return [name for name in names if name and (name.startswith(('src/','include/','data/','third_party/')) or name in {'CMakeLists.txt','LICENSE','README.md','THIRD_PARTY_NOTICES.md'})]


def snapshot(folder, destination):
    import shutil
    folder, destination = Path(folder), Path(destination)
    names = source_files(folder)
    hashes = {}
    for name in names:
        target = destination/name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(folder/name, target)
        hashes[name] = hashlib.sha256(target.read_bytes()).hexdigest()
    (destination/'.cyui-source-revision').write_text(REVISION+'\n')
    (destination/'.cyui-source-manifest.json').write_text(json.dumps({'revision':REVISION,'files':hashes},indent=2))
    return hashes
