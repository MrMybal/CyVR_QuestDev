# Copyright (c) 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only
"""Private NDJSON update worker; no tokens are emitted or persisted in JSON."""
import argparse
import json
from pathlib import Path
import sys
import threading
import time
import uuid
sys.path.insert(0,str(Path(__file__).resolve().parent))
import credentials
from common import UpdateError,read_json,write_json,newest,extract_package,validate_install
from github import GitHub,git_token
from install import prepare


def main():
    for stream in (sys.stdin,sys.stdout,sys.stderr):
        if hasattr(stream,'reconfigure'):stream.reconfigure(encoding='utf8',errors='replace')
    parser=argparse.ArgumentParser();parser.add_argument('--data-dir',type=Path,required=True);args=parser.parse_args()
    data=args.data_dir.resolve();data.mkdir(parents=True,exist_ok=True)
    root=Path(__file__).resolve().parent.parent;current=(root/'VERSION').read_text().strip()
    prefs={'channel':'alpha','autoCheck':False,'useGit':True}
    try:
        loaded=read_json(data/'preferences.json',4096)
        if isinstance(loaded,dict):prefs.update({k:v for k,v in loaded.items() if k in prefs})
    except (OSError,ValueError,UpdateError):pass
    prefs={k:v for k,v in prefs.items() if (k=='channel' and isinstance(v,str) and v in {'stable','alpha'}) or (k in {'autoCheck','useGit'} and isinstance(v,bool))}
    prefs={'channel':'alpha','autoCheck':False,'useGit':True,**prefs}
    lock=threading.Lock();cancel=threading.Event();thread=None;release=None;product=None;epoch=0
    def emit(**fields):
        with lock:print(json.dumps({'type':'update_state',**fields},ensure_ascii=False),flush=True)
    def auth():
        try:token=credentials.read()
        except UpdateError:token=''
        return token or (git_token() if prefs['useGit'] else '')
    def previous_version():
        previous=''
        try:
            state=read_json(data/'last-install.json',8192)
            if Path(state['target']).resolve()==root.resolve() and Path(state['backup']).is_dir():previous=state.get('previousVersion','')
        except (OSError,ValueError,KeyError,TypeError,UpdateError):pass
        return previous
    def ready():
        try:saved=bool(credentials.read())
        except UpdateError:saved=False
        emit(phase='idle',busy=False,current=current,prefs=prefs,savedCredential=saved,previousVersion=previous_version(),status='Prêt à rechercher une mise à jour.',error='')
    ready()
    shutdown=threading.Event()
    def watch_receipt():
        previous=None
        while not shutdown.wait(.5):
            try:changed=(data/'last-install.json').stat().st_mtime_ns
            except OSError:changed=0
            if changed!=previous:previous=changed;emit(previousVersion=previous_version())
    watcher=threading.Thread(target=watch_receipt,daemon=True);watcher.start()
    def task(message):
        nonlocal release,product,epoch
        try:
            kind=message['type']
            if kind=='check':
                release=None;product=None;epoch+=1
                emit(phase='checking',busy=True,status='Recherche des releases GitHub…',error='',release=None,downloaded=False)
                release=newest(GitHub(auth()).releases(cancel),current,prefs['channel'])
                emit(phase='available' if release else 'current',busy=False,release=release,status='Nouvelle version disponible.' if release else 'Aucune version plus récente sur ce canal.')
            elif kind=='download':
                if not release:raise UpdateError('Recherchez d’abord une mise à jour.')
                job=data/'downloads'/uuid.uuid4().hex;job.mkdir(parents=True)
                emit(phase='downloading',busy=True,status='Téléchargement et vérification…',error='',progress=0.0)
                last=[0.0]
                def progress(done,total):
                    now=time.monotonic()
                    if now-last[0]>.15:last[0]=now;emit(progress=done/total)
                archive=job/release['name'];GitHub(auth()).download(release,archive,cancel,progress)
                emit(status='Vérification et extraction du paquet…',progress=1.0)
                product=extract_package(archive,job/'extracted',release,cancel)
                emit(phase='downloaded',busy=False,downloaded=True,status='Téléchargement vérifié. Prêt à installer et redémarrer.')
            elif kind in {'install','rollback'}:
                target=Path(message['target']).resolve();validate_install(target)
                source=product
                if kind=='rollback':
                    previous=read_json(data/'last-install.json',8192)
                    if Path(previous['target']).resolve()!=target:raise UpdateError('La sauvegarde appartient à un autre dossier de l’application.')
                    source=Path(previous['backup']);validate_install(source)
                if not source:raise UpdateError('Téléchargez et vérifiez la mise à jour avant de l’installer.')
                emit(phase='preparing',busy=True,status='Préparation de l’installation et de la sauvegarde…',error='')
                prepared=prepare(target,source,data,sys.executable,message['parentPid'],message['parentCreated'],kind=='rollback')
                emit(phase='prepared',busy=True,prepared=prepared,status='Installation préparée. Fermeture de l’application…')
            else:raise UpdateError('Action de mise à jour inconnue.')
        except Exception as error:
            text=str(error) if isinstance(error,UpdateError) else 'Opération impossible. Vérifiez le réseau, l’espace disque et les droits du dossier, puis réessayez.'
            emit(phase='error',busy=False,error=text,status='Opération interrompue.')
    try:
        for line in sys.stdin:
            if len(line)>16384:continue
            try:
                message=json.loads(line);kind=message.get('type')
                if kind=='shutdown':cancel.set();break
                if kind=='cancel':cancel.set();continue
                if thread and thread.is_alive():continue
                if kind=='save_token':credentials.store(message.get('token',''));ready();continue
                if kind=='forget_token':credentials.forget();ready();continue
                if kind=='preferences':
                    values=message.get('values',{})
                    if values.get('channel',prefs['channel']) not in {'stable','alpha'}:raise UpdateError('Canal invalide.')
                    for key in ['autoCheck','useGit']:
                        if key in values and not isinstance(values[key],bool):raise UpdateError('Préférence invalide.')
                    prefs.update({k:v for k,v in values.items() if k in prefs});write_json(data/'preferences.json',prefs);release=None;product=None;ready();emit(release=None,downloaded=False);continue
                cancel.clear();thread=threading.Thread(target=task,args=(message,),daemon=True);thread.start()
            except Exception as error:emit(phase='error',busy=False,error=str(error) if isinstance(error,UpdateError) else 'Commande de mise à jour invalide.')
    finally:
        shutdown.set();watcher.join(timeout=1)
        cancel.set()
        if thread:thread.join(timeout=9)


if __name__=='__main__':main()
