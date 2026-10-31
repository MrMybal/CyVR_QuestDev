# Copyright (c) 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only
"""Exercise the detached portable updater and rollback on disposable copies."""
import argparse
import ctypes
from ctypes import wintypes as W
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'updater'))
from common import read_json,digest
from install import prepare

K=ctypes.WinDLL('kernel32',use_last_error=True);U=ctypes.WinDLL('user32',use_last_error=True)
K.OpenProcess.argtypes=[W.DWORD,W.BOOL,W.DWORD];K.OpenProcess.restype=W.HANDLE
K.GetProcessTimes.argtypes=[W.HANDLE,*([ctypes.POINTER(W.FILETIME)]*4)]
K.WaitForSingleObject.argtypes=[W.HANDLE,W.DWORD];K.CloseHandle.argtypes=[W.HANDLE]
U.GetWindowThreadProcessId.argtypes=[W.HWND,ctypes.POINTER(W.DWORD)];U.PostMessageW.argtypes=[W.HWND,W.UINT,W.WPARAM,W.LPARAM]
CALLBACK=ctypes.WINFUNCTYPE(W.BOOL,W.HWND,W.LPARAM);U.EnumWindows.argtypes=[CALLBACK,W.LPARAM]


def stamp(pid):
    handle=K.OpenProcess(0x1000,False,pid);assert handle
    try:
        values=[W.FILETIME() for _ in range(4)];assert K.GetProcessTimes(handle,*[ctypes.byref(v) for v in values])
        return (values[0].dwHighDateTime<<32)|values[0].dwLowDateTime
    finally:K.CloseHandle(handle)


def close(pid):
    @CALLBACK
    def visit(window,param):
        owner=W.DWORD();U.GetWindowThreadProcessId(window,ctypes.byref(owner))
        if owner.value==pid:U.PostMessageW(window,0x10,0,0)
        return True
    U.EnumWindows(visit,0)


def wait_file(path,timeout=60):
    deadline=time.monotonic()+timeout
    while not path.exists():
        assert time.monotonic()<deadline,'Updater did not acknowledge'
        time.sleep(.1)
    return read_json(path)


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--product',type=Path,required=True);parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
    work=args.output.resolve();work.mkdir(parents=True,exist_ok=False);target=work/'Application équipe';source=args.product.resolve()
    shutil.copytree(source,target)
    (target/'personal.ini').write_text('fixture preference',encoding='utf8')
    (target/'plugins/personal.txt').write_text('fixture personal plugin',encoding='utf8')
    env=dict(os.environ);env['LOCALAPPDATA']=str(work/'profile');env['PATH']=str(Path(os.environ['SystemRoot'])/'System32')
    for key in ['PYTHONPATH','PYTHONHOME','VIRTUAL_ENV','ANDROID_HOME','ANDROID_SDK_ROOT']:env.pop(key,None)
    data=work/'profile/CyVR/QuestDev/updates';parent=subprocess.Popen([str(target/'CyVR_QuestDev.exe'),'--demo','--updates-settings'],cwd=work,env=env,creationflags=subprocess.CREATE_NO_WINDOW)
    children=[parent.pid];helpers=[]
    try:
        time.sleep(.8)
        for rollback in [False,True]:
            pid=children[-1];origin=source if not rollback else Path(read_json(data/'last-install.json')['backup'])
            value=prepare(target,origin,data,target/'runtime/python/Scripts/python.exe',pid,stamp(pid),rollback)
            helper=subprocess.Popen([value['python'],'-I','-S',value['script'],'--plan',value['plan']],cwd=Path(value['python']).parent,env=env,creationflags=subprocess.CREATE_NO_WINDOW);helpers.append(helper)
            ready=wait_file(Path(value['ready']));assert ready.get('ok'),ready
            assert (target/'CyVR_QuestDev.exe').exists(),'Replaced running application before parent exit'
            close(pid)
            result=wait_file(Path(value['plan']).parent/'result.json');assert result['ok'],result
            helper.wait(timeout=10);assert helper.returncode==0
            health=read_json(Path(value['plan']).parent/'health.json');children.append(health['pid'])
            assert health['version']==read_json(target/'RELEASE.json')['version']
            assert (target/'personal.ini').read_text(encoding='utf8')=='fixture preference'
            assert (target/'plugins/personal.txt').read_text(encoding='utf8')=='fixture personal plugin'
            if rollback:assert (target/'after-update.txt').read_text()=='preserved during rollback'
            else:(target/'after-update.txt').write_text('preserved during rollback')
            assert digest(target/'CyVR_QuestDev.exe')==digest(source/'CyVR_QuestDev.exe')
            print(('Rollback' if rollback else 'Install')+': detached embedded Python, native restart/Core readiness, backup and personal files OK',flush=True)
    finally:
        for pid in reversed(children):
            handle=K.OpenProcess(0x100000,False,pid)
            if handle:
                close(pid);K.WaitForSingleObject(handle,20000);K.CloseHandle(handle)
        for helper in helpers:
            if helper.poll() is None:helper.terminate();helper.wait(timeout=10)
        if parent.poll() is None:parent.terminate();parent.wait(timeout=10)
    assert not (data.parent/'assistant/connection.json').exists()


if __name__=='__main__':main()
