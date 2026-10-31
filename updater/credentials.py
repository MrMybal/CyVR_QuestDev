# Copyright (c) 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only
"""User-scoped Windows credential store; values never enter update state files."""
import ctypes
from ctypes import wintypes as W
import os
from common import UpdateError
from github import validate_token

TARGET='CyVR.QuestDev.GitHubUpdates'


class Credential(ctypes.Structure):
    _fields_=[('Flags',W.DWORD),('Type',W.DWORD),('TargetName',W.LPWSTR),('Comment',W.LPWSTR),('LastWritten',W.FILETIME),('CredentialBlobSize',W.DWORD),('CredentialBlob',ctypes.POINTER(ctypes.c_ubyte)),('Persist',W.DWORD),('AttributeCount',W.DWORD),('Attributes',ctypes.c_void_p),('TargetAlias',W.LPWSTR),('UserName',W.LPWSTR)]


def api():
    if os.name!='nt':raise UpdateError('Le Gestionnaire d’identifiants nécessite Windows.')
    dll=ctypes.WinDLL('advapi32',use_last_error=True)
    dll.CredReadW.argtypes=[W.LPCWSTR,W.DWORD,W.DWORD,ctypes.POINTER(ctypes.POINTER(Credential))]
    dll.CredWriteW.argtypes=[ctypes.POINTER(Credential),W.DWORD]
    dll.CredDeleteW.argtypes=[W.LPCWSTR,W.DWORD,W.DWORD];dll.CredFree.argtypes=[ctypes.c_void_p]
    return dll


def read():
    dll=api();value=ctypes.POINTER(Credential)()
    if not dll.CredReadW(TARGET,1,0,ctypes.byref(value)):
        if ctypes.get_last_error()==1168:return ''
        raise UpdateError('Lecture du Gestionnaire d’identifiants impossible.')
    try:
        try:return validate_token(ctypes.string_at(value.contents.CredentialBlob,value.contents.CredentialBlobSize).decode('utf8'))
        except UnicodeError:raise UpdateError('Le jeton Windows n’est pas reconnu. Enregistrez-le de nouveau dans QuestDev.') from None
    finally:dll.CredFree(value)


def store(token):
    raw=validate_token(token).encode('utf8');blob=(ctypes.c_ubyte*len(raw)).from_buffer_copy(raw)
    item=Credential();item.Type=1;item.TargetName=TARGET;item.UserName='GitHub';item.Persist=2;item.CredentialBlobSize=len(raw);item.CredentialBlob=blob
    if not api().CredWriteW(ctypes.byref(item),0):raise UpdateError('Enregistrement du jeton dans Windows impossible.')
    ctypes.memset(blob,0,len(raw))


def forget():
    if not api().CredDeleteW(TARGET,1,0) and ctypes.get_last_error()!=1168:raise UpdateError('Suppression du jeton Windows impossible.')
