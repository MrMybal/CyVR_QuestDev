# Copyright (c) 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only

"""Build portable Windows and corresponding-source archives from reviewed files."""
import argparse
import hashlib
import json
import re
from pathlib import Path, PurePosixPath
import shutil
import subprocess
import sys
from urllib.request import urlopen
import zipfile
from setup_mirroring import validate as validate_mirroring, RUNTIME_FILES, SOURCES

ROOT=Path(__file__).resolve().parents[1]
VERSION=(ROOT/'VERSION').read_text(encoding='ascii').strip()
PYTHON_VERSION='3.13.17'
PYTHON_URL=f'https://www.python.org/ftp/python/{PYTHON_VERSION}/python-{PYTHON_VERSION}-embeddable-amd64.zip'
PYTHON_SHA256='0ba1ee4bfcbdc3a559d27e2e9ce76d04a7839947a23f46375872c755c0d463b8'


def git(*args):
    return subprocess.check_output(['git','-c','safe.directory='+ROOT.as_posix(),*args],cwd=ROOT)


def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def copy(source,destination):
    destination.parent.mkdir(parents=True,exist_ok=True)
    shutil.copyfile(source,destination)


def archive(folder,path):
    with zipfile.ZipFile(path,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as out:
        for file in sorted(folder.rglob('*')):
            if file.is_file():out.write(file,folder.name+'/'+file.relative_to(folder).as_posix())


def wheel_file(name):
    part=PurePosixPath(name)
    if part.is_absolute() or '..' in part.parts or '\\' in name:raise RuntimeError('Invalid wheel entry')
    return part


def prepare_runtime(destination,cache):
    embedded=cache/f'python-{PYTHON_VERSION}-embed-amd64.zip'
    if not embedded.exists():
        embedded.parent.mkdir(parents=True,exist_ok=True)
        embedded.write_bytes(urlopen(PYTHON_URL,timeout=60).read())
    if digest(embedded)!=PYTHON_SHA256:raise RuntimeError('Python archive checksum mismatch')
    scripts=destination/'runtime/python/Scripts';scripts.mkdir(parents=True)
    with zipfile.ZipFile(embedded) as source:
        for item in source.infolist():
            part=wheel_file(item.filename)
            if not item.is_dir():
                target=scripts.joinpath(*part.parts);target.parent.mkdir(parents=True,exist_ok=True)
                target.write_bytes(source.read(item))
    wheels=cache/'wheels'
    if not wheels.exists():raise RuntimeError('Download the pinned cp313 Windows wheels first; see docs/RELEASE.md')
    requirements={line.split('==')[0].lower().replace('_','-'):line.split('==')[1] for line in (ROOT/'runtime/requirements.lock').read_text().splitlines() if '==' in line}
    found={};origins=[]
    for file in sorted(wheels.glob('*.whl')):
        name,version=file.name.split('-')[:2];key=name.lower().replace('_','-')
        if key not in requirements or version!=requirements[key]:raise RuntimeError('Unexpected wheel: '+file.name)
        if key in found:raise RuntimeError('Duplicate wheel: '+key)
        found[key]=version;origins.append({'file':file.name,'sha256':digest(file)})
        with zipfile.ZipFile(file) as source:
            for item in source.infolist():
                part=wheel_file(item.filename)
                if item.is_dir() or any(p.lower() in {'tests','test','__pycache__'} for p in part.parts) or part.suffix.lower() in {'.pyc','.pdb','.lib'}:continue
                parts=list(part.parts);base=scripts/'Lib/site-packages'
                if parts[0].endswith('.data'):
                    kind=parts[1]
                    if kind in {'purelib','platlib'}:parts=parts[2:]
                    elif kind=='data':base=scripts;parts=parts[2:]
                    else:continue
                target=base.joinpath(*parts);target.parent.mkdir(parents=True,exist_ok=True)
                target.write_bytes(source.read(item))
    if found!=requirements:raise RuntimeError('Missing pinned wheels: '+str(requirements.keys()-found.keys()))
    (scripts/'python313._pth').write_text('python313.zip\n.\nLib/site-packages\n../../../assistant\nimport site\n',encoding='utf8')
    (destination/'runtime/portable.json').write_text(json.dumps({'python':PYTHON_VERSION,'origin':PYTHON_URL,'sha256':PYTHON_SHA256,'wheels':origins},indent=2),encoding='utf8')


def package(output,cyui):
    validate_mirroring()
    if output.exists():raise RuntimeError('Use a new output directory; existing packages are preserved')
    output.mkdir(parents=True)
    tracked=[p for p in git('ls-files','-z').decode().split('\0') if p]
    commit=git('rev-parse','HEAD').decode().strip()
    product=output/f'CyVR_QuestDev-{VERSION}-Windows-x64';product.mkdir()
    copy(ROOT/'build/windows/CyVR_QuestDev.exe',product/'CyVR_QuestDev.exe')
    for name in tracked:
        include=(name.startswith(('assistant/','updater/','plugins/','docs/','examples/','LICENCES/','licenses/')) and not name.startswith(('assistant/tests/','updater/tests/'))) or (name.startswith('assets/branding/') and Path(name).suffix in {'.png','.ico'}) or name in {'README.md','THIRD_PARTY.md','runtime/requirements.lock','VERSION'}
        if include:copy(ROOT/name,product/name)
    for source,target in [('LICENSE','LICENSE.txt'),('COPYRIGHT','COPYRIGHT.txt')]:copy(ROOT/source,product/target)
    prepare_runtime(product,ROOT/'build/release-cache')
    for name in sorted(RUNTIME_FILES|{'origins.json'}):copy(ROOT/'runtime/mirror'/name,product/'runtime/mirror'/name)
    (product/'DEMARRER.txt').write_text(
        f'CyVR QuestDev {VERSION} — ALPHA\n\n'
        'Windows 10/11 x64. Extraire toute cette archive dans un dossier, puis lancer CyVR_QuestDev.exe.\n'
        'Le runtime Python, CyToolsCore, MCP et les plugins sont inclus. Aucune installation Python séparée n’est nécessaire.\n'
        'Installer les Android SDK Platform-Tools officiels et sélectionner adb.exe dans Réglages si sa détection échoue.\n'
        'Activer le mode développeur du Quest et accepter le débogage USB dans le casque.\n'
        'Assistant IA : choisir un fournisseur et se connecter explicitement. Les CLI des fournisseurs restent externes.\n'
        'OVR Metrics / CyInsightOVR : installer leurs distributions officielles sur le casque.\n\n'
        'Mirroring : Appareil → Mirroring vidéo / audio → Démarrer le mirroring. Moteur scrcpy inclus.\n'
        'Réglages vidéo/audio, F11 plein écran, enregistrement MKV facultatif.\n\n'
        'Mises à jour : Réglages → Mises à jour GitHub. Voir docs/UPDATES.md.\n\n'
        'Version Alpha : voir docs/VALIDATION.md et docs/METRICS.md pour les limites connues.\n'
        'Code source correspondant : archive Source de la même release. Licence GPLv3, notices tierces incluses.\n',encoding='utf8')
    manifest={'version':VERSION,'channel':'alpha','commit':commit,'python':PYTHON_VERSION,'platform':'Windows x64'}
    (product/'RELEASE.json').write_text(json.dumps(manifest,indent=2),encoding='utf8')
    update_files={p.relative_to(product).as_posix():digest(p) for p in sorted(product.rglob('*')) if p.is_file()}
    (product/'UPDATE-MANIFEST.json').write_text(json.dumps({'version':VERSION,'platform':'Windows x64','files':update_files},indent=2),encoding='utf8')
    source=output/f'CyVR_QuestDev-{VERSION}-Source';source.mkdir()
    for name in tracked:copy(ROOT/name,source/'CyVR_QuestDev'/name)
    for name in [item['file'] for item in SOURCES]+['origins.json']:
        copy(ROOT/'runtime/mirror-sources'/name,source/'CyVR_QuestDev/runtime/mirror-sources'/name)
    # Exact CyUI/ImGui source inputs needed to rebuild the statically linked host.
    ui_files=[cyui/'CMakeLists.txt',cyui/'LICENSE',cyui/'README.md']
    for directory in ['include','src']:
        ui_files += [p for p in (cyui/directory).rglob('*') if p.is_file() and p.suffix in {'.h','.hpp','.cpp','.inc'}]
    ui_files += [p for p in (cyui/'third_party/imgui').iterdir() if p.is_file() and p.suffix in {'.h','.cpp','.txt'}]
    for name in ['imgui_impl_win32.h','imgui_impl_win32.cpp','imgui_impl_dx11.h','imgui_impl_dx11.cpp']:
        ui_files.append(cyui/'third_party/imgui/backends'/name)
    for file in ui_files:
        target=source/'CyImgui_Interface'/file.relative_to(cyui);copy(file,target)
        if file.name=='README.md':
            text=re.sub(r'[A-Za-z]:[^\r\n`)]*CyImgui_Interface','path/to/CyImgui_Interface',target.read_text(encoding='utf8'))
            target.write_text(text,encoding='utf8')
    (source/'RELEASE.json').write_text(json.dumps(manifest,indent=2),encoding='utf8')
    (source/'BUILD.txt').write_text('Build Windows x64: ouvrir CyVR_QuestDev, puis exécuter .\\build.ps1 -Test.\nCyImgui_Interface et Dear ImGui sont fournis à côté du projet.\nVoir CyVR_QuestDev/README.md pour Visual Studio et les autres prérequis.\n',encoding='utf8')
    # Hash every source input in the archive, including the linked UI dependency.
    files={p.relative_to(source).as_posix():digest(p) for p in sorted(source.rglob('*')) if p.is_file()}
    (source/'SOURCE-SHA256.json').write_text(json.dumps(files,indent=2),encoding='utf8')
    for folder in [product,source]:archive(folder,output/(folder.name+'.zip'))
    assets=sorted(output.glob('*.zip'))
    (output/'SHA256SUMS.txt').write_text(''.join(digest(p)+'  '+p.name+'\n' for p in assets),encoding='ascii')
    print(json.dumps({'commit':commit,'version':VERSION,'assets':[{'file':p.name,'bytes':p.stat().st_size,'sha256':digest(p)} for p in assets]},indent=2))


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--cyui',type=Path,required=True);args=parser.parse_args()
    package(args.output.resolve(),args.cyui.resolve())
