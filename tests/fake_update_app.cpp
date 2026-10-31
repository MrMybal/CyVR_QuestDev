// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <string>
int wmain(int argc,wchar_t** argv){
    wchar_t buffer[32768]{};GetModuleFileNameW(nullptr,buffer,32768);auto root=std::filesystem::path(buffer).parent_path();
    if(std::filesystem::exists(root/L"FAIL_START"))return 7;
    std::string version;std::ifstream(root/L"VERSION")>>version;
    for(int i=1;i+1<argc;++i)if(std::wstring(argv[i])==L"--update-ready-file"){
        auto path=std::filesystem::path(argv[i+1]),temp=path;temp+=L".tmp";
        std::ofstream out(temp);out<<"{\"ok\":true,\"version\":\""<<version<<"\",\"pid\":"<<GetCurrentProcessId()<<"}";out.close();std::filesystem::rename(temp,path);
    }
    Sleep(60000);return 0;
}
