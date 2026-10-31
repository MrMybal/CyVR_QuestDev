# Copyright (c) 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only
"""Check saved retention/mode settings in isolated profiles, using only fake ADB."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'assistant'))
from sdk import QuestDevClient
from verify_updates import close


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(ROOT / 'build'):
        raise RuntimeError('Use a new verification directory under build/')
    output.mkdir(parents=True, exist_ok=False)
    cases = [
        ('saved', '250000', '256', True, 250000, 256),
        ('bounds', '-5', '4096', False, 1000, 2048),
        ('invalid', 'invalid', '999999999999999999999999', False, 50000, 64),
        ('saved-again', '250000', '256', True, 250000, 256),
    ]
    for name, lines, memory, split, expected_lines, expected_memory in cases:
        folder = output / ('saved' if name == 'saved-again' else name)
        profile = folder / 'CyVR/QuestDev'
        if name != 'saved-again':
            profile.mkdir(parents=True)
            (profile / 'settings.ini').write_text(f'log_lines={lines}\nlog_memory_mb={memory}\nfiles_side_by_side={int(split)}\nfiles_prepare_list={int(split)}\n', encoding='utf8')
        env = dict(os.environ, LOCALAPPDATA=str(folder))
        app = subprocess.Popen([str(ROOT / 'build/windows/CyVR_QuestDev.exe'), '--adb', str(ROOT / 'build/windows/fake_adb.exe'), '--log-settings'], env=env, creationflags=subprocess.CREATE_NO_WINDOW)
        connection = profile / 'assistant/connection.json'
        try:
            deadline = time.monotonic() + 25
            while not connection.exists():
                if app.poll() is not None:
                    raise RuntimeError(f'Isolated app exited: {app.returncode}')
                if time.monotonic() > deadline:
                    raise RuntimeError('Isolated Core did not become ready')
                time.sleep(.1)
            with QuestDevClient(connection) as quest:
                context = quest.invoke('get_context')['context']
                deadline = time.monotonic() + 5
                while 'logRetention' not in context:
                    if time.monotonic() > deadline:
                        raise RuntimeError('Native context snapshot did not become ready')
                    time.sleep(.1)
                    context = quest.invoke('get_context')['context']
                retention = context['logRetention']
                assert retention['maxLines'] == expected_lines, retention
                assert retention['maxBytes'] == expected_memory * 1024 * 1024, retention
                assert context['fileBrowser']['sideBySide'] is split
                assert context['fileBrowser']['prepareList'] is split
            print(name + ': saved limits, bounds and file modes OK', flush=True)
        finally:
            close(app.pid)
            try:
                app.wait(timeout=20)
            except subprocess.TimeoutExpired:
                app.terminate()
                app.wait(timeout=10)
        assert app.returncode == 0
        assert not connection.exists(), 'MCP discovery file survived normal shutdown'
    (output / 'result.json').write_text(json.dumps({'ok': True, 'starts': len(cases), 'profiles': 3}), encoding='utf8')


if __name__ == '__main__':
    main()
