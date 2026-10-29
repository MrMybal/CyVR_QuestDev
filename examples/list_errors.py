# Copyright (c) 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only

"""Run with runtime/python/Scripts/python.exe while QuestDev is open."""
from pathlib import Path
import json
import sys

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'assistant'))
from sdk import QuestDevClient

with QuestDevClient() as quest:
    current=quest.invoke('get_context')['context']
    result=quest.invoke('read_logs',{'package':current.get('selectedPackage',''),'minimum':'E','limit':100})
    print(json.dumps(result,ensure_ascii=False,indent=2))
