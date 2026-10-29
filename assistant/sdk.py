# Copyright (c) 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only

"""Small native-app SDK using the official CyToolsCore HTTP client."""
import time
from cytools_core import CyToolError
from tool import client


class QuestDevClient:
    def __init__(self,connection_file=None):
        self.client=client(connection_file)
        self.session=self.client.call('CreateSession',temporary=True)['sessionId']

    def describe(self):
        return self.client.call('DescribeTool')

    def run(self,operation,parameters=None,timeout=180):
        """Return the completed Core job, including its ID and outputs."""
        job=self.client.call('SubmitJob',sessionId=self.session,operationId=operation,parameters=parameters or {})
        deadline=time.monotonic()+timeout
        while job['state'] not in {'Completed','Failed','Cancelled'}:
            if time.monotonic()>deadline:
                self.client.call('CancelJob',jobId=job['jobId'])
                raise TimeoutError(operation)
            time.sleep(.05)
            job=self.client.call('GetJob',jobId=job['jobId'])
        if job['state']!='Completed':
            raise RuntimeError(job.get('error',job['state']))
        return job

    def invoke(self,operation,parameters=None,timeout=180):
        return self.run(operation,parameters,timeout).get('outputs',{})

    def export_job(self,job_id,destination,timeout=30):
        """Preserve a stopped job outside the runtime before closing its task."""
        deadline=time.monotonic()+timeout
        while True:
            try:return self.client.call('ExportJob',jobId=job_id,destination=str(destination))
            except CyToolError as error:
                if error.code!='Busy' or time.monotonic()>=deadline:raise
                time.sleep(.05)

    def close(self):
        return self.client.call('FinishTask',sessionId=self.session,cancelRunning=True)

    def __enter__(self):return self
    def __exit__(self,*args):self.close()
