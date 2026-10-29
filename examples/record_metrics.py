"""Record OVR CSVs and preserve the Core job before its task is closed."""
import argparse
from pathlib import Path
import sys

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'assistant'))
from sdk import QuestDevClient


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('destination',type=Path)
    parser.add_argument('--seconds',type=int,default=10)
    options=parser.parse_args()
    with QuestDevClient() as quest:
        job=quest.run('record_metrics',{'seconds':options.seconds},timeout=options.seconds+90)
        print(f"Reports: {job['outputs']['filesSaved']}; measured samples: {job['outputs']['hasSamples']}")
        quest.export_job(job['jobId'],options.destination.resolve())


if __name__=='__main__':main()
