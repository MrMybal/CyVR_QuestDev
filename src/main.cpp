// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only

// Win32 / DirectX 11 host, adapted from the CyUI and Dear ImGui examples.
// See THIRD_PARTY.md for dependency notices.

#include <cyui/CyUI.h>
#include "quest/app.hpp"
#include "quest/resources.h"

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <imgui_internal.h>

#include <d3d11.h>
#include <tchar.h>
#include <windows.h>

#include <shellapi.h>

#include <algorithm>
#include <memory>
#include <chrono>
#include <fstream>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace {

ID3D11Device* gDevice {};
ID3D11DeviceContext* gContext {};
IDXGISwapChain* gSwapChain {};
ID3D11RenderTargetView* gTarget {};
UINT gResizeWidth {}, gResizeHeight {};
float gDpiChange{};

void createTarget()
{
    ID3D11Texture2D* back {};
    gSwapChain->GetBuffer(0, IID_PPV_ARGS(&back));
    gDevice->CreateRenderTargetView(back, nullptr, &gTarget);
    back->Release();
}

void releaseTarget()
{
    if (gTarget != nullptr) {
        gTarget->Release();
        gTarget = nullptr;
    }
}

bool createDevice(HWND hwnd)
{
    DXGI_SWAP_CHAIN_DESC sd {};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    const D3D_FEATURE_LEVEL levels[] { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    D3D_FEATURE_LEVEL level {};
    auto result = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, 2, D3D11_SDK_VERSION, &sd,
                                                &gSwapChain, &gDevice, &level, &gContext);
    if (result == DXGI_ERROR_UNSUPPORTED)
        result = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, levels, 2, D3D11_SDK_VERSION, &sd,
                                               &gSwapChain, &gDevice, &level, &gContext);
    if (result != S_OK)
        return false;
    createTarget();
    return true;
}

void releaseDevice()
{
    releaseTarget();
    if (gSwapChain != nullptr)
        gSwapChain->Release();
    if (gContext != nullptr)
        gContext->Release();
    if (gDevice != nullptr)
        gDevice->Release();
}

// Writes the back buffer to a 32-bit BMP (the --capture option, for checks).
bool captureBackBuffer(const wchar_t* path)
{
    ID3D11Texture2D* back {};
    if (FAILED(gSwapChain->GetBuffer(0, IID_PPV_ARGS(&back))))
        return false;
    D3D11_TEXTURE2D_DESC desc {};
    back->GetDesc(&desc);
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.MiscFlags = 0;
    ID3D11Texture2D* staging {};
    if (FAILED(gDevice->CreateTexture2D(&desc, nullptr, &staging))) {
        back->Release();
        return false;
    }
    gContext->CopyResource(staging, back);
    back->Release();
    D3D11_MAPPED_SUBRESOURCE mapped {};
    if (FAILED(gContext->Map(staging, 0, D3D11_MAP_READ, 0, &mapped))) {
        staging->Release();
        return false;
    }
    const auto width = static_cast<int>(desc.Width);
    const auto height = static_cast<int>(desc.Height);
    std::vector<unsigned char> pixels(static_cast<std::size_t>(width) * height * 4);
    for (auto y = 0; y < height; ++y) {
        const auto* row = static_cast<const unsigned char*>(mapped.pData) + static_cast<std::size_t>(y) * mapped.RowPitch;
        for (auto x = 0; x < width; ++x) {
            auto* out = &pixels[(static_cast<std::size_t>(y) * width + x) * 4];
            out[0] = row[x * 4 + 2];
            out[1] = row[x * 4 + 1];
            out[2] = row[x * 4 + 0];
            out[3] = 255;
        }
    }
    gContext->Unmap(staging, 0);
    staging->Release();
    BITMAPFILEHEADER file {};
    BITMAPINFOHEADER info {};
    info.biSize = sizeof(info);
    info.biWidth = width;
    info.biHeight = -height;
    info.biPlanes = 1;
    info.biBitCount = 32;
    info.biCompression = BI_RGB;
    file.bfType = 0x4d42;
    file.bfOffBits = sizeof(file) + sizeof(info);
    file.bfSize = file.bfOffBits + static_cast<DWORD>(pixels.size());
    FILE* out {};
    if (_wfopen_s(&out, path, L"wb") != 0 || out == nullptr)
        return false;
    std::fwrite(&file, sizeof(file), 1, out);
    std::fwrite(&info, sizeof(info), 1, out);
    std::fwrite(pixels.data(), 1, pixels.size(), out);
    std::fclose(out);
    return true;
}

LRESULT WINAPI windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
        return true;
    switch (msg) {
    case WM_DPICHANGED: {
        gDpiChange=static_cast<float>(HIWORD(wParam))/96.0f;
        const auto* suggested=reinterpret_cast<const RECT*>(lParam);
        SetWindowPos(hwnd,nullptr,suggested->left,suggested->top,suggested->right-suggested->left,suggested->bottom-suggested->top,SWP_NOZORDER|SWP_NOACTIVATE);
        return 0;
    }
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED) {
            gResizeWidth = LOWORD(lParam);
            gResizeHeight = HIWORD(lParam);
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return ::DefWindowProcW(hwnd, msg, wParam, lParam);
}

} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    // --capture out.bmp [--theme key] [--comfortable] [--tab n]: renders a few
    // frames, writes the window to a BMP and quits.
    std::wstring capturePath;
    std::string themeKey = "navy";
    quest::Options options;
    int captureFrames = 80;
    int captureDelay = 1500;
    int count = 0;
    auto** args = CommandLineToArgvW(GetCommandLineW(), &count);
    for (int i = 1; i < count; ++i) {
        std::wstring arg = args[i];
        auto next = [&]() { return i + 1 < count ? std::wstring(args[++i]) : std::wstring(); };
        if (arg == L"--capture") capturePath = next();
        else if (arg == L"--theme") themeKey = quest::narrow(next());
        else if (arg == L"--demo") options.demo = true;
        else if (arg == L"--compare") options.compare = true;
        else if (arg == L"--adb") options.adb = next();
        else if (arg == L"--package") options.package = quest::narrow(next());
        else if (arg == L"--plugin") options.plugin = quest::narrow(next());
        else if (arg == L"--metrics-csv") options.metricsCsv = next();
        else if (arg == L"--scrcpy") options.scrcpy = next();
        else if (arg == L"--mirror-settings") options.mirrorSetup = true;
        else if (arg == L"--plugin-tab") options.pluginTab = std::clamp(_wtoi(next().c_str()),0,2);
        else if (arg == L"--page") options.page = _wtoi(next().c_str());
        else if (arg == L"--frames") captureFrames = std::max(20, _wtoi(next().c_str()));
        else if (arg == L"--capture-after-ms") captureDelay = std::max(0, _wtoi(next().c_str()));
    }
    LocalFree(args);
    if(!capturePath.empty())_set_error_mode(_OUT_TO_STDERR);
    // One live companion owns the canonical MCP connection for this profile.
    struct Instance {HANDLE mutex{};~Instance(){if(mutex)CloseHandle(mutex);}} instance;
    if(!options.demo){
        instance.mutex=CreateMutexW(nullptr,FALSE,L"Local\\CyVR.QuestDev.0.2");
        if(instance.mutex&&GetLastError()==ERROR_ALREADY_EXISTS){
            if(auto existing=FindWindowW(L"CyVRQuestDev02",nullptr)){ShowWindow(existing,SW_RESTORE);SetForegroundWindow(existing);}
            return 3;
        }
    }
    const auto comResult=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);

    ImGui_ImplWin32_EnableDpiAwareness();
    const auto dpiScale = ImGui_ImplWin32_GetDpiScaleForMonitor(::MonitorFromPoint(POINT { 0, 0 }, MONITOR_DEFAULTTOPRIMARY));

    WNDCLASSEXW wc { sizeof(wc), CS_CLASSDC, windowProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr,
                     L"CyVRQuestDev02", nullptr };
    wc.hIcon=static_cast<HICON>(LoadImageW(wc.hInstance,MAKEINTRESOURCEW(IDI_QUESTDEV_APP),IMAGE_ICON,GetSystemMetrics(SM_CXICON),GetSystemMetrics(SM_CYICON),LR_SHARED));
    wc.hIconSm=static_cast<HICON>(LoadImageW(wc.hInstance,MAKEINTRESOURCEW(IDI_QUESTDEV_APP),IMAGE_ICON,GetSystemMetrics(SM_CXSMICON),GetSystemMetrics(SM_CYSMICON),LR_SHARED));
    ::RegisterClassExW(&wc);
    const auto hwnd = ::CreateWindowW(wc.lpszClassName, L"CyVR QuestDev", WS_OVERLAPPEDWINDOW, 100, 100,
                                      static_cast<int>(1440 * dpiScale), static_cast<int>(900 * dpiScale), nullptr, nullptr,
                                      wc.hInstance, nullptr);
    if (!createDevice(hwnd)) {
        releaseDevice();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    if(!capturePath.empty())::ShowWindow(hwnd,SW_RESTORE);
    ::UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
#ifdef IMGUI_HAS_DOCK
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
#endif

    // CyUI: the theme, then the fonts.
    cyui::Config config;
    config.themeKey = themeKey.c_str();
    config.density = cyui::Density::comfortable;
    config.dpiScale = dpiScale;
    cyui::apply(config);
    cyui::loadFonts();

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(gDevice, gContext);

    if (!capturePath.empty())
        io.IniFilename = nullptr;

    options.graphicsDevice=gDevice;
    auto app = std::make_unique<quest::App>(options);
    if(!capturePath.empty())io.IniFilename=nullptr;
    const auto started=std::chrono::steady_clock::now();
    auto done = false;
    bool captureOk = true;
    auto frames = 0;
    while (!done) {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;
        if(gDpiChange>0){cyui::applyDpi(gDpiChange);gDpiChange=0;}
        if (gResizeWidth != 0 && gResizeHeight != 0) {
            releaseTarget();
            gSwapChain->ResizeBuffers(0, gResizeWidth, gResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            gResizeWidth = gResizeHeight = 0;
            createTarget();
        }

        if(!capturePath.empty()&&IsIconic(hwnd))::ShowWindow(hwnd,SW_RESTORE);
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        app->frame();
        ImGui::Render();

        const auto ground = cyui::vec(cyui::palette().bg0);
        const float clear[4] { ground.x, ground.y, ground.z, 1.0f };
        gContext->OMSetRenderTargets(1, &gTarget, nullptr);
        gContext->ClearRenderTargetView(gTarget, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        if (!capturePath.empty() && ++frames >= captureFrames && io.DisplaySize.x>0 && io.DisplaySize.y>0 && std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started).count()>=captureDelay) {
            captureOk = captureBackBuffer(capturePath.c_str());
            std::ofstream stats(capturePath+L".stats.txt");stats<<app->diagnostics();
            stats<<"display="<<io.DisplaySize.x<<"x"<<io.DisplaySize.y<<"\ndraw_vertices="<<ImGui::GetDrawData()->TotalVtxCount<<"\n";
            done = true;
        }
        gSwapChain->Present(1, 0);
    }

    app.reset();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    releaseDevice();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
    if(SUCCEEDED(comResult))CoUninitialize();
    return captureOk ? 0 : 2;
}
