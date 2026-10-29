# Copyright (c) 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only

"""Capture-only smoke check on the selected Quest; no device settings changed."""
import argparse
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import shutil
import subprocess
import time


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--serial',required=True);parser.add_argument('--adb',default=shutil.which('adb'));options=parser.parse_args()
    root=Path(__file__).resolve().parents[1];output=root/'captures/device-check';output.mkdir(parents=True,exist_ok=True)
    def adb(*arguments,timeout=25):
        result=subprocess.run([options.adb,'-s',options.serial,*arguments],capture_output=True,timeout=timeout)
        if result.returncode:raise RuntimeError(result.stderr.decode(errors='replace') or result.stdout.decode(errors='replace'))
        return result.stdout
    def screenshot():
        data=adb('exec-out','screencap','-p');assert data.startswith(b'\x89PNG\r\n\x1a\n');(output/'screen.png').write_bytes(data)
        print(f'Screenshot: valid PNG ({len(data)} bytes)',flush=True)
    def trace():
        remote=f'/data/misc/perfetto-traces/questdev-test-{time.time_ns()}.perfetto-trace'
        try:
            adb('shell','perfetto','-o',remote,'-t','2s','-b','32mb','sched','freq','idle','am','wm','gfx','view','binder_driver')
            path=output/'capture.perfetto-trace';adb('pull',remote,str(path));assert path.stat().st_size>100
            print(f'Perfetto: {path.stat().st_size} bytes',flush=True)
        finally:adb('shell','rm','-f',remote)
    def video():
        suffix=str(time.time_ns());remote=f'/sdcard/questdev-test-{suffix}.mp4';pid=f'/data/local/tmp/questdev-test-{suffix}.pid'
        script=f'screenrecord --time-limit 8 {remote} & p=$!; echo $p > {pid}; wait $p; test -s {remote}'
        process=subprocess.Popen([options.adb,'-s',options.serial,'shell',script],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
        try:
            time.sleep(2)
            adb('shell',f'p=$(cat {pid} 2>/dev/null); test -n "$p" && grep -q \'{remote}\' /proc/$p/cmdline && kill -2 $p')
            _,error=process.communicate(timeout=15);assert process.returncode==0,error
            path=output/'recording.mp4';adb('pull',remote,str(path));data=path.read_bytes();assert b'ftyp' in data[:32] and b'moov' in data
            print(f'Video: finalized MP4 after early Stop ({len(data)} bytes)',flush=True)
        finally:
            if process.poll() is None:process.wait(timeout=12)
            adb('shell','rm','-f',remote,pid)
    with ThreadPoolExecutor(max_workers=3) as executor:
        futures=[executor.submit(fn) for fn in (screenshot,trace,video)]
        for future in futures:future.result()


if __name__=='__main__':main()
