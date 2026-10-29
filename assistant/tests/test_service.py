# Copyright (c) 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only

import json
from pathlib import Path
import sys
import tempfile
import time
import unittest
from unittest.mock import patch
import queue
import hashlib

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from service import Host,create_runtime,download_name
from cytools_core.transports.api import LocalClient
from cytools_core import CyToolError


class LocalHost(Host):
    def call(self,context,operation,parameters):
        context.cancellation.check()
        if operation=='export_logs':
            path=Path(parameters['_output']);path.write_text('test log\n',encoding='utf8')
            return {'file':str(path),'mimeType':'application/x-ndjson'}
        if operation=='pull_file':
            path=Path(parameters['_output']);path.write_bytes(b'fixture download')
            return {'file':str(path),'mimeType':'application/octet-stream'}
        if operation=='plugin_action' and parameters.get('action')=='read_state':
            path=Path(parameters['_output']);path.write_text('{"schema":"cyinsightovr.capture","version":1,"surfaces":[]}',encoding='utf8')
            return {'file':str(path),'mimeType':'application/json'}
        if operation=='push_file':
            path=Path(parameters['sourceFile'])
            return {'snapshot':str(path),'sha256':hashlib.sha256(path.read_bytes()).hexdigest()}
        if operation=='metrics_action' and parameters.get('action')=='open_csv':
            path=Path(parameters['sourceFile'])
            return {'snapshot':str(path),'csv':path.read_text(encoding='utf8')}
        if operation=='metrics_action' and parameters.get('action')=='export_csv':
            path=Path(parameters['_output']);path.write_text('Time Stamp,fps\n0,90\n',encoding='utf8')
            return {'file':str(path),'mimeType':'text/csv'}
        if operation=='record_metrics':
            path=Path(parameters['_output']);path.mkdir();(path/'session.json').write_text('{}',encoding='utf8')
            return {'file':str(path),'mimeType':'inode/directory','filesSaved':0,'hasSamples':False}
        return {'operation':operation,'parameters':parameters}


class RuntimeTests(unittest.TestCase):
    def setUp(self):
        self.directory=tempfile.TemporaryDirectory()
        self.host=LocalHost();self.host.context={'selectedDevice':'TEST-QUEST','recentLogs':[{'message':'sample'}]}
        self.runtime=create_runtime(Path(self.directory.name),self.host).start()
        a=self.runtime.create_client('reader');b=self.runtime.create_client('controller',permissions=['deviceControl'])
        self.reader=LocalClient(self.runtime,self.runtime.authenticate(a['token']))
        self.controller=LocalClient(self.runtime,self.runtime.authenticate(b['token']))
        self.session=self.reader.call('CreateSession',temporary=True)['sessionId']
        self.control_session=self.controller.call('CreateSession',temporary=True)['sessionId']

    def tearDown(self):
        self.runtime.close();self.directory.cleanup()

    def finish(self,client,job):
        deadline=time.monotonic()+5
        while job['state'] not in {'Completed','Failed','Cancelled'}:
            self.assertLess(time.monotonic(),deadline)
            time.sleep(.02);job=client.call('GetJob',jobId=job['jobId'])
        return job

    def test_context_and_contract(self):
        descriptor=self.reader.call('DescribeTool')
        self.assertEqual(len(descriptor['operations']),17)
        job=self.reader.call('SubmitJob',sessionId=self.session,operationId='get_context',parameters={})
        job=self.finish(self.reader,job)
        self.assertEqual(job['state'],'Completed')
        self.assertEqual(job['outputs']['context']['selectedDevice'],'TEST-QUEST')

    def test_permissions_and_parameter_validation(self):
        with self.assertRaises(CyToolError):
            self.reader.call('SubmitJob',sessionId=self.session,operationId='adb_execute',parameters={'command':'getprop'})
        with self.assertRaises(CyToolError):
            self.controller.call('SubmitJob',sessionId=self.control_session,operationId='device_action',parameters={'action':'unsupported'})
        with self.assertRaises(CyToolError):
            self.controller.call('SubmitJob',sessionId=self.control_session,operationId='read_logs',parameters={'limit':2001})

    def test_native_operation_dispatch(self):
        job=self.controller.call('SubmitJob',sessionId=self.control_session,operationId='adb_execute',parameters={'command':'getprop'})
        job=self.finish(self.controller,job)
        self.assertEqual(job['state'],'Completed')
        self.assertEqual(job['outputs']['operation'],'adb_execute')

    def test_private_export_and_isolation(self):
        job=self.reader.call('SubmitJob',sessionId=self.session,operationId='export_logs',parameters={})
        job=self.finish(self.reader,job)
        self.assertEqual(job['state'],'Completed')
        path=Path(job['outputs']['file'])
        self.assertTrue(path.resolve().is_relative_to(Path(self.directory.name).resolve()))
        self.assertEqual(path.read_text(),'test log\n')
        with self.assertRaises(CyToolError):self.controller.call('GetJob',jobId=job['jobId'])
        result=self.reader.call('FinishTask',sessionId=self.session,cancelRunning=True)
        self.assertFalse(path.exists())

    def test_cancellation_waits_for_native_acknowledgement(self):
        events=queue.Queue()
        self.host.call=lambda context,operation,parameters:Host.call(self.host,context,operation,parameters)
        with patch('service.emit',side_effect=lambda kind,**data:events.put({'type':kind,**data})):
            job=self.reader.call('SubmitJob',sessionId=self.session,operationId='read_logs',parameters={})
            call=events.get(timeout=3);self.assertEqual(call['type'],'native_call')
            self.reader.call('CancelJob',jobId=job['jobId'])
            cancellation=events.get(timeout=3);self.assertEqual(cancellation['type'],'native_cancel')
            job=self.reader.call('GetJob',jobId=job['jobId'])
            self.assertNotIn(job['state'],{'Completed','Failed','Cancelled'})
            self.host.reply({'id':call['id'],'ok':False,'error':'Native worker stopped'})
            job=self.finish(self.reader,job)
            self.assertEqual(job['state'],'Cancelled')

    def test_file_permissions_and_limits(self):
        with self.assertRaises(CyToolError):self.reader.call('SubmitJob',sessionId=self.session,operationId='file_action',parameters={'action':'delete','path':'/sdcard/test.txt'})
        with self.assertRaises(CyToolError):self.reader.call('SubmitJob',sessionId=self.session,operationId='push_file',parameters={'sourceFile':'not-used','destination':'/sdcard/test.txt'})
        with self.assertRaises(CyToolError):self.reader.call('SubmitJob',sessionId=self.session,operationId='list_files',parameters={'limit':501})

    def test_upload_automatically_imports_external_source(self):
        with tempfile.TemporaryDirectory() as external:
            source=Path(external)/"équipe's source.bin";payload=b'original source\x00content';source.write_bytes(payload)
            job=self.controller.call('SubmitJob',sessionId=self.control_session,operationId='push_file',parameters={'sourceFile':str(source.resolve()),'destination':'/sdcard/test.bin'})
            job=self.finish(self.controller,job);self.assertEqual(job['state'],'Completed',job.get('error'))
            snapshot=Path(job['outputs']['snapshot']).resolve()
            self.assertNotEqual(snapshot,source.resolve())
            self.assertTrue(snapshot.is_relative_to(Path(self.directory.name).resolve()))
            self.assertEqual(job['outputs']['sha256'],hashlib.sha256(payload).hexdigest())
            self.controller.call('FinishTask',sessionId=self.control_session,cancelRunning=True)
            self.assertFalse(snapshot.exists());self.assertEqual(source.read_bytes(),payload)

    def test_file_results_are_private_and_temporary(self):
        job=self.reader.call('SubmitJob',sessionId=self.session,operationId='pull_file',parameters={'path':'/sdcard/readme.txt'})
        job=self.finish(self.reader,job);self.assertEqual(job['state'],'Completed')
        output=Path(job['outputs']['file']);self.assertEqual(output.read_bytes(),b'fixture download')
        with self.assertRaises(CyToolError):self.controller.call('GetJob',jobId=job['jobId'])
        self.reader.call('FinishTask',sessionId=self.session,cancelRunning=True);self.assertFalse(output.exists())

    def test_download_names_are_safe_on_windows(self):
        self.assertEqual(download_name('/sdcard/équipe.png'),'équipe.png')
        self.assertEqual(download_name('/sdcard/NUL.txt'),'download-NUL.txt')
        self.assertEqual(download_name('/sdcard/../..'),'download.bin')
        self.assertNotIn('\\',download_name('/sdcard/..\\outside.png'))
        self.assertNotIn(':',download_name('/sdcard/C:outside.png'))
        self.assertTrue(download_name('/sdcard/'+200*'a'+'.png').endswith('.png'))
        self.assertLessEqual(len(download_name('/sdcard/'+200*'a'+'.png')),96)

    def test_plugin_capture_is_owned_by_core(self):
        job=self.controller.call('SubmitJob',sessionId=self.control_session,operationId='plugin_action',parameters={'plugin':'cyinsights.ovr','action':'read_state','settings':{}})
        job=self.finish(self.controller,job);self.assertEqual(job['state'],'Completed',job.get('error'))
        output=Path(job['outputs']['file']);self.assertEqual(output.suffix,'.cyovr')
        self.assertTrue(output.resolve().is_relative_to(Path(self.directory.name).resolve()))
        self.assertEqual(json.loads(output.read_text())['schema'],'cyinsightovr.capture')
        self.controller.call('FinishTask',sessionId=self.control_session,cancelRunning=True)
        self.assertFalse(output.exists())

    def test_metrics_import_preserves_original(self):
        with tempfile.TemporaryDirectory() as external:
            source=Path(external)/'metrics.csv';payload='Time Stamp,fps\n0,90\n';source.write_text(payload,encoding='utf8')
            job=self.controller.call('SubmitJob',sessionId=self.control_session,operationId='metrics_action',parameters={'action':'open_csv','sourceFile':str(source.resolve())})
            job=self.finish(self.controller,job);self.assertEqual(job['state'],'Completed',job.get('error'))
            snapshot=Path(job['outputs']['snapshot']).resolve();self.assertNotEqual(snapshot,source.resolve())
            self.assertTrue(snapshot.is_relative_to(Path(self.directory.name).resolve()));self.assertEqual(job['outputs']['csv'],payload)
            self.controller.call('FinishTask',sessionId=self.control_session,cancelRunning=True)
            self.assertFalse(snapshot.exists());self.assertEqual(source.read_text(),payload)

    def test_metrics_exports_and_recordings_are_job_owned(self):
        for operation,parameters in [('metrics_action',{'action':'export_csv'}),('record_metrics',{'seconds':1})]:
            job=self.controller.call('SubmitJob',sessionId=self.control_session,operationId=operation,parameters=parameters)
            job=self.finish(self.controller,job);self.assertEqual(job['state'],'Completed',job.get('error'))
            output=Path(job['outputs']['file']);self.assertTrue(output.exists())
            self.assertTrue(output.resolve().is_relative_to(Path(self.directory.name).resolve()))
            with self.assertRaises(CyToolError):self.reader.call('GetJob',jobId=job['jobId'])
        self.controller.call('FinishTask',sessionId=self.control_session,cancelRunning=True);self.assertFalse(output.exists())

    def test_metrics_permissions_and_limits(self):
        for operation,parameters in [('record_metrics',{'seconds':0}),('record_metrics',{'seconds':301}),('read_metrics',{'limit':2001}),('read_metrics',{'metrics':['fps']*17})]:
            with self.assertRaises(CyToolError):self.controller.call('SubmitJob',sessionId=self.control_session,operationId=operation,parameters=parameters)
        with self.assertRaises(CyToolError):self.reader.call('SubmitJob',sessionId=self.session,operationId='record_metrics',parameters={'seconds':1})
        job=self.reader.call('SubmitJob',sessionId=self.session,operationId='read_metrics',parameters={'limit':2})
        self.assertEqual(self.finish(self.reader,job)['state'],'Completed')


if __name__=='__main__':unittest.main()
