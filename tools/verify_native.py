# Copyright (c) 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only

"""Local native/Core integration on a connected Quest; sends no data to an AI."""
import json
import os
from pathlib import Path
import subprocess
import sys
import time
import argparse

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'assistant'))
from sdk import QuestDevClient


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--fake',action='store_true');options=parser.parse_args()
    connection=Path(os.environ['LOCALAPPDATA'])/'CyVR/QuestDev/assistant/connection.json'
    screenshot=ROOT/('captures/files-native-fake.bmp' if options.fake else 'captures/assistant-live.bmp');screenshot.parent.mkdir(exist_ok=True)
    started=time.time()
    arguments=[str(ROOT/'build/windows/CyVR_QuestDev.exe'),'--page','7' if options.fake else '3','--capture',str(screenshot),'--capture-after-ms','20000' if options.fake else '16000']
    if options.fake:arguments += ['--adb',str(ROOT/'build/windows/fake_adb.exe')]
    app=subprocess.Popen(arguments,creationflags=subprocess.CREATE_NO_WINDOW)
    try:
        deadline=time.monotonic()+12
        while time.monotonic()<deadline:
            if connection.exists() and connection.stat().st_mtime>=started-1:break
            if app.poll() is not None:raise RuntimeError(f'App exited: {app.returncode}')
            time.sleep(.1)
        else:raise RuntimeError('Companion not ready; inspect assistant/runtime-errors.log')
        time.sleep(3)
        subprocess.run([sys.executable,str(ROOT/'tools/verify_assistant.py'),'--connection',str(connection)],check=True,timeout=12)
        with QuestDevClient(connection) as quest:
            if options.fake:
                context=quest.invoke('get_context')['context'];assert context['selectedDevice']=='FAKE-QUEST',context
                listing=quest.invoke('list_files',{'path':'/sdcard','offset':0,'limit':2})
                assert listing['count']==5 and len(listing['entries'])==2 and listing['nextOffset']==2,listing
                next_page=quest.invoke('list_files',{'path':'/sdcard','offset':2,'limit':3})
                assert len(next_page['entries'])==3 and next_page['nextOffset'] is None,next_page
                try:quest.invoke('list_files',{'path':'/denied'})
                except RuntimeError:pass
                else:raise AssertionError('Permission error was reported as success')
                downloaded=quest.run('pull_file',{'path':'/sdcard/readme.txt'});output=Path(downloaded['outputs']['file']);assert output.read_bytes()==b'fixture Quest payload\n'
                exported=quest.export_job(downloaded['jobId'],ROOT/'captures/exported-files')
                preserved=Path(exported['outputs']['file']);assert preserved.read_bytes()==output.read_bytes()
                print('Native file listing, pagination, denied folder and private download (fake ADB): OK',flush=True)
            # Default action policy must keep a read-only shell request pending
            # for confirmation; cancelling it must be acknowledged by the GUI.
            job=quest.client.call('SubmitJob',sessionId=quest.session,operationId='adb_execute',parameters={'command':'getprop ro.product.model'})
            time.sleep(.3);job=quest.client.call('GetJob',jobId=job['jobId'])
            assert job['state'] not in {'Completed','Failed','Cancelled'},job
            quest.client.call('CancelJob',jobId=job['jobId'])
            deadline=time.monotonic()+3
            while job['state'] not in {'Completed','Failed','Cancelled'}:
                assert time.monotonic()<deadline,job
                time.sleep(.05);job=quest.client.call('GetJob',jobId=job['jobId'])
            assert job['state']=='Cancelled',job
            print('Native action gate and cancellation acknowledgement: OK',flush=True)
            if options.fake:
                fixture=ROOT/'captures/upload-source-é.bin';fixture.write_bytes(b'local original\x00fixture')
                requests=[('file_action',{'action':'delete','path':'/sdcard/readme.txt'}),('push_file',{'sourceFile':str(fixture),'destination':'/sdcard/upload.bin'})]
                for operation,parameters in requests:
                    job=quest.client.call('SubmitJob',sessionId=quest.session,operationId=operation,parameters=parameters)
                    deadline=time.monotonic()+4
                    while job['state'] in {'Queued','Starting'}:
                        assert time.monotonic()<deadline,job
                        time.sleep(.05);job=quest.client.call('GetJob',jobId=job['jobId'])
                    time.sleep(.2);job=quest.client.call('GetJob',jobId=job['jobId']);assert job['state'] not in {'Completed','Failed','Cancelled'},job
                    quest.client.call('CancelJob',jobId=job['jobId'])
                    while job['state'] not in {'Completed','Failed','Cancelled'}:
                        assert time.monotonic()<deadline,job
                        time.sleep(.05);job=quest.client.call('GetJob',jobId=job['jobId'])
                    assert job['state']=='Cancelled',job
                assert fixture.read_bytes()==b'local original\x00fixture';fixture.unlink()
                print('File changes require approval; cancelled upload preserves local original: OK',flush=True)
            quest.invoke('navigate',{'page':'files' if options.fake else 'assistant'})
        if options.fake:
            assert not output.exists(),'Private download retained after task finish'
            assert preserved.read_bytes()==b'fixture Quest payload\n','Export did not survive task finish'
        app.wait(timeout=20);assert app.returncode==0,app.returncode
        pixels=screenshot.read_bytes()[54:]
        assert len({pixels[i:i+4] for i in range(0,len(pixels),400)})>15,'Blank native capture'
        assert 'modal_open=0\n' in Path(str(screenshot)+'.stats.txt').read_text(encoding='utf8'),'Cancelled action left a modal open'
        assert not connection.exists(),'Stale live connection after shutdown'
        print('Native render, clean shutdown and connection removal: OK',flush=True)
    finally:
        if app.poll() is None:app.wait(timeout=25)


if __name__=='__main__':main()
