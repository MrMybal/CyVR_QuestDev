# Copyright (c) 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only

param([switch]$Test, [switch]$Run, [switch]$Clean, [string]$CyUIRoot = $env:CYUI_ROOT, [string]$BuildDirectory = 'build/windows')
$ErrorActionPreference = 'Stop'
$questBuildArgs = @("$PSScriptRoot\tools\build.py")
if ($CyUIRoot) { $questBuildArgs += @('--cyui', $CyUIRoot) }
$questBuildArgs += @('--build-dir', $BuildDirectory)
if ($Test) { $questBuildArgs += '--test' }
if ($Run) { $questBuildArgs += '--run' }
if ($Clean) { $questBuildArgs += '--clean' }
& python @questBuildArgs
if ($LASTEXITCODE) { throw 'La compilation ou les tests ont échoué.' }
