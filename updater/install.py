# Copyright (c) 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only
"""Prepare a sibling tree, then replace only after the native owner exits."""
import ctypes
from ctypes import wintypes as W
import os
from pathlib import Path
import shutil
import subprocess
import time
import uuid
from common import UpdateError,digest,read_json,write_json,regular_tree,validate_install,verify_tree


def snapshot(root):return {p.relative_to(root).as_posix():digest(p) for p in regular_tree(root)}


def prepare(target,source,data,python,parent_pid,parent_created,rollback=False):
    target,current=validate_install(target);source=Path(source).resolve();data=Path(data).resolve()
    if data==target or target in data.parents:raise UpdateError('Le dossier des données de mise à jour doit rester hors du dossier de l’application.')
    regular_tree(source);new=read_json(source/'RELEASE.json')
    if not rollback:verify_tree(source,new['version'])
    identifier=uuid.uuid4().hex;work=data/'jobs'/identifier;work.mkdir(parents=True)
    stage=target.parent/('.'+target.name+'-update-'+identifier)
    backup=target.parent/('.'+target.name+'-backup-'+identifier)
    if stage.exists() or backup.exists():raise UpdateError('Dossier de préparation déjà présent.')
    try:
        shutil.copytree(source,stage)
        old_manifest=read_json(target/'UPDATE-MANIFEST.json');old=old_manifest['files'];managed=read_json(stage/'UPDATE-MANIFEST.json')['files']
        personal_bytes=0;personal_count=0
        for file in regular_tree(target):
            name=file.relative_to(target).as_posix()
            if '__pycache__' in file.parts or file.suffix=='.pyc':continue
            if name=='UPDATE-MANIFEST.json':continue
            if name in old:
                if not rollback and digest(file)!=old[name]:raise UpdateError('Des fichiers de l’application ont été modifiés. Utilisez un dossier portable intact pour installer automatiquement.')
                continue
            if name in managed:raise UpdateError('Un fichier personnel porte le nom d’un nouveau composant. Déplacez-le avant l’installation.')
            personal_bytes+=file.stat().st_size;personal_count+=1
            if personal_bytes>128*1024*1024 or personal_count>2048:raise UpdateError('Le dossier contient trop de fichiers personnels. Utilisez un dossier dédié à l’application.')
            destination=stage/name;destination.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(file,destination)
        bootstrap=work/'bootstrap';bootstrap.mkdir()
        python=Path(python).resolve();base=python.parent
        if not any(base.glob('python3*.zip')):raise UpdateError('Le runtime portable est requis pour installer les mises à jour.')
        for file in base.iterdir():
            if file.is_file() and file.suffix.lower() in {'.dll','.pyd','.zip','.exe'}:shutil.copyfile(file,bootstrap/file.name)
        for file in Path(__file__).parent.glob('*.py'):shutil.copyfile(file,bootstrap/file.name)
        library=next(bootstrap.glob('python3*.zip')).name
        (bootstrap/(library.removesuffix('.zip')+'._pth')).write_text('.\n'+library+'\n',encoding='ascii')
        value={'id':identifier,'target':str(target),'stage':str(stage),'backup':str(backup),'data':str(data),'work':str(work),
            'parentPid':int(parent_pid),'parentCreated':int(parent_created),'from':current['version'],'version':new['version'],
            'files':snapshot(stage),'rollback':rollback,'ready':str(work/'ready.json'),'health':str(work/'health.json')}
        write_json(work/'plan.json',value)
        return {'plan':str(work/'plan.json'),'python':str(bootstrap/'python.exe'),'script':str(bootstrap/'apply.py'),'ready':value['ready']}
    except Exception:
        if stage.exists():
            if stage.resolve().parent!=target.parent or stage.resolve().name!=stage.name:raise UpdateError('Dossier de préparation déplacé ; nettoyage automatique refusé.')
            shutil.rmtree(stage)
        raise


def parent_handle(plan):
    kernel=ctypes.WinDLL('kernel32',use_last_error=True)
    kernel.OpenProcess.argtypes=[W.DWORD,W.BOOL,W.DWORD];kernel.OpenProcess.restype=W.HANDLE
    kernel.QueryFullProcessImageNameW.argtypes=[W.HANDLE,W.DWORD,W.LPWSTR,ctypes.POINTER(W.DWORD)]
    kernel.GetProcessTimes.argtypes=[W.HANDLE,*([ctypes.POINTER(W.FILETIME)]*4)]
    kernel.WaitForSingleObject.argtypes=[W.HANDLE,W.DWORD];kernel.CloseHandle.argtypes=[W.HANDLE]
    handle=kernel.OpenProcess(0x100000|0x1000,False,plan['parentPid'])
    if not handle:raise UpdateError('L’application à mettre à jour n’est plus active.')
    try:
        name=ctypes.create_unicode_buffer(32768);size=W.DWORD(len(name))
        created=W.FILETIME();exited=W.FILETIME();cpu=W.FILETIME();user=W.FILETIME()
        if not kernel.QueryFullProcessImageNameW(handle,0,name,ctypes.byref(size)) or not kernel.GetProcessTimes(handle,ctypes.byref(created),ctypes.byref(exited),ctypes.byref(cpu),ctypes.byref(user)):
            raise UpdateError('Impossible de vérifier l’application ouverte.')
        stamp=(created.dwHighDateTime<<32)|created.dwLowDateTime
        if Path(name.value).resolve()!=Path(plan['target'])/'CyVR_QuestDev.exe' or stamp!=plan['parentCreated']:raise UpdateError('L’identité du processus a changé.')
        return kernel,handle
    except Exception:kernel.CloseHandle(handle);raise


def apply(plan_path):
    plan_path=Path(plan_path).resolve();plan=read_json(plan_path);target=Path(plan['target']);stage=Path(plan['stage']);backup=Path(plan['backup']);work=plan_path.parent
    if work!=Path(plan['work']) or work.name!=plan['id'] or not all(c in '0123456789abcdef' for c in plan['id']) or len(plan['id'])!=32:raise UpdateError('Plan de mise à jour invalide.')
    if stage!=target.parent/('.'+target.name+'-update-'+plan['id']) or backup!=target.parent/('.'+target.name+'-backup-'+plan['id']):raise UpdateError('Destinations de mise à jour invalides.')
    if Path(plan['ready'])!=work/'ready.json' or Path(plan['health'])!=work/'health.json':raise UpdateError('Accusé de réception invalide.')
    target,current=validate_install(target)
    if current['version']!=plan['from'] or backup.exists() or snapshot(stage)!=plan['files']:raise UpdateError('La préparation a changé depuis sa validation.')
    kernel,handle=parent_handle(plan);moved=False;installed=False;parent_exited=False;child=None
    with (work/'claimed').open('x'):pass
    try:
        write_json(work/'ready.json',{'ok':True})
        if kernel.WaitForSingleObject(handle,120000)!=0:raise UpdateError('L’application ne s’est pas fermée ; installation annulée.')
        parent_exited=True
        # Revalidate after waiting, before the first mutation of the old install.
        if snapshot(stage)!=plan['files']:raise UpdateError('Les fichiers préparés ont changé.')
        rename_wait(target,backup);moved=True;rename_wait(stage,target);installed=True
        env=dict(os.environ)
        for key in ['PYTHONHOME','PYTHONPATH','VIRTUAL_ENV']:env.pop(key,None)
        child=subprocess.Popen([str(target/'CyVR_QuestDev.exe'),'--update-ready-file',str(work/'health.json')],cwd=target,env=env,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
        deadline=time.monotonic()+35
        while time.monotonic()<deadline:
            if child.poll() is not None:raise UpdateError('La nouvelle version s’est arrêtée au démarrage.')
            if (work/'health.json').exists():
                health=read_json(work/'health.json',4096)
                if health.get('ok') and health.get('version')==plan['version'] and health.get('pid')==child.pid:break
                raise UpdateError('La nouvelle version n’a pas confirmé son démarrage.')
            time.sleep(.1)
        else:raise UpdateError('La nouvelle version ne répond pas ; restauration de la sauvegarde.')
        result={'ok':True,'version':plan['version'],'target':str(target),'backup':str(backup),'previousVersion':plan['from']}
        write_json(Path(plan['data'])/'last-install.json',result);write_json(work/'result.json',result)
    except Exception:
        if child and child.poll() is None:child.terminate();child.wait(timeout=10)
        if installed:rename_wait(target,target.parent/('.'+target.name+'-failed-'+plan['id']))
        if moved:
            rename_wait(backup,target)
        if parent_exited:
            subprocess.Popen([str(target/'CyVR_QuestDev.exe')],cwd=target,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
        raise
    finally:kernel.CloseHandle(handle)


def rename_wait(source,destination):
    deadline=time.monotonic()+8
    while True:
        try:source.rename(destination);return
        except PermissionError:
            if time.monotonic()>=deadline:raise
            time.sleep(.2)
