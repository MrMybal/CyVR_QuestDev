# Copyright (c) 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only

"""Native metrics + official Core SDK/MCP integration with growing ADB fixtures.

Requires no other QuestDev instance. No AI provider or physical device is used.
"""
import asyncio
import json
import os
from pathlib import Path
import subprocess
import sys
import time

ROOT=Path(__file__).resolve().parents[1]
BUILD=Path(os.environ.get('QUESTDEV_BUILD_DIR',str(ROOT/'build/windows')))
sys.path.insert(0,str(ROOT/'assistant'))
from sdk import QuestDevClient
from mcp import ClientSession,StdioServerParameters
from mcp.client.stdio import stdio_client


async def check_mcp(connection):
    parameters=StdioServerParameters(command=sys.executable,args=[str(ROOT/'assistant/tool.py'),'mcp','--connection',str(connection)])
    async with stdio_client(parameters) as (reader,writer):
        async with ClientSession(reader,writer) as session:
            await session.initialize()
            catalogue=await session.list_tools()
            assert {'op_read_metrics','op_metrics_action','op_record_metrics'} <= {t.name for t in catalogue.tools}
            result=await session.call_tool('op_read_metrics',{'parameters':{'metrics':['average_frame_rate'],'limit':2}})
            assert not result.isError,result
            job=result.structuredContent['result'];deadline=time.monotonic()+8
            while job['state'] not in {'Completed','Failed','Cancelled'}:
                assert time.monotonic()<deadline,job
                await asyncio.sleep(.05)
                result=await session.call_tool('cy_get_job',{'jobId':job['jobId']});job=result.structuredContent['result']
            assert job['state']=='Completed',job
            assert len(job['outputs']['samples'])==2,job
            print(f'MCP: {len(catalogue.tools)} tools, sampled metrics and cleanup OK',flush=True)


def main():
    folder=ROOT/f'captures/metrics-native-{os.getpid()}-{time.time_ns()}'
    reports=folder/'device';reports.mkdir(parents=True)
    source=folder/'session-équipe.csv'
    payload='Time Stamp,average_frame_rate,custom_extra\n1000,90,-3\n1100,80,\n1200,89,5\n'
    source.write_text(payload,encoding='utf8')
    remote=reports/'quest.csv';remote.write_text(payload,encoding='utf8')
    environment=dict(os.environ);environment['QUEST_FAKE_METRICS_ROOT']=str(reports)
    connection=Path(os.environ['LOCALAPPDATA'])/'CyVR/QuestDev/assistant/connection.json'
    assert not connection.exists(),'Close the other QuestDev instance first'
    command=[str(BUILD/'CyVR_QuestDev.exe'),'--adb',str(BUILD/'fake_adb.exe'),'--page','8','--metrics-csv',str(source),'--capture',str(folder/'native.bmp'),'--capture-after-ms','23000']
    started=time.time();app=subprocess.Popen(command,env=environment,creationflags=subprocess.CREATE_NO_WINDOW)
    try:
        deadline=time.monotonic()+12
        while not (connection.exists() and connection.stat().st_mtime>=started-1):
            assert time.monotonic()<deadline,'Assistant did not become ready'
            assert app.poll() is None,'App exited early'
            time.sleep(.1)
        time.sleep(2)
        with QuestDevClient(connection) as quest:
            context=quest.invoke('get_context')['context']
            assert context['version']==(ROOT/'VERSION').read_text().strip().split('-')[0] and context['selectedDevice']=='FAKE-QUEST',context
            assert context['pages']['metrics']==8
            quest.invoke('metrics_action',{'action':'open_csv','sourceFile':str(source)})
            values=quest.invoke('read_metrics',{'metrics':['average_frame_rate','custom_extra'],'start':.1,'end':.2,'limit':2})
            assert values['rows']==3 and len(values['columns'])==2,values
            assert [s['values']['custom_extra'] for s in values['samples']]==[None,5.0],values
            job=quest.run('metrics_action',{'action':'export_csv'})
            temporary=Path(job['outputs']['file']);assert temporary.read_text(encoding='utf8')==payload
            exported=quest.export_job(job['jobId'],folder/'durable')
            assert exported,exported
            reports_state=quest.invoke('metrics_action',{'action':'refresh_reports'})
            assert reports_state['reportsCount']==1 and not reports_state['reportsTruncated'],reports_state
            path=reports_state['reports'][0]['path']
            quest.invoke('metrics_action',{'action':'follow_report','path':path})
            with remote.open('a',encoding='utf8') as out:out.write('1300,87,')
            time.sleep(1.5)
            assert quest.invoke('read_metrics')['rows']==3,'Partial row must wait'
            with remote.open('a',encoding='utf8') as out:out.write('9\n')
            deadline=time.monotonic()+5
            while quest.invoke('read_metrics')['rows']!=4:
                assert time.monotonic()<deadline,'Growing CSV did not reach the native viewer'
                time.sleep(.2)
            quest.invoke('metrics_action',{'action':'pause'})
            assert not quest.invoke('read_metrics')['follow']
            pending=quest.client.call('SubmitJob',sessionId=quest.session,operationId='record_metrics',parameters={'seconds':2})
            time.sleep(.4);pending=quest.client.call('GetJob',jobId=pending['jobId'])
            assert pending['state'] not in {'Completed','Failed','Cancelled'},pending
            quest.client.call('CancelJob',jobId=pending['jobId'])
            deadline=time.monotonic()+5
            while pending['state'] not in {'Completed','Failed','Cancelled'}:
                assert time.monotonic()<deadline,pending
                time.sleep(.05);pending=quest.client.call('GetJob',jobId=pending['jobId'])
            assert pending['state']=='Cancelled' and not (reports/'enabled').exists(),pending
            asyncio.run(check_mcp(connection))
            print('Native SDK: CSV import/export, range, missing values, growing follow, pause and cancelled approval OK',flush=True)
        assert not temporary.exists(),'Temporary export retained after FinishTask'
        assert source.read_text(encoding='utf8')==payload,'Original input changed'
        assert list((folder/'durable').rglob('session.csv')),'Export did not survive FinishTask'
        app.wait(timeout=30);assert app.returncode==0
        assert not connection.exists(),'Core connection retained after app shutdown'
        print('Durable export, original source and normal shutdown OK',flush=True)
    finally:
        if app.poll() is None:
            # The native --capture timer closes the app normally, including workers.
            try:app.wait(timeout=30)
            except subprocess.TimeoutExpired:app.terminate();app.wait(timeout=10)


if __name__=='__main__':main()
