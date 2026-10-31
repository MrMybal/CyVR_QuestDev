# Copyright (c) 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only
import ctypes
import hashlib
import io
import json
import os
import queue
from pathlib import Path
import shutil
import stat
import subprocess
import sys
import tempfile
import threading
import time
import unittest
from unittest.mock import patch
import urllib.request
import zipfile

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'updater'))
from common import UpdateError,version,newest,digest,extract_package,verify_tree,write_json,read_json
from github import Redirects,GitHub
import install


def product(folder,number='0.7.0-alpha.1',failure=False):
    folder.mkdir(parents=True)
    files={'VERSION':number.encode(),'updater/service.py':b'# fixture\n','updater/apply.py':b'# fixture\n','runtime/python/Scripts/python.exe':b'fixture runtime','old-component.txt':b'original'}
    fixture=ROOT/'build/windows/fake_update_app.exe'
    files['CyVR_QuestDev.exe']=fixture.read_bytes() if fixture.exists() else b'fixture executable'
    if failure:files['FAIL_START']=b'fixture failure'
    for name,data in files.items():
        p=folder/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(data)
    write_json(folder/'RELEASE.json',{'version':number,'platform':'Windows x64'})
    hashes={p.relative_to(folder).as_posix():digest(p) for p in folder.rglob('*') if p.is_file()}
    write_json(folder/'UPDATE-MANIFEST.json',{'version':number,'platform':'Windows x64','files':hashes})
    return folder


def zip_product(folder,path,extra=None):
    prefix='CyVR_QuestDev-'+read_json(folder/'RELEASE.json')['version']+'-Windows-x64/'
    with zipfile.ZipFile(path,'w',zipfile.ZIP_DEFLATED) as z:
        for p in folder.rglob('*'):
            if p.is_file():z.write(p,prefix+p.relative_to(folder).as_posix())
        if extra:
            for name,data in extra:z.writestr(prefix+name,data)
    return {'version':read_json(folder/'RELEASE.json')['version'],'size':path.stat().st_size,'sha256':digest(path)}


class UpdatesTests(unittest.TestCase):
    def setUp(self):self.temp=tempfile.TemporaryDirectory(dir=ROOT/'build');self.root=Path(self.temp.name)
    def tearDown(self):self.temp.cleanup()

    def test_semver_and_channels(self):
        self.assertLess(version('0.7.0-alpha.9'),version('v0.7.0-alpha.10'))
        self.assertLess(version('0.7.0-alpha.10'),version('0.7.0'))
        for text in ['v1','1.02.3','1.2.3-alpha..1','1.2.3-alpha.01','../1.2.3']:
            with self.assertRaises(UpdateError):version(text)
        def release(v,**extra):
            name='CyVR_QuestDev-'+v+'-Windows-x64.zip'
            return dict(tag_name='v'+v,published_at='fixture',prerelease='-' in v,draft=False,assets=[{'id':1,'name':name,'state':'uploaded','size':100,'digest':'sha256:'+'a'*64},{'id':2,'name':'SHA256SUMS.txt','state':'uploaded'}],**extra)
        values=[release('0.8.0-alpha.1'),release('0.7.1'),release('0.7.0-alpha.2')]
        self.assertEqual(newest(values,'0.7.0-alpha.1','alpha')['version'],'0.8.0-alpha.1')
        self.assertEqual(newest(values,'0.7.0-alpha.1','stable')['version'],'0.7.1')
        values[0]['draft']=True;values[1]['assets'][0]['digest']='';self.assertEqual(newest(values,'0.7.0-alpha.1','alpha')['version'],'0.7.0-alpha.2')
        self.assertIsNone(newest(values,'0.9.0','alpha'))

    def test_redirects_drop_credentials_and_reject_other_origins(self):
        request=urllib.request.Request('https://api.github.com/repos/test/releases/assets/1',headers={'Authorization':'Bearer fixture-token-not-a-credential'})
        handler=Redirects()
        forwarded=handler.redirect_request(request,None,302,'',{},'https://release-assets.githubusercontent.com/file?fixture=1')
        self.assertIsNone(forwarded.get_header('Authorization'))
        for value in ['http://release-assets.githubusercontent.com/file','https://release-assets.githubusercontent.com.evil.invalid/file','https://user:password@api.github.com/file']:
            with self.assertRaises(UpdateError):handler.redirect_request(request,None,302,'',{},value)

    def test_download_rejects_truncation_and_digest_mismatch(self):
        payload=b'fixture package';sha=hashlib.sha256(payload).hexdigest();release={'sumId':2,'assetId':1,'name':'fixture.zip','size':len(payload),'sha256':sha}
        sums=(sha+'  fixture.zip\n').encode();client=GitHub()
        for body in [payload[:-1],b'x'*len(payload)]:
            with patch.object(client,'open',side_effect=[io.BytesIO(sums),io.BytesIO(body)]):
                with self.assertRaises(UpdateError):client.download(release,self.root/'fixture.zip',threading.Event(),lambda *a:None)
            self.assertFalse((self.root/'fixture.part').exists());self.assertFalse((self.root/'fixture.zip').exists())
        with patch.object(client,'open',side_effect=[io.BytesIO(sums),io.BytesIO(payload)]):client.download(release,self.root/'fixture.zip',threading.Event(),lambda *a:None)
        self.assertEqual((self.root/'fixture.zip').read_bytes(),payload)

    def test_verified_extraction_and_tampering(self):
        source=product(self.root/'source');archive=self.root/'package.zip';release=zip_product(source,archive)
        dest=extract_package(archive,self.root/'ok',release);self.assertTrue((dest/'CyVR_QuestDev.exe').is_file())
        (dest/'VERSION').write_text('tampered')
        with self.assertRaises(UpdateError):verify_tree(dest,release['version'])
        release['sha256']='0'*64
        with self.assertRaises(UpdateError):extract_package(archive,self.root/'bad',release)
        self.assertFalse((self.root/'bad').exists())

    def test_archive_path_attacks_and_case_collisions(self):
        source=product(self.root/'source')
        for i,name in enumerate(['../escape.txt','nested\\escape.txt','file:stream','CON.txt','folder/trailing.','version']):
            archive=self.root/f'bad-{i}.zip';release=zip_product(source,archive,[(name,b'fixture')])
            with self.assertRaises(UpdateError):extract_package(archive,self.root/f'bad-{i}',release)
        item=zipfile.ZipInfo('link');item.external_attr=(stat.S_IFLNK|0o777)<<16
        archive=self.root/'link.zip'
        with zipfile.ZipFile(archive,'w') as z:
            item.filename='CyVR_QuestDev-0.7.0-alpha.1-Windows-x64/link';z.writestr(item,b'outside')
        release={'version':'0.7.0-alpha.1','size':archive.stat().st_size,'sha256':digest(archive)}
        with self.assertRaises(UpdateError):extract_package(archive,self.root/'links',release)

    def test_worker_protocol_keeps_credentials_out_of_state(self):
        data=self.root/'worker';data.mkdir();write_json(data/'preferences.json',{'channel':['invalid'],'autoCheck':'invalid','useGit':False})
        worker=subprocess.Popen([sys.executable,str(ROOT/'updater/service.py'),'--data-dir',str(data)],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,encoding='utf8',creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
        lines=queue.Queue()
        reader=threading.Thread(target=lambda:[lines.put(line) for line in worker.stdout],daemon=True);reader.start()
        try:
            ready=json.loads(lines.get(timeout=5));self.assertEqual(ready['current'],(ROOT/'VERSION').read_text().strip());self.assertEqual(ready['prefs']['channel'],'alpha');self.assertFalse(ready['prefs']['autoCheck'])
            worker.stdin.write(json.dumps({'type':'preferences','values':{'channel':'stable','useGit':False}})+'\n');worker.stdin.flush()
            for _ in range(4):
                state=json.loads(lines.get(timeout=5))
                self.assertNotIn('token',state)
                if 'prefs' in state:
                    self.assertEqual(state['prefs']['channel'],'stable');break
            else:self.fail('Preference acknowledgement absent')
            self.assertNotIn('token',(data/'preferences.json').read_text())
            backup=self.root/'backup';backup.mkdir()
            write_json(data/'last-install.json',{'target':str(ROOT),'backup':str(backup),'previousVersion':'0.6.0-alpha.2'})
            for _ in range(8):
                state=json.loads(lines.get(timeout=5))
                if state.get('previousVersion')=='0.6.0-alpha.2':
                    self.assertNotIn('busy',state);break
            else:self.fail('Receipt watcher did not refresh the backup')
        finally:
            worker.stdin.write('{"type":"shutdown"}\n');worker.stdin.flush();worker.wait(timeout=12)
            self.assertEqual(worker.returncode,0)
            reader.join(timeout=2);worker.stdin.close();worker.stdout.close();worker.stderr.close()

    def fixture_prepare(self,failure=False):
        old=product(self.root/'Application équipe','0.6.0-alpha.2');new=product(self.root/'new','0.7.0-alpha.1',failure)
        runtime=self.root/'python';runtime.mkdir();(runtime/'python.exe').write_bytes(b'bootstrap fixture');(runtime/'python313.zip').write_bytes(b'zip fixture')
        return old,new,runtime/'python.exe'

    def test_prepare_preserves_personal_files_and_refuses_modified_code(self):
        old,new,python=self.fixture_prepare();(old/'personal.ini').write_text('fixture settings')
        value=install.prepare(old,new,self.root/'updates',python,1,1)
        plan=read_json(value['plan']);stage=Path(plan['stage']);self.assertEqual((stage/'personal.ini').read_text(),'fixture settings')
        self.assertTrue((old/'CyVR_QuestDev.exe').exists());self.assertFalse(Path(plan['backup']).exists())
        (old/'old-component.txt').write_text('modified')
        with self.assertRaises(UpdateError):install.prepare(old,new,self.root/'updates',python,1,1)
        (old/'.git').mkdir()
        with self.assertRaises(UpdateError):install.prepare(old,new,self.root/'updates',python,1,1)

    @unittest.skipUnless(os.name=='nt' and (ROOT/'build/windows/fake_update_app.exe').exists(),'Windows fixture build required')
    def test_transaction_success_failure_and_parent_identity(self):
        for fail in [False,True]:
            case=self.root/str(fail);case.mkdir();original=self.root;self.root=case
            old,new,python=self.fixture_prepare(fail);parent=subprocess.Popen([str(old/'CyVR_QuestDev.exe')],creationflags=subprocess.CREATE_NO_WINDOW)
            kernel=ctypes.WinDLL('kernel32');kernel.GetProcessTimes.argtypes=[ctypes.c_void_p,*([ctypes.POINTER(ctypes.wintypes.FILETIME)]*4)]
            times=[ctypes.wintypes.FILETIME() for _ in range(4)];self.assertTrue(kernel.GetProcessTimes(int(parent._handle),*[ctypes.byref(t) for t in times]));stamp=(times[0].dwHighDateTime<<32)|times[0].dwLowDateTime
            prepared=install.prepare(old,new,case/'updates',python,parent.pid,stamp);plan=read_json(prepared['plan']);children=[];errors=[];popen=subprocess.Popen
            def track(*args,**kwargs):
                process=popen(*args,**kwargs);children.append(process);return process
            def run():
                try:install.apply(prepared['plan'])
                except Exception as error:errors.append(error)
            try:
                bad=dict(plan,parentCreated=stamp+1);write_json(case/'bad.json',bad)
                with self.assertRaises(UpdateError):install.parent_handle(bad)
                with patch('install.subprocess.Popen',side_effect=track):
                    worker=threading.Thread(target=run);worker.start();deadline=time.monotonic()+10
                    while not Path(prepared['ready']).exists():self.assertLess(time.monotonic(),deadline);time.sleep(.05)
                    self.assertIsNone(parent.poll());self.assertTrue(old.exists());parent.terminate();parent.wait(timeout=5);worker.join(timeout=15);self.assertFalse(worker.is_alive())
                self.assertEqual(bool(errors),fail)
                self.assertEqual(read_json(old/'RELEASE.json')['version'],'0.6.0-alpha.2' if fail else '0.7.0-alpha.1')
                if not fail:self.assertTrue(Path(plan['backup']).is_dir())
                else:self.assertFalse(Path(plan['backup']).exists())
            finally:
                if parent.poll() is None:parent.terminate();parent.wait(timeout=5)
                for child in children:
                    if child.poll() is None:child.terminate();child.wait(timeout=5)
                self.root=original


if __name__=='__main__':unittest.main()
