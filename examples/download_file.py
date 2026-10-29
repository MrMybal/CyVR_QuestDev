"""Download a Quest file and preserve the Core export after closing its task."""
from pathlib import Path
import argparse
import sys

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'assistant'))
from sdk import QuestDevClient

parser=argparse.ArgumentParser()
parser.add_argument('remote_path',help='Absolute Quest path, for example /sdcard/Download/report.txt')
parser.add_argument('output_directory',help='Explicit local directory for the durable export')
options=parser.parse_args()
with QuestDevClient() as quest:
    job=quest.run('pull_file',{'path':options.remote_path},timeout=360)
    saved=quest.export_job(job['jobId'],Path(options.output_directory).resolve())
    print(saved['outputs']['file'])
