# Copyright (c) 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only

"""Exercise an extracted portable release with no Python/ADB on PATH."""
import argparse
import os
from pathlib import Path
import subprocess
import time
import zipfile

ROOT=Path(__file__).resolve().parents[1]


def main():
    parser=argparse.ArgumentParser();parser.add_argument('archive',type=Path);parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
    destination=args.output.resolve()
    if destination.exists():raise RuntimeError('Use a new verification directory')
    destination.mkdir(parents=True)
    with zipfile.ZipFile(args.archive) as source:
        for item in source.infolist():
            if not (destination/item.filename).resolve().is_relative_to(destination):raise RuntimeError('Invalid archive path')
        source.extractall(destination)
    product=next(destination.glob('CyVR_QuestDev-*-Windows-x64'))
    assert (product/'updater/service.py').is_file() and (product/'UPDATE-MANIFEST.json').is_file()
    environment=dict(os.environ)
    environment['PATH']=str(Path(os.environ['SystemRoot'])/'System32')
    environment['LOCALAPPDATA']=str(destination/'profile')
    for key in ['PYTHONPATH','PYTHONHOME','VIRTUAL_ENV','ANDROID_HOME','ANDROID_SDK_ROOT','QUESTDEV_CONNECTION']:
        environment.pop(key,None)
    video=subprocess.run([str(product/'runtime/mirror/scrcpy.exe'),'--version'],cwd=product,env=environment,capture_output=True,text=True,check=True,timeout=10)
    assert 'scrcpy 5.0.1' in video.stdout,video.stdout
    assert (product/'runtime/mirror/scrcpy-server').is_file()
    csv=destination/'fixture.csv';csv.write_text('Time Stamp,average_frame_rate,custom\n1000,90,\n1100,88,-2\n',encoding='utf8')
    screenshot=destination/'metrics.bmp'
    command=[str(product/'CyVR_QuestDev.exe'),'--adb',str(ROOT/'build/windows/fake_adb.exe'),'--metrics-csv',str(csv),'--page','8','--capture',str(screenshot),'--capture-after-ms','45000']
    app=subprocess.Popen(command,cwd=product,env=environment,creationflags=subprocess.CREATE_NO_WINDOW)
    try:
        connection=destination/'profile/CyVR/QuestDev/assistant/connection.json'
        deadline=time.monotonic()+10
        while not connection.exists():
            assert app.poll() is None,'App exited early'
            assert time.monotonic()<deadline,'Portable Core did not start'
            time.sleep(.1)
        time.sleep(1)
        code=r'''
import asyncio,sys
from pathlib import Path
import win32api,win32security,cryptography,pydantic_core
from sdk import QuestDevClient
from mcp import ClientSession,StdioServerParameters
from mcp.client.stdio import stdio_client
connection=Path(sys.argv[1]);product=Path(sys.argv[2])
assert Path(sys.prefix).is_relative_to(product)
with QuestDevClient(connection) as client:
    assert len(client.describe()['operations'])==17
    context=client.invoke('get_context')['context'];assert context['selectedDevice']=='FAKE-QUEST'
    assert context['mirroring']['active'] is False and context['mirroring']['window']=='dedicated_native_video'
    pending=client.client.call('SubmitJob',sessionId=client.session,operationId='device_action',parameters={'action':'mirror_start','mirror':{'maxSize':1600,'audio':'off'}})
    await_id=pending['jobId']
    import time
    time.sleep(.3)
    pending=client.client.call('GetJob',jobId=await_id)
    assert pending['state'] not in {'Completed','Failed','Cancelled'},pending
    client.client.call('CancelJob',jobId=await_id)
    for _ in range(100):
        pending=client.client.call('GetJob',jobId=await_id)
        if pending['state'] in {'Completed','Failed','Cancelled'}:break
        time.sleep(.05)
    assert pending['state']=='Cancelled',pending
    assert not client.invoke('get_context')['context']['mirroring']['active']
    values=client.invoke('read_metrics',{'limit':2})
    assert values['rows']==2 and values['samples'][0]['values']['custom'] is None,values
    job=client.run('metrics_action',{'action':'export_csv'})
    assert Path(job['outputs']['file']).is_file()
async def check():
    parameters=StdioServerParameters(command=sys.executable,args=[str(product/'assistant/tool.py'),'mcp','--connection',str(connection)])
    async with stdio_client(parameters) as (reader,writer):
        async with ClientSession(reader,writer) as session:
            await session.initialize();catalogue=await session.list_tools();assert len(catalogue.tools)==38
            result=await session.call_tool('op_read_metrics',{'parameters':{'limit':1}});assert not result.isError
            job=result.structuredContent['result']
            for _ in range(100):
                if job['state'] in {'Completed','Failed','Cancelled'}:break
                await asyncio.sleep(.05);result=await session.call_tool('cy_get_job',{'jobId':job['jobId']});job=result.structuredContent['result']
            assert job['state']=='Completed' and len(job['outputs']['samples'])==1,job
asyncio.run(check())
print('Embedded Python + Windows imports + SDK + 38 MCP tools + metrics/export OK')
'''
        subprocess.run([str(product/'runtime/python/Scripts/python.exe'),'-B','-c',code,str(connection),str(product)],cwd=product,env=environment,check=True,timeout=30)
        app.wait(timeout=55);assert app.returncode==0
        assert screenshot.stat().st_size>10000
        assert not connection.exists(),'Connection retained after normal app shutdown'
        print('Relocated ZIP, restricted PATH, native rendering and shutdown OK')
    finally:
        if app.poll() is None:
            try:app.wait(timeout=55)
            except subprocess.TimeoutExpired:app.terminate();app.wait(timeout=10)


if __name__=='__main__':main()
