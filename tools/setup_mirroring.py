# Copyright (c) 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only
"""Install the pinned official scrcpy runtime and retain corresponding sources."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import tarfile
from urllib.request import urlopen
import zipfile

ROOT=Path(__file__).resolve().parents[1]
VERSION='5.0.1'
BINARY={
    'file':f'scrcpy-win64-v{VERSION}.zip',
    'url':f'https://github.com/Genymobile/scrcpy/releases/download/v{VERSION}/scrcpy-win64-v{VERSION}.zip',
    'sha256':'b12a2c4ee8be317422451fc7dcf8ee20a71b5ea7ef9ad73ddd825a25316227a5',
}
SOURCES=[
    {'file':'scrcpy-5.0.1.tar.gz','url':'https://github.com/Genymobile/scrcpy/archive/refs/tags/v5.0.1.tar.gz','sha256':'a24b996ac23d0f674d3237c00b39a97829d8acbe9e1657a6c216fd51ea488ee5','license':'scrcpy-5.0.1/LICENSE','notice':'scrcpy-Apache-2.0.txt'},
    {'file':'ffmpeg-9.0.2.tar.xz','url':'https://ffmpeg.org/releases/ffmpeg-9.0.2.tar.xz','sha256':'8c3850283eb25fa026482078a04051e0be17347b09ef81a0849bec15a96e002e','license':'ffmpeg-9.0.2/COPYING.LGPLv2.1','notice':'FFmpeg-LGPL-2.1.txt'},
    {'file':'sdl-3.4.18.tar.gz','url':'https://github.com/libsdl-org/SDL/archive/refs/tags/release-3.4.18.tar.gz','sha256':'c4b08b950bd29d83caae0ab8d884298d27046a9d30700fb42841c50992665c90','license':'SDL-release-3.4.18/LICENSE.txt','notice':'SDL-zlib.txt'},
    {'file':'libusb-1.0.30.tar.gz','url':'https://github.com/libusb/libusb/archive/refs/tags/v1.0.30.tar.gz','sha256':'2ae28adb0bb9558c86135c4e1c11b320b0805461e207a64a6e520a114094bf07','license':'libusb-1.0.30/COPYING','notice':'libusb-LGPL-2.1.txt'},
    {'file':'dav1d-1.5.4.tar.xz','url':'https://downloads.videolan.org/pub/videolan/dav1d/1.5.4/dav1d-1.5.4.tar.xz','sha256':'686616b7c69eb88d44459391ab25cac13b6647a3b288835c5784e71c1514a5c5','license':'dav1d-1.5.4/COPYING','notice':'dav1d-BSD.txt'},
]
RUNTIME_FILES={'scrcpy.exe','scrcpy-server','avcodec-63.dll','avformat-63.dll','avutil-61.dll','swresample-7.dll','SDL3.dll','libusb-1.0.dll','scrcpy.png','disconnected.png','LICENSE.txt'}


def digest(file):return hashlib.sha256(file.read_bytes()).hexdigest()


def download(item,folder,offline):
    file=folder/item['file'];folder.mkdir(parents=True,exist_ok=True)
    if not file.exists():
        if offline:raise RuntimeError('Missing cached archive: '+file.name)
        data=urlopen(item['url'],timeout=60).read()
        if hashlib.sha256(data).hexdigest()!=item['sha256']:raise RuntimeError('Download checksum mismatch: '+file.name)
        file.write_bytes(data)
    if digest(file)!=item['sha256']:raise RuntimeError('Cached checksum mismatch: '+file.name)
    return file


def validate(root=ROOT):
    runtime=root/'runtime/mirror';metadata=json.loads((runtime/'origins.json').read_text(encoding='utf8'))
    if metadata['archive']!=BINARY:raise RuntimeError('Unrecognized mirroring origin')
    if set(metadata['files'])!=RUNTIME_FILES:raise RuntimeError('Mirroring runtime inventory differs')
    for name,expected in metadata['files'].items():
        if digest(runtime/name)!=expected:raise RuntimeError('Modified mirroring runtime: '+name)
    for item in SOURCES:
        if digest(root/'runtime/mirror-sources'/item['file'])!=item['sha256']:raise RuntimeError('Modified corresponding source: '+item['file'])
    return metadata


def setup(offline=False):
    runtime=ROOT/'runtime/mirror';runtime.mkdir(parents=True,exist_ok=True)
    source=ROOT/'runtime/mirror-sources'
    with ThreadPoolExecutor(max_workers=3) as pool:
        paths=list(pool.map(lambda item:download(item,source,offline),SOURCES))
    binary=download(BINARY,ROOT/'build/mirroring-cache',offline)
    found=set()
    with zipfile.ZipFile(binary) as archive:
        for item in archive.infolist():
            name=item.filename.rsplit('/',1)[-1]
            if name in RUNTIME_FILES:
                if name in found:raise RuntimeError('Duplicate runtime file')
                found.add(name);(runtime/name).write_bytes(archive.read(item))
    if found!=RUNTIME_FILES:raise RuntimeError('Incomplete official runtime archive')
    notices=ROOT/'LICENCES';notices.mkdir(exist_ok=True)
    for item,file in zip(SOURCES,paths):
        with tarfile.open(file) as archive:
            data=archive.extractfile(item['license']).read()
            (notices/item['notice']).write_bytes(data)
    origins=[{key:item[key] for key in ('file','url','sha256')} for item in SOURCES]
    (source/'origins.json').write_text(json.dumps(origins,indent=2)+'\n',encoding='utf8')
    metadata={'version':VERSION,'archive':BINARY,'files':{name:digest(runtime/name) for name in sorted(found)},'sources':origins}
    (runtime/'origins.json').write_text(json.dumps(metadata,indent=2)+'\n',encoding='utf8')
    validate();print('Official scrcpy runtime, notices and corresponding sources verified.')


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--offline',action='store_true');args=parser.parse_args()
    setup(args.offline)
