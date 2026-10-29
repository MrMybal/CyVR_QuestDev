# Copyright (c) 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only

param([switch]$Test, [switch]$Run, [switch]$Clean, [string]$CyUIRoot = $env:CYUI_ROOT)
$ErrorActionPreference = 'Stop'
$questBuildArgs = @("$PSScriptRoot\tools\build.py")
if ($CyUIRoot) { $questBuildArgs += @('--cyui', $CyUIRoot) }
if ($Test) { $questBuildArgs += '--test' }
if ($Run) { $questBuildArgs += '--run' }
if ($Clean) { $questBuildArgs += '--clean' }
& python @questBuildArgs
if ($LASTEXITCODE) { throw 'La compilation ou les tests ont échoué.' }
