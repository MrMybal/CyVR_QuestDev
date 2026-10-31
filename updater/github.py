# Copyright (c) 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only
"""GitHub transport: no credential forwarding to signed download origins."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import urllib.error
import urllib.parse
import urllib.request
from common import API,UpdateError,digest

DOWNLOAD_HOSTS={'release-assets.githubusercontent.com','objects.githubusercontent.com','github-releases.githubusercontent.com'}


def allowed_url(url):
    value=urllib.parse.urlsplit(url)
    if value.scheme!='https' or value.username or value.password or value.port not in {None,443} or value.fragment:raise UpdateError('Redirection de téléchargement refusée.')
    if value.hostname not in DOWNLOAD_HOSTS|{'api.github.com'}:raise UpdateError('Serveur de téléchargement refusé.')
    return value


class Redirects(urllib.request.HTTPRedirectHandler):
    max_redirections=5
    def redirect_request(self,request,fp,code,message,headers,newurl):
        target=allowed_url(newurl)
        # A fresh request deliberately omits Authorization on the CDN.
        values={'User-Agent':'CyVR-QuestDev-Updater','Accept':'application/octet-stream'}
        if target.hostname=='api.github.com':
            for key,value in request.header_items():
                if key.lower()=='authorization':values['Authorization']=value
        return urllib.request.Request(newurl,headers=values)


def validate_token(value):
    if not isinstance(value,str) or not 10<=len(value)<=2048 or any(ord(c)<33 or ord(c)>126 for c in value):raise UpdateError('Jeton GitHub invalide.')
    return value


def git_token():
    git=shutil.which('git')
    if not git:return ''
    env=dict(os.environ);env.update(GIT_TERMINAL_PROMPT='0',GCM_INTERACTIVE='Never')
    try:
        result=subprocess.run([git,'credential','fill'],input='protocol=https\nhost=github.com\n\n',capture_output=True,text=True,encoding='utf8',errors='replace',env=env,timeout=15,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
        if result.returncode:return ''
        values=dict(line.split('=',1) for line in result.stdout.splitlines() if '=' in line)
        value=values.get('password','')
        return validate_token(value) if value else ''
    except (OSError,subprocess.TimeoutExpired,UpdateError):return ''


class GitHub:
    def __init__(self,token='',opener=None):
        self.token=validate_token(token) if token else ''
        self.opener=opener or urllib.request.build_opener(Redirects())

    def open(self,path,binary=False):
        if not path.startswith('/releases'):raise UpdateError('Requête GitHub refusée.')
        headers={'User-Agent':'CyVR-QuestDev-Updater','Accept':'application/octet-stream' if binary else 'application/vnd.github+json','X-GitHub-Api-Version':'2022-11-28'}
        if self.token:headers['Authorization']='Bearer '+self.token
        try:return self.opener.open(urllib.request.Request(API+path,headers=headers),timeout=10)
        except urllib.error.HTTPError as e:
            if e.code in {401,404}:raise UpdateError('Accès GitHub refusé : connectez un compte autorisé au dépôt privé (permission Contents : lecture).') from None
            if e.code in {403,429}:raise UpdateError('GitHub limite les requêtes ou refuse l’accès. Vérifiez les permissions puis réessayez plus tard.') from None
            raise UpdateError('GitHub ne répond pas correctement (HTTP '+str(e.code)+').') from None
        except (OSError,urllib.error.URLError):raise UpdateError('Connexion à GitHub impossible. Vérifiez le réseau.') from None

    def releases(self,cancel):
        values=[]
        for page in range(1,11):
            if cancel.is_set():raise UpdateError('Recherche annulée.')
            with self.open(f'/releases?per_page=100&page={page}') as response:
                data=response.read(8*1024*1024+1)
            if len(data)>8*1024*1024:raise UpdateError('Réponse GitHub trop volumineuse.')
            items=json.loads(data)
            if not isinstance(items,list):raise UpdateError('Réponse GitHub invalide.')
            values+=items
            if len(items)<100:return values
        raise UpdateError('Catalogue GitHub trop volumineux.')

    def download(self,release,path,cancel,progress):
        with self.open('/releases/assets/'+str(release['sumId']),True) as response:sums=response.read(65537)
        if len(sums)>65536:raise UpdateError('Sommes de contrôle trop volumineuses.')
        matches=[line.split() for line in sums.decode('ascii').splitlines() if len(line.split())==2 and line.split()[1]==release['name']]
        if len(matches)!=1 or matches[0][0]!=release['sha256']:raise UpdateError('Les sommes de contrôle GitHub ne correspondent pas au paquet.')
        path=Path(path);part=path.with_suffix('.part');received=0
        try:
            with self.open('/releases/assets/'+str(release['assetId']),True) as response,part.open('xb') as output:
                while True:
                    if cancel.is_set():raise UpdateError('Téléchargement annulé.')
                    block=response.read(256*1024)
                    if not block:break
                    received+=len(block)
                    if received>release['size']:raise UpdateError('Téléchargement plus grand que prévu.')
                    output.write(block);progress(received,release['size'])
            if received!=release['size'] or digest(part)!=release['sha256']:raise UpdateError('Téléchargement incomplet ou empreinte SHA-256 incorrecte.')
            part.replace(path)
        finally:part.unlink(missing_ok=True)
