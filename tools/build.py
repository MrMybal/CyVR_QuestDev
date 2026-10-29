"""Build the native app with the Visual Studio toolchain and CMake/Ninja."""
import argparse
import os
from pathlib import Path
import subprocess


def main():
    root = Path(__file__).resolve().parents[1]
    args = argparse.ArgumentParser()
    args.add_argument('--test', action='store_true')
    args.add_argument('--run', action='store_true')
    args.add_argument('--clean', action='store_true')
    args.add_argument('--cyui', default=os.environ.get('CYUI_ROOT'))
    options = args.parse_args()
    cyui = Path(options.cyui).expanduser().resolve() if options.cyui else next(
        (parent / 'CyImgui_Interface' for parent in root.parents
         if (parent / 'CyImgui_Interface/include/cyui/CyUI.h').is_file()),
        root.parent / 'CyImgui_Interface')
    if not (cyui / 'CMakeLists.txt').is_file():
        raise SystemExit('CyUI introuvable : définir CYUI_ROOT ou passer --cyui avec son dossier source.')
    environment = {key.upper(): value for key, value in os.environ.items()}
    vswhere = Path(environment.get('PROGRAMFILES(X86)', r'C:\Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
    vs = Path(subprocess.check_output([str(vswhere), '-latest', '-products', '*', '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], env=environment, text=True).strip())
    vcvars = vs / 'VC/Auxiliary/Build/vcvars64.bat'
    setup = subprocess.check_output(f'call "{vcvars}" >nul && set', shell=True, env=environment, text=True, errors='replace')
    configured = {}
    for line in setup.splitlines():
        key, separator, value = line.partition('=')
        if separator and key:
            if key == 'Path' and 'PATH' in configured:
                continue
            configured[key.upper()] = value
    cmake_root = vs / 'Common7/IDE/CommonExtensions/Microsoft/CMake'
    configured['VSLANG'] = '1033'
    cmake = str(cmake_root / 'CMake/bin/cmake.exe')
    ninja = str(cmake_root / 'Ninja/ninja.exe')
    build = root / 'build/windows'
    compiler = str(Path(configured['VCTOOLSINSTALLDIR']) / 'bin/Hostx64/x64/cl.exe')
    subprocess.run([cmake, '-S', str(root), '-B', str(build), '-G', 'Ninja', f'-DCMAKE_MAKE_PROGRAM={ninja}', f'-DCMAKE_CXX_COMPILER={compiler}', '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_CXX_FLAGS_RELEASE=/O2 /Ob2 /DNDEBUG', f'-DCYUI_ROOT={cyui}', '-DBUILD_TESTING=ON'], env=configured, check=True)
    build_command = [cmake, '--build', str(build), '--parallel', '8']
    if options.clean:
        build_command.append('--clean-first')
    subprocess.run(build_command, env=configured, check=True)
    if options.test:
        ctest = str(cmake_root / 'CMake/bin/ctest.exe')
        subprocess.run([ctest, '--test-dir', str(build), '--output-on-failure'], env=configured, check=True)
    if options.run:
        subprocess.Popen([str(build / 'CyVR_QuestDev.exe')], cwd=root, env=environment)


if __name__ == '__main__':
    main()
