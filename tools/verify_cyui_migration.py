# Copyright (c) 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only
"""Render migrated views and check the actual selective compilation graph."""
import argparse
import json
from pathlib import Path
import subprocess
from cyui_dependency import REVISION


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--build-dir',type=Path,default=Path('build/cyui-migration'))
    parser.add_argument('--output',type=Path,default=Path('captures/cyui-migration'))
    args=parser.parse_args();root=Path(__file__).resolve().parents[1]
    build=(root/args.build_dir).resolve();output=(root/args.output).resolve();output.mkdir(parents=True,exist_ok=True)
    graph=(build/'build.ninja').read_text(encoding='utf-8').replace(chr(92),'/')
    for forbidden in ('cyui_audio.dir','cyui_gallery.dir','cyui_webcef.dir','cyui_nodes.dir','imgui_demo.cpp.obj'):
        assert forbidden not in graph,forbidden
    assert graph.count('build CMakeFiles/quest_imgui.dir/')==4,'One host ImGui with four compilation units is required'
    assert 'std:c++17' in graph.split('src/app.cpp.obj:')[1].split('\n\n')[0]
    assert 'c++20' in graph.split('src/log_view.cpp.obj:')[1].split('\n\n')[0]
    metadata=json.loads((build/'UI-BUILD.json').read_text());assert metadata['cyuiRevision']==REVISION
    cases=[]
    views={'raw':['--log-raw'],'metrics':['--page','8'],'chat':['--page','6'],'image':['--page','3'],'terminal':['--page','4'],'json':['--page','5','--plugin','meta.ovr-metrics','--plugin-tab','2']}
    for theme in ('navy','mid-grey','light-grey','amber-night','violet'):
        for compact in (False,True):
            for name,flags in views.items():
                cases.append((f'{name}-{theme}-{int(compact)}',['--theme',theme,'--locale','fr']+(['--compact'] if compact else [])+flags))
    for dpi in ('1.25','1.5'):
        for name in ('raw','metrics','chat','image'):
            cases.append((f'{name}-dpi-{dpi}',['--dpi',dpi,'--locale','en',*views[name]]))
    cases.append(('raw-compare',['--compare','--log-raw','--locale','fr']))
    for name,flags in cases:
        path=output/(name+'.bmp')
        result=subprocess.run([str(build/'CyVR_QuestDev.exe'),'--demo','--ui-fixtures','--capture',str(path),'--capture-after-ms','300',*flags],cwd=root,capture_output=True,timeout=30)
        assert result.returncode==0,(name,result.returncode,result.stderr.decode(errors='replace'))
        assert path.stat().st_size>10000,name
        stats=Path(str(path)+'.stats.txt').read_text(encoding='utf-8');assert 'demo=1\n' in stats and 'foreign_visible=0\n' in stats,(name,stats)
        locale=flags[flags.index('--locale')+1]
        assert f'locale={locale}\n' in stats,(name,stats)
        assert f'density={"compact" if "--compact" in flags else "comfortable"}\n' in stats,(name,stats)
        if '--dpi' in flags:
            values=dict(line.split('=',1) for line in stats.splitlines() if '=' in line)
            assert abs(float(values['dpi'])-float(flags[flags.index('--dpi')+1]))<.001,(name,stats)
        if name.startswith('raw'):assert 'log_raw=1\n' in stats,(name,stats)
        if name.startswith('chat'):assert 'chat_messages=2\n' in stats,(name,stats)
        if name.startswith('image'):assert 'image_loaded=1\n' in stats,(name,stats)
        print(name+': OK',flush=True)
    (output/'result.json').write_text(json.dumps({'renders':len(cases),'revision':REVISION,'themes':5,'densities':2,'dpi':[1,1.25,1.5]},indent=2))
    print('Selective build graph and migrated renders: OK')


if __name__=='__main__':main()
