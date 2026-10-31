# Copyright (c) 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only
"""Exercise the separately registered installer test build on disposable files."""
import argparse
import ctypes
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time
import winreg

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'updater'))
from common import digest, read_json, write_json
from install import prepare
from verify_updates import stamp, close, wait_file, K

TEST_NAME = 'CyVR QuestDev Installer Test'
TEST_KEY = r'Software\Microsoft\Windows\CurrentVersion\Uninstall\{2642838C-4031-4E54-B92D-670214F669C8}_is1'


def registration():
    try:
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, TEST_KEY, 0, winreg.KEY_READ | winreg.KEY_WOW64_64KEY) as key:
            return {name: winreg.QueryValueEx(key, name)[0] for name in ['InstallLocation', 'DisplayVersion']}
    except FileNotFoundError:
        return None


def run(setup, target, log, *extra):
    result = subprocess.run([str(setup), '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/SP-', '/LANG=french', '/TASKS=desktopicon', f'/DIR={target}', f'/LOG={log}', *extra], timeout=90, creationflags=subprocess.CREATE_NO_WINDOW).returncode
    if log.exists():
        assert 'Runtime error' not in log.read_text(encoding='utf-8-sig', errors='replace'), 'Installer script runtime error'
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--setup', type=Path, required=True)
    parser.add_argument('--product', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    work = args.output.resolve()
    if not work.is_relative_to(ROOT / 'build'):
        raise RuntimeError('Installer verification output must be a new directory under build/')
    work.mkdir(parents=True, exist_ok=False)
    if registration() is not None:
        raise RuntimeError('An earlier installer test is still registered; preserve it and uninstall it first')
    with winreg.OpenKey(winreg.HKEY_CURRENT_USER, r'Software\Microsoft\Windows\CurrentVersion\Explorer\Shell Folders') as key:
        shortcuts = [Path(winreg.QueryValueEx(key, name)[0]) / (TEST_NAME + '.lnk') for name in ['Programs', 'Desktop']]
    if any(p.exists() for p in shortcuts):
        raise RuntimeError('An earlier test shortcut exists; preserve it')
    setup, source = args.setup.resolve(), args.product.resolve()
    target = work / 'Installation équipe'
    children, helpers = [], []
    env = dict(os.environ)
    env['LOCALAPPDATA'] = str(work / 'profile')
    env['PATH'] = str(Path(os.environ['SystemRoot']) / 'System32')
    for key in ['PYTHONHOME', 'PYTHONPATH', 'VIRTUAL_ENV', 'ANDROID_HOME', 'ANDROID_SDK_ROOT']:
        env.pop(key, None)
    data = work / 'profile/CyVR/QuestDev/updates'
    preference = data.parent / 'fixture-preference.txt'
    preference.parent.mkdir(parents=True)
    preference.write_text('preserve fixture settings')
    try:
        unrelated = work / 'Unrelated folder'
        unrelated.mkdir()
        (unrelated / 'personal.txt').write_text('untouched')
        assert run(setup, unrelated, work / 'rejected.log') != 0
        assert sorted(p.name for p in unrelated.iterdir()) == ['personal.txt']
        assert run(setup, target, work / 'install.log') == 0
        registered = registration()
        assert registered and Path(registered['InstallLocation']) == target
        assert registered['DisplayVersion'] == read_json(source / 'RELEASE.json')['version']
        assert all(p.is_file() for p in shortcuts)
        for name, sha in read_json(source / 'UPDATE-MANIFEST.json')['files'].items():
            assert digest(target / name) == sha, name
        personal = target / 'personal.txt'
        personal.write_text('preserve fixture document')
        assert run(setup, target, work / 'reinstall.log') == 0
        assert personal.read_text() == 'preserve fixture document'
        uninstaller = target / 'unins000.exe'
        uninstaller_sha = digest(uninstaller)
        print('Install/reinstall: per-user registration, shortcuts, complete payload and personal files OK', flush=True)

        # Simulate a new release which introduces a managed file unknown to the
        # original Inno log, and exercise the actual detached native restart.
        future = work / 'Future release'
        shutil.copytree(source, future)
        (future / 'plugins/added-by-update.txt').write_text('fixture managed component')
        names = sorted({p.relative_to(future).as_posix() for p in future.rglob('*') if p.is_file()})
        (future / 'INSTALL-FILES.txt').write_text('\n'.join(names) + '\n', encoding='utf-8-sig')
        manifest = read_json(future / 'UPDATE-MANIFEST.json')
        manifest['files'] = {name: digest(future / name) for name in names if name != 'UPDATE-MANIFEST.json'}
        write_json(future / 'UPDATE-MANIFEST.json', manifest)
        parent = subprocess.Popen([str(target / 'CyVR_QuestDev.exe'), '--demo'], cwd=work, env=env, creationflags=subprocess.CREATE_NO_WINDOW)
        children.append(parent.pid)
        time.sleep(1)
        for action in ['update', 'rollback', 'update']:
            pid = children[-1]
            origin = Path(read_json(data / 'last-install.json')['backup']) if action == 'rollback' else future
            value = prepare(target, origin, data, target / 'runtime/python/Scripts/python.exe', pid, stamp(pid), action == 'rollback')
            helper = subprocess.Popen([value['python'], '-I', '-S', value['script'], '--plan', value['plan']], cwd=Path(value['python']).parent, env=env, creationflags=subprocess.CREATE_NO_WINDOW)
            helpers.append(helper)
            assert wait_file(Path(value['ready']))['ok']
            close(pid)
            result = wait_file(Path(value['plan']).parent / 'result.json')
            assert result['ok'], result
            helper.wait(timeout=10)
            assert helper.returncode == 0
            children.append(read_json(Path(value['plan']).parent / 'health.json')['pid'])
            assert digest(uninstaller) == uninstaller_sha
            assert personal.read_text() == 'preserve fixture document'
            assert registration() and all(p.is_file() for p in shortcuts)
            assert (target / 'plugins/added-by-update.txt').exists() == (action == 'update')
            print(action + ': native/Core readiness, uninstaller, shortcuts and personal files OK', flush=True)
    finally:
        for pid in reversed(children):
            handle = K.OpenProcess(0x100000, False, pid)
            if handle:
                close(pid)
                K.WaitForSingleObject(handle, 20000)
                K.CloseHandle(handle)
        for helper in helpers:
            if helper.poll() is None:
                helper.terminate()
                helper.wait(timeout=10)
        if (target / 'unins000.exe').is_file():
            assert target.resolve().is_relative_to(work)
            code = subprocess.run([str(target / 'unins000.exe'), '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', f'/LOG={work / "uninstall.log"}'], timeout=90, creationflags=subprocess.CREATE_NO_WINDOW).returncode
            assert code == 0, code
    assert registration() is None
    assert not any(p.exists() for p in shortcuts)
    assert not (target / 'CyVR_QuestDev.exe').exists()
    assert not (target / 'plugins/added-by-update.txt').exists()
    assert (target / 'personal.txt').read_text() == 'preserve fixture document'
    assert preference.read_text() == 'preserve fixture settings'
    # Inno's temporary uninstall process removes the original EXE on exit.
    deadline = time.monotonic() + 10
    while (target / 'unins000.exe').exists() and time.monotonic() < deadline:
        time.sleep(.1)
    remaining = sorted(p.relative_to(target).as_posix() for p in target.rglob('*') if p.is_file())
    assert remaining == ['personal.txt'], remaining
    print('Uninstall: initial and update-added components removed; preferences and personal files preserved', flush=True)
    (work / 'result.json').write_text(json.dumps({'ok': True, 'checks': ['installation', 'shortcuts', 'reinstallation', 'native update', 'rollback', 'uninstallation', 'personal data preserved']}), encoding='utf8')


if __name__ == '__main__':
    main()
