# Copyright (c) 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only

"""CLI / official MCP bridge to the running QuestDev CyToolsCore service."""
import argparse
import asyncio
import json
import os
from pathlib import Path
from cytools_core.transports.http import HTTPClient
from cytools_core.transports.mcp import serve


def client(connection=None):
    path=Path(connection or os.environ.get('QUESTDEV_CONNECTION',Path(os.environ.get('LOCALAPPDATA',Path.home()))/'CyVR/QuestDev/assistant/connection.json'))
    data=json.loads(path.read_text(encoding='utf-8'))
    return HTTPClient(data['url'],data['token'],timeout=30)


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('mode',choices=['mcp','describe','call'],nargs='?',default='mcp')
    parser.add_argument('--connection')
    parser.add_argument('--method',default='DescribeTool')
    parser.add_argument('--parameters',default='{}')
    options=parser.parse_args()
    c=client(options.connection)
    if options.mode=='mcp':
        asyncio.run(serve(c))
    else:
        print(json.dumps(c.call('DescribeTool' if options.mode=='describe' else options.method,**json.loads(options.parameters)),ensure_ascii=False))


if __name__=='__main__':
    main()
