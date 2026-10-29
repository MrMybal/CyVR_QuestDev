# Copyright (c) 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only

$ErrorActionPreference = 'Stop'
$questProductRoot = Split-Path $PSScriptRoot
if (-not (Test-Path "$PSScriptRoot\python\Scripts\python.exe")) { & python -m venv "$PSScriptRoot\python" }
& "$PSScriptRoot\python\Scripts\python.exe" -m pip install --keyring-provider disabled --disable-pip-version-check -c "$PSScriptRoot\requirements.lock" "$PSScriptRoot\wheels\cytools_core-0.10.1-py3-none-any.whl[mcp]"
if ($LASTEXITCODE) { throw 'Installation du runtime QuestDev échouée.' }
& "$PSScriptRoot\python\Scripts\python.exe" "$questProductRoot\assistant\contract.py"
