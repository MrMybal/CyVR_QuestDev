#pragma once
#include <string>
#include <cstdint>
struct ID3D11Device;
struct ID3D11ShaderResourceView;
namespace quest {
class Preview {
public:
    explicit Preview(void* device):device_(static_cast<ID3D11Device*>(device)){}
    ~Preview();
    bool load(const std::wstring& path,std::string& error);
    uint64_t texture() const {return reinterpret_cast<uint64_t>(view_);}
    int width() const {return width_;}int height() const{return height_;}
private:
    ID3D11Device* device_{};ID3D11ShaderResourceView* view_{};int width_{},height_{};
};
}
