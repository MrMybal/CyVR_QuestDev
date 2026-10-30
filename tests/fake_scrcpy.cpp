// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
static LRESULT CALLBACK windowProc(HWND window,UINT message,WPARAM wparam,LPARAM lparam){if(message==WM_DESTROY){PostQuitMessage(0);return 0;}return DefWindowProcW(window,message,wparam,lparam);}
int wmain(int argc,wchar_t** argv){
    wchar_t mode[32]{},adb[32768]{};GetEnvironmentVariableW(L"QUEST_MIRROR_FAKE_MODE",mode,32);GetEnvironmentVariableW(L"ADB",adb,32768);
    if(std::wstring(mode)==L"fail"){std::cerr<<"ERROR: Fixture encoder refused\n"<<std::flush;return 7;}
    std::wstring title=L"Fake scrcpy";std::filesystem::path file;
    for(int i=1;i<argc;++i){std::wstring arg=argv[i];if(arg.rfind(L"--window-title=",0)==0)title=arg.substr(15);if(arg.rfind(L"--record=",0)==0)file=arg.substr(9);}
    if(!file.empty()){std::ofstream out(file,std::ios::binary);out<<"FAKE_MKV_START\n";}
    WNDCLASSW type{};type.lpfnWndProc=windowProc;type.hInstance=GetModuleHandleW(nullptr);type.lpszClassName=L"QuestFakeScrcpy";RegisterClassW(&type);
    auto window=CreateWindowW(type.lpszClassName,title.c_str(),WS_OVERLAPPEDWINDOW|WS_VISIBLE,-20000,-20000,320,200,nullptr,nullptr,type.hInstance,nullptr);
    if(!window)return 4;if(std::wstring(mode)==L"slow")Sleep(700);
    std::cout<<"INFO: Fixture ADB environment "<<(std::filesystem::path(adb).filename()==L"fake_adb.exe"?"OK":"OTHER")<<"\nINFO: Renderer: fixture\nINFO: Texture: 1600x900\nINFO: 60 fps\n"<<std::flush;
    MSG message{};while(GetMessageW(&message,nullptr,0,0)>0){TranslateMessage(&message);DispatchMessageW(&message);}
    if(!file.empty()){std::ofstream out(file,std::ios::binary|std::ios::app);out<<"FINALIZED\n";}
    return 0;
}
