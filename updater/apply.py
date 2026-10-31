# Copyright (c) 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only
"""Detached bootstrap, copied outside the application directory before launch."""
import argparse
import ctypes
from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parent))
from common import UpdateError,write_json
from install import apply

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--plan',type=Path,required=True);args=parser.parse_args()
    try:apply(args.plan)
    except Exception as error:
        text=str(error) if isinstance(error,UpdateError) else 'Installation impossible. La version précédente a été conservée ou restaurée. Consultez le dossier des mises à jour.'
        write_json(args.plan.parent/'ready.json',{'ok':False,'error':text})
        write_json(args.plan.parent/'result.json',{'ok':False,'error':text})
        ctypes.windll.user32.MessageBoxW(None,text,'CyVR QuestDev — mise à jour',0x10)
        sys.exit(1)
