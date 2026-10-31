# Copyright (c) 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only
"""Version selection and bounded, verified portable packages. Standard library only."""
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import stat
import zipfile

REPO='MrMybal/CyVR_QuestDev'
API='https://api.github.com/repos/'+REPO
VERSION_RE=re.compile(r'^v?(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)(?:-([0-9A-Za-z.-]+))?$')
SHA_RE=re.compile(r'^[0-9a-f]{64}$')


class UpdateError(Exception):pass


def version(value):
    match=VERSION_RE.fullmatch(value)
    if not match:raise UpdateError('Version non reconnue.')
    major,minor,patch,pre=match.groups();parts=[]
    if pre:
        for part in pre.split('.'):
            if not part or (part.isdigit() and len(part)>1 and part[0]=='0'):raise UpdateError('Préversion non reconnue.')
            parts.append((0,int(part)) if part.isdigit() else (1,part))
    return int(major),int(minor),int(patch),1 if pre is None else 0,tuple(parts)


def newest(releases,current,channel):
    choices=[];installed=version(current)
    if channel not in {'stable','alpha'}:raise UpdateError('Canal inconnu.')
    for release in releases:
        if release.get('draft') or not release.get('published_at'):continue
        try:
            tag=release['tag_name'];key=version(tag)
            if key<=installed or (channel=='stable' and (release.get('prerelease') or not key[3])):continue
            name=f'CyVR_QuestDev-{tag.removeprefix("v")}-Windows-x64.zip'
            assets=release.get('assets',[])
            files=[a for a in assets if a.get('name')==name and a.get('state')=='uploaded']
            sums=[a for a in assets if a.get('name')=='SHA256SUMS.txt' and a.get('state')=='uploaded']
            if len(files)!=1 or len(sums)!=1:continue
            asset=files[0];digest=asset.get('digest','')
            if not digest.startswith('sha256:') or not SHA_RE.fullmatch(digest[7:]):continue
            if not 0<int(asset['size'])<=256*1024*1024:continue
            choices.append((key,{'version':tag.removeprefix('v'),'tag':tag,'assetId':int(asset['id']),'sumId':int(sums[0]['id']),
                'name':name,'size':int(asset['size']),'sha256':digest[7:],'notes':str(release.get('body') or '')[:16000],
                'url':'https://github.com/'+REPO+'/releases/tag/'+tag}))
        except (KeyError,ValueError,TypeError,UpdateError):continue
    return max(choices,key=lambda x:x[0])[1] if choices else None


def digest(path):
    sha=hashlib.sha256()
    with Path(path).open('rb') as file:
        for block in iter(lambda:file.read(1024*1024),b''):sha.update(block)
    return sha.hexdigest()


def write_json(path,value):
    path=Path(path);path.parent.mkdir(parents=True,exist_ok=True)
    temp=path.with_name(path.name+'.tmp');temp.write_text(json.dumps(value,ensure_ascii=False,indent=2),encoding='utf8');temp.replace(path)


def read_json(path,limit=4*1024*1024):
    if Path(path).stat().st_size>limit:raise UpdateError('Métadonnées trop volumineuses.')
    return json.loads(Path(path).read_text(encoding='utf8'))


def relative_path(name):
    part=PurePosixPath(name)
    if not name or '\\' in name or ':' in name or part.is_absolute() or any(x in {'..','.'} for x in name.split('/')):
        raise UpdateError('Chemin refusé dans le paquet.')
    for x in part.parts:
        if not x or x.endswith((' ','.')) or any(ord(c)<32 for c in x):raise UpdateError('Nom de fichier refusé.')
        stem=x.split('.')[0].upper()
        if stem in {'CON','PRN','AUX','NUL'} or re.fullmatch(r'(COM|LPT)[1-9]',stem) or x.lower()=='.git':raise UpdateError('Nom Windows réservé.')
    return part


def regular_tree(root):
    root=Path(root)
    def inspect(file):
        if file.name.lower()=='.git':raise UpdateError('Une copie de sources ne peut pas être remplacée automatiquement.')
        info=file.lstat()
        if stat.S_ISLNK(info.st_mode) or getattr(info,'st_file_attributes',0)&0x400:raise UpdateError('Liens et jonctions refusés pour la mise à jour.')
        if not (stat.S_ISREG(info.st_mode) or stat.S_ISDIR(info.st_mode)):raise UpdateError('Fichier spécial refusé.')
    inspect(root);files=[]
    for folder,dirs,names in os.walk(root,followlinks=False):
        for name in dirs:inspect(Path(folder)/name)
        for name in names:
            file=Path(folder)/name;inspect(file);files.append(file)
    return files


def validate_install(root):
    root=Path(root).resolve()
    protected=[]
    for key in ['USERPROFILE','LOCALAPPDATA','APPDATA','SystemRoot','ProgramFiles','ProgramFiles(x86)','ProgramData','PUBLIC']:
        if os.environ.get(key):protected.append(Path(os.environ[key]).resolve())
    if os.environ.get('USERPROFILE'):
        protected += [Path(os.environ['USERPROFILE'])/name for name in ['Desktop','Documents','Downloads','Pictures','Videos','Music']]
    if any(root==p or root in p.parents for p in protected):raise UpdateError('Extrayez QuestDev dans son propre dossier avant la mise à jour.')
    if root==Path(root.anchor) or (root/'.git').exists() or (root/'CMakeLists.txt').exists():raise UpdateError('Utilisez une distribution portable pour installer les mises à jour.')
    meta=read_json(root/'RELEASE.json')
    if meta.get('platform')!='Windows x64' or not (root/'CyVR_QuestDev.exe').is_file():raise UpdateError('Dossier portable QuestDev non reconnu.')
    version(meta['version'])
    regular_tree(root)
    return root,meta


def verify_tree(root,expected_version):
    root=Path(root);manifest=read_json(root/'UPDATE-MANIFEST.json')
    if manifest.get('version')!=expected_version or manifest.get('platform')!='Windows x64':raise UpdateError('Manifeste de mise à jour incompatible.')
    files=manifest.get('files',{})
    if not isinstance(files,dict) or not 1<=len(files)<=16000:raise UpdateError('Inventaire de mise à jour invalide.')
    actual={p.relative_to(root).as_posix() for p in regular_tree(root)}
    if actual!=set(files)|{'UPDATE-MANIFEST.json'}:raise UpdateError('Inventaire du paquet incomplet ou inattendu.')
    for name,sha in files.items():
        part=relative_path(name)
        if not isinstance(sha,str) or not SHA_RE.fullmatch(sha) or digest(root.joinpath(*part.parts))!=sha:raise UpdateError('Un fichier du paquet a été altéré.')
    meta=read_json(root/'RELEASE.json')
    if meta.get('version')!=expected_version or meta.get('platform')!='Windows x64':raise UpdateError('Version interne du paquet incorrecte.')
    for name in ['CyVR_QuestDev.exe','VERSION','updater/service.py','updater/apply.py','runtime/python/Scripts/python.exe']:
        if name not in files:raise UpdateError('Composant nécessaire absent du paquet.')
    return manifest


def extract_package(archive,destination,release,cancel=None):
    archive=Path(archive);destination=Path(destination)
    if archive.stat().st_size!=release['size'] or digest(archive)!=release['sha256']:raise UpdateError('L’empreinte SHA-256 du téléchargement est incorrecte.')
    prefix=f'CyVR_QuestDev-{release["version"]}-Windows-x64/'
    with zipfile.ZipFile(archive) as source:
        entries=source.infolist();seen=set();total=0
        if not 1<=len(entries)<=16000:raise UpdateError('Nombre de fichiers du ZIP refusé.')
        for item in entries:
            if not item.filename.startswith(prefix):raise UpdateError('Racine du ZIP incorrecte.')
            relative_path(item.filename)
            mode=item.external_attr>>16
            if stat.S_IFMT(mode) not in {0,stat.S_IFREG,stat.S_IFDIR} or item.flag_bits&1:raise UpdateError('Fichier ZIP spécial ou chiffré refusé.')
            if item.compress_type not in {zipfile.ZIP_STORED,zipfile.ZIP_DEFLATED}:raise UpdateError('Compression ZIP non prise en charge.')
            key=item.filename.casefold()
            if key in seen:raise UpdateError('Chemins dupliqués dans le ZIP.')
            seen.add(key);total+=item.file_size
            if item.file_size>256*1024*1024 or total>1024*1024*1024:raise UpdateError('Taille décompressée trop grande.')
        destination.mkdir(parents=True,exist_ok=False)
        for item in entries:
            if cancel and cancel.is_set():raise UpdateError('Téléchargement annulé.')
            target=destination.joinpath(*PurePosixPath(item.filename).parts)
            if item.is_dir():target.mkdir(parents=True,exist_ok=True);continue
            target.parent.mkdir(parents=True,exist_ok=True)
            with source.open(item) as incoming,target.open('xb') as outgoing:
                for block in iter(lambda:incoming.read(1024*1024),b''):
                    if cancel and cancel.is_set():raise UpdateError('Téléchargement annulé.')
                    outgoing.write(block)
    product=destination/prefix.rstrip('/');verify_tree(product,release['version']);return product
