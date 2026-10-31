# Copyright (c) 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only
"""Install a pinned official Inno compiler in build/, in its portable mode."""
import hashlib
from pathlib import Path
import shutil
import subprocess
from urllib.request import urlopen

ROOT = Path(__file__).resolve().parents[1]
VERSION = '6.7.4'
SHA256 = 'a549929319f9120716677c685535ca4671e4b937b5fdc87e2585f32d8eb30e8c'
URL = f'https://github.com/jrsoftware/issrc/releases/download/is-6_7_4/innosetup-{VERSION}.exe'


def setup():
    cache = ROOT / 'build/toolchain'
    cache.mkdir(parents=True, exist_ok=True)
    archive = cache / f'innosetup-{VERSION}.exe'
    if not archive.exists():
        archive.write_bytes(urlopen(URL, timeout=45).read())
    if hashlib.sha256(archive.read_bytes()).hexdigest() != SHA256:
        raise RuntimeError('Inno Setup download checksum mismatch')
    # Do not execute a downloaded compiler installer without checking its signer.
    quoted = "'" + str(archive).replace("'", "''") + "'"
    script = "$s=Get-AuthenticodeSignature -LiteralPath " + quoted + "; if($s.Status -ne 'Valid' -or $s.SignerCertificate.Subject -notlike '*CN=Pyrsys B.V.,*') {exit 1}"
    subprocess.run([shutil.which('pwsh') or 'powershell.exe', '-NoProfile', '-Command', script], check=True)
    destination = cache / f'inno-{VERSION}'
    subprocess.run([str(archive), '/CURRENTUSER', '/PORTABLE=1', '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/SP-', f'/DIR={destination}', '/NOICONS', '/TASKS='], check=True, creationflags=subprocess.CREATE_NO_WINDOW)
    if not (destination / 'ISCC.exe').is_file():
        raise RuntimeError('Inno Setup compiler is missing')
    print(destination / 'ISCC.exe')
    return destination / 'ISCC.exe'


if __name__ == '__main__':
    setup()
