"""Native UI + actual Core HTTP/MCP plugin integration, without an AI provider.

--live reads the installed services. It never applies HUD settings or captures GPU
data. Initialize the CyInsightOVR collector from the UI first.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import time

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'assistant'))
from sdk import QuestDevClient


def main():
    parser=argparse.ArgumentParser()
    mode=parser.add_mutually_exclusive_group(required=True)
    mode.add_argument('--fake',action='store_true')
    mode.add_argument('--live',action='store_true')
    options=parser.parse_args()
    connection=Path(os.environ['LOCALAPPDATA'])/'CyVR/QuestDev/assistant/connection.json'
    screenshot=ROOT/'captures'/('plugins-native-fake.bmp' if options.fake else 'plugins-native-live.bmp')
    screenshot.parent.mkdir(exist_ok=True)
    command=[str(ROOT/'build/windows/CyVR_QuestDev.exe'),'--page','5','--plugin','meta.ovr-metrics','--capture',str(screenshot),'--capture-after-ms','22000']
    if options.fake:command+=['--adb',str(ROOT/'build/windows/fake_adb.exe')]
    started=time.time()
    app=subprocess.Popen(command,creationflags=subprocess.CREATE_NO_WINDOW)
    try:
        deadline=time.monotonic()+12
        while not (connection.exists() and connection.stat().st_mtime>=started-1):
            assert time.monotonic()<deadline,'Assistant did not become ready'
            assert app.poll() is None,'App exited early'
            time.sleep(.1)
        time.sleep(3)
        with QuestDevClient(connection) as quest:
            context=quest.invoke('get_context')['context']
            assert context['version']=='0.5.0'
            if options.fake:assert context['selectedDevice']=='FAKE-QUEST'
            else:assert context['online'] and context['selectedDevice']!='FAKE-QUEST'
            catalogue=quest.invoke('list_plugins')['plugins']
            for identifier in ('meta.ovr-metrics','cyinsights.ovr'):
                plugin=next(p for p in catalogue if p['id']==identifier)
                assert plugin['installed'] and plugin['settingsSchema']['additionalProperties'] is False,plugin
                assert {'apply','read_state','hide'} <= {a['id'] for a in plugin['actions']}
                response=quest.invoke('plugin_action',{'plugin':identifier,'action':'read_state'})
                if identifier=='meta.ovr-metrics':assert response['configuration']['scale']>=1
                else:
                    assert response['bridge'] is True
                    if options.fake:
                        assert Path(response['file']).suffix=='.cyovr'
                        capture=json.loads(Path(response['file']).read_text())
                        assert capture['capture_id']=='fixture-capture-123' and capture['surfaces']==[]
                print(identifier+': native/Core service state OK',flush=True)
            response=quest.invoke('plugin_action',{'plugin':'cyinsights.ovr','action':'catalog'})
            assert any(m['id']=='average_frame_rate' and m['available'] for m in response['metrics']),response
            try:quest.invoke('plugin_action',{'plugin':'meta.ovr-metrics','action':'read_state','settings':{'shell':'reboot'}})
            except RuntimeError:pass
            else:raise AssertionError('Native plugin accepted unknown settings')
            job=quest.client.call('SubmitJob',sessionId=quest.session,operationId='plugin_action',parameters={'plugin':'meta.ovr-metrics','action':'apply','settings':{'scale':1.5}})
            time.sleep(.3)
            job=quest.client.call('GetJob',jobId=job['jobId'])
            assert job['state'] not in {'Completed','Failed','Cancelled'},job
            quest.client.call('CancelJob',jobId=job['jobId'])
            deadline=time.monotonic()+3
            while job['state'] not in {'Completed','Failed','Cancelled'}:
                assert time.monotonic()<deadline,job
                time.sleep(.05);job=quest.client.call('GetJob',jobId=job['jobId'])
            assert job['state']=='Cancelled',job
            quest.invoke('plugin_action',{'plugin':'meta.ovr-metrics','action':'read_state'})
            print('Plugin schemas, catalogue, action gate and cancellation: OK',flush=True)
            # Exercise the official MCP transport against this same native host.
            subprocess.run([sys.executable,str(ROOT/'tools/verify_assistant.py'),'--connection',str(connection)],check=True,timeout=12)
        app.wait(timeout=25)
        assert app.returncode==0
        assert 'modal_open=0\n' in Path(str(screenshot)+'.stats.txt').read_text(encoding='utf8')
        assert not connection.exists()
        print('Native render and clean shutdown: OK',flush=True)
    finally:
        if app.poll() is None:app.wait(timeout=30)


if __name__=='__main__':main()
