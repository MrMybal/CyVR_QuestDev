"""Real provider/Core/MCP smoke check; uses the existing Codex CLI account."""
import argparse
import asyncio
import json
import os
from pathlib import Path
import queue
import subprocess
import sys
import threading
import time

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'assistant'))
from sdk import QuestDevClient
from mcp import ClientSession,StdioServerParameters
from mcp.client.stdio import stdio_client


async def check_mcp(connection,files=False):
    parameters=StdioServerParameters(command=sys.executable,args=[str(ROOT/'assistant/tool.py'),'mcp','--connection',str(connection)])
    async with stdio_client(parameters) as (reader,writer):
        async with ClientSession(reader,writer) as session:
            await session.initialize()
            catalogue=await session.list_tools()
            assert 'op_get_context' in {t.name for t in catalogue.tools}
            result=await session.call_tool('op_get_context',{'parameters':{}})
            assert not result.isError,result
            job=result.structuredContent['result']
            while job['state'] not in {'Completed','Failed','Cancelled'}:
                await asyncio.sleep(.05)
                result=await session.call_tool('cy_get_job',{'jobId':job['jobId']})
                job=result.structuredContent['result']
            assert job['state']=='Completed',job
            assert job['outputs']['context']['selectedDevice']
            assert 'op_list_files' in {t.name for t in catalogue.tools}
            if files:
                result=await session.call_tool('op_list_files',{'parameters':{'path':'/sdcard','limit':2}})
                assert not result.isError,result
                job=result.structuredContent['result'];deadline=time.monotonic()+8
                while job['state'] not in {'Completed','Failed','Cancelled'}:
                    assert time.monotonic()<deadline,job
                    await asyncio.sleep(.05);result=await session.call_tool('cy_get_job',{'jobId':job['jobId']});job=result.structuredContent['result']
                assert job['state']=='Completed',job
                assert 'entries' in job['outputs'],job
                result=await session.call_tool('op_plugin_action',{'parameters':{'plugin':'meta.ovr-metrics','action':'read_state'}})
                assert not result.isError,result
                job=result.structuredContent['result'];deadline=time.monotonic()+8
                while job['state'] not in {'Completed','Failed','Cancelled'}:
                    assert time.monotonic()<deadline,job
                    await asyncio.sleep(.05);result=await session.call_tool('cy_get_job',{'jobId':job['jobId']});job=result.structuredContent['result']
                assert job['state']=='Completed' and 'configuration' in job['outputs'],job
            print(f'MCP: catalogue ({len(catalogue.tools)} tools), invocation and task cleanup OK',flush=True)


class Service:
    def __init__(self):
        self.events=queue.Queue();self.tool_calls=[]
        directory=ROOT/f'captures/assistant-smoke/run-{os.getpid()}';directory.mkdir(parents=True,exist_ok=True)
        self.errors=open(directory/'stderr.log','w',encoding='utf8')
        self.process=subprocess.Popen([sys.executable,str(ROOT/'assistant/service.py'),'--data-dir',str(directory/'data')],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=self.errors,text=True,encoding='utf8',creationflags=subprocess.CREATE_NO_WINDOW)
        def read():
            for line in self.process.stdout:
                try:self.events.put(json.loads(line))
                except ValueError:pass
        threading.Thread(target=read,daemon=True).start()

    def send(self,**message):
        self.process.stdin.write(json.dumps(message)+'\n');self.process.stdin.flush()

    def wait(self,kind,timeout=180):
        deadline=time.monotonic()+timeout
        while time.monotonic()<deadline:
            try:event=self.events.get(timeout=.2)
            except queue.Empty:
                if self.process.poll() is not None:raise RuntimeError('Service stopped; inspect captures/assistant-smoke/stderr.log')
                continue
            if event['type']=='error':raise RuntimeError(event.get('message'))
            if event['type']=='assistant_tool':self.tool_calls.append((event['operation'],event['status']))
            if event['type']=='native_call':
                assert event['operation']=='read_logs',event
                self.send(type='native_result',id=event['id'],ok=True,result={'logs':[{'level':'I','tag':'Smoke','message':'QUESTDEV_NATIVE_LOG_42'}]})
            if event['type']==kind:return event
        raise TimeoutError(kind)

    def close(self):
        if self.process.poll() is None:
            self.send(type='shutdown')
            try:self.process.wait(timeout=12)
            except subprocess.TimeoutExpired:self.process.kill();self.process.wait()
        self.errors.close()


def main():
    parser=argparse.ArgumentParser();mode=parser.add_mutually_exclusive_group();mode.add_argument('--connection');mode.add_argument('--provider',action='store_true');options=parser.parse_args()
    service=None
    try:
        if options.connection:connection=Path(options.connection)
        else:
            service=Service();ready=service.wait('ready',30);connection=Path(ready['connectionFile'])
            service.send(type='context',context={'selectedDevice':'QUESTDEV_SMOKE_CONTEXT_17','session':42,'recentLogs':[]})
            time.sleep(.1)
        with QuestDevClient(connection) as client:
            assert len(client.describe()['operations'])==14
            context=client.invoke('get_context')['context']
            assert context.get('selectedDevice'),context
            if options.connection:
                apps=client.invoke('list_applications')['applications']
                assert apps,'No installed packages'
                logs=client.invoke('read_logs',{'limit':50})
                assert logs['matched']>0,logs
                export=client.invoke('export_logs',{'format':'jsonl'})
                assert Path(export['file']).stat().st_size>0
                plugins=client.invoke('list_plugins')['plugins']
                assert plugins,'No loaded plugin'
                print(f'Native SDK: {len(apps)} packages, {logs["matched"]} retained logs, {len(plugins)} plugin(s), private export OK',flush=True)
            print('CyToolsCore: authenticated HTTP and SDK invocation OK',flush=True)
        asyncio.run(check_mcp(connection,files=bool(options.connection)))
        if options.provider:
            assert service,'Provider smoke uses an isolated companion'
            service.send(type='connect',config={'connector':'codex'})
            connected=service.wait('connected',60)
            assert connected.get('tools'),connected
            service.wait('idle',5)
            print(f'Codex: authenticated, {len(connected["models"])} models discovered',flush=True)
            service.send(type='chat',message='Call questdev_get_context, then questdev_read_logs with limit 1. Reply only with the selectedDevice value and the exact message from the tool result. Use no other tools.',includeContext=False)
            answer=service.wait('chat_done',180)['text']
            assert 'QUESTDEV_SMOKE_CONTEXT_17' in answer,answer
            assert 'QUESTDEV_NATIVE_LOG_42' in answer,answer
            for operation in ('get_context','read_logs'):assert (operation,'completed') in service.tool_calls,service.tool_calls
            print('Codex: real streamed answer + two native tool calls through CyToolsCore OK',flush=True)
    finally:
        if service:service.close()


if __name__=='__main__':main()
