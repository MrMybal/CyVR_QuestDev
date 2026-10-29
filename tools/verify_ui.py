"""Render UI checks. --live additionally reads logs from a connected Quest."""
import argparse
import os
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--live', action='store_true')
    parser.add_argument('--package', default='com.cyvr.player')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    output = root / 'captures'
    output.mkdir(exist_ok=True)
    exe = root / 'build/windows/CyVR_QuestDev.exe'
    environment = {key.upper(): value for key, value in os.environ.items()}
    cases = [(theme, ['--demo', '--theme', theme]) for theme in ('navy', 'mid-grey', 'light-grey', 'amber-night', 'violet')]
    cases += [('compare', ['--demo', '--compare'])]
    cases += [(name,['--demo','--page',str(page)]) for page,name in enumerate(('console','applications','settings','device-actions','adb','plugins','assistant','files'))]
    cases += [('files-'+theme,['--demo','--page','7','--theme',theme]) for theme in ('mid-grey','light-grey','amber-night','violet')]
    for plugin,label in (('meta.ovr-metrics','ovr-metrics'),('cyinsights.ovr','cyinsight-ovr')):
        cases += [(label+'-'+theme,['--demo','--page','5','--plugin',plugin,'--theme',theme]) for theme in ('navy','mid-grey','light-grey','amber-night','violet')]
        cases += [(label+'-connection',['--demo','--page','5','--plugin',plugin,'--plugin-tab','1'])]
        cases += [(label+'-state',['--demo','--page','5','--plugin',plugin,'--plugin-tab','2'])]
    if args.live:
        cases.append(('quest-live', ['--package', args.package, '--capture-after-ms', '6000']))
    for name, options in cases:
        path = output / (name + '.bmp')
        command = [str(exe), '--capture', str(path), '--capture-after-ms', '300'] + options
        process = subprocess.run(command, cwd=root, env=environment, capture_output=True, timeout=20)
        if process.returncode:
            raise RuntimeError(f'{name}: exit {process.returncode}: {process.stderr.decode(errors="replace")}')
        if not path.exists() or path.stat().st_size < 1000:
            raise RuntimeError(f'{name}: missing screenshot')
        pixels=path.read_bytes()[54:]
        if len({pixels[i:i+4] for i in range(0,len(pixels),400)})<15:
            raise RuntimeError(f'{name}: blank screenshot')
        stats = Path(str(path) + '.stats.txt').read_text(encoding='utf-8')
        if name == 'quest-live':
            if 'online=1\n' not in stats or 'capturing=1\n' not in stats or 'foreign_visible=0\n' not in stats:
                raise RuntimeError(stats)
            values = dict(line.split('=', 1) for line in stats.splitlines() if '=' in line and ' lines=' not in line)
            if int(values.get('selected_lines', '0')) == 0:
                raise RuntimeError('The selected app emitted no logs during the capture.\n' + stats)
        print(f'{name}: OK')
    print(f'Screenshots: {output}')


if __name__ == '__main__':
    main()
