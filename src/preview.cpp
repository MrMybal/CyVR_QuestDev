// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only

#include "quest/preview.hpp"
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincodec.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <vector>
namespace quest {
Preview::~Preview(){if(view_)view_->Release();}
bool Preview::load(const std::wstring& path,std::string& error) {
    return loadImage(path,0,error);
}
bool Preview::loadResource(int identifier,std::string& error) {
    return loadImage({},identifier,error);
}
bool Preview::loadImage(const std::wstring& path,int resource,std::string& error) {
    using Microsoft::WRL::ComPtr;error.clear();if(!device_){error="Le rendu DirectX n’est pas initialisé.";return false;}
    ComPtr<IWICImagingFactory> factory;ComPtr<IWICBitmapDecoder> decoder;ComPtr<IWICBitmapFrameDecode> frame;ComPtr<IWICFormatConverter> converter;ComPtr<IWICStream> stream;
    HRESULT hr=CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory));
    if(SUCCEEDED(hr)&&resource){
        auto module=GetModuleHandleW(nullptr);auto entry=FindResourceW(module,MAKEINTRESOURCEW(resource),RT_RCDATA);
        auto size=entry?SizeofResource(module,entry):0;auto data=entry?LoadResource(module,entry):nullptr;auto bytes=data?static_cast<BYTE*>(LockResource(data)):nullptr;
        if(!size||!bytes)hr=HRESULT_FROM_WIN32(ERROR_RESOURCE_DATA_NOT_FOUND);
        if(SUCCEEDED(hr))hr=factory->CreateStream(&stream);
        if(SUCCEEDED(hr))hr=stream->InitializeFromMemory(bytes,size);
        if(SUCCEEDED(hr))hr=factory->CreateDecoderFromStream(stream.Get(),nullptr,WICDecodeMetadataCacheOnLoad,&decoder);
    }else if(SUCCEEDED(hr))hr=factory->CreateDecoderFromFilename(path.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&decoder);
    if(SUCCEEDED(hr))hr=decoder->GetFrame(0,&frame);UINT width=0,height=0;if(SUCCEEDED(hr))hr=frame->GetSize(&width,&height);
    if(width>8192||height>8192||width==0||height==0)hr=E_INVALIDARG;
    if(SUCCEEDED(hr))hr=factory->CreateFormatConverter(&converter);
    if(SUCCEEDED(hr))hr=converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom);
    std::vector<unsigned char> pixels;if(SUCCEEDED(hr)){pixels.resize(size_t(width)*height*4);hr=converter->CopyPixels(nullptr,width*4,UINT(pixels.size()),pixels.data());}
    ComPtr<ID3D11Texture2D> texture;ID3D11ShaderResourceView* next=nullptr;
    if(SUCCEEDED(hr)){
        D3D11_TEXTURE2D_DESC desc{};desc.Width=width;desc.Height=height;desc.MipLevels=1;desc.ArraySize=1;desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_IMMUTABLE;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA data{};data.pSysMem=pixels.data();data.SysMemPitch=width*4;hr=device_->CreateTexture2D(&desc,&data,&texture);
        if(SUCCEEDED(hr))hr=device_->CreateShaderResourceView(texture.Get(),nullptr,&next);
    }
    if(FAILED(hr)){error=std::string("Impossible de décoder ")+(resource?"le logo de l’application":"la capture du casque")+" ("+std::to_string(unsigned(hr))+").";return false;}
    if(view_)view_->Release();view_=next;width_=int(width);height_=int(height);return true;
}
}
