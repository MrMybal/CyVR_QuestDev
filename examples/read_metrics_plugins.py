# Copyright (c) 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only

"""Read both installed metrics services through the native QuestDev SDK."""
from pathlib import Path
import json
import sys

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'assistant'))
from sdk import QuestDevClient

with QuestDevClient() as quest:
    catalogue=quest.invoke('list_plugins')['plugins']
    for plugin in catalogue:
        if not plugin.get('controller') or not plugin.get('installed'):continue
        job=quest.run('plugin_action',{'plugin':plugin['id'],'action':'read_state'})
        result=job['outputs']
        print(plugin['name']+': '+json.dumps(result,ensure_ascii=False,indent=2))
        if 'file' in result:
            destination=Path.cwd()/'QuestDev-captures'
            exported=quest.export_job(job['jobId'],destination)
            print('Capture conservée : '+exported['outputs']['file'])
