# Copyright (c) 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only
"""Export the approved PNG as a Windows icon (development-only Pillow)."""
from pathlib import Path
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]

if __name__=='__main__':
    folder=ROOT/'assets/branding'
    with Image.open(folder/'cyvr-questdev-logo-gradient.png') as logo:
        logo.convert('RGBA').save(folder/'cyvr-questdev.ico',format='ICO',sizes=[(size,size) for size in (16,20,24,32,40,48,64,96,128,256)])
    print('Windows icon exported from approved PNG: 16–256 px.')
