#define STB_IMAGE_IMPLEMENTATION
#include "textureclass.h"
#include "stb_image.h"

TextureClass::TextureClass()
{
    m_texture = 0;
    m_textureView = 0;
    m_channels = 0;
}

TextureClass::TextureClass(const TextureClass& other)
{
}

TextureClass::~TextureClass()
{
}

bool TextureClass::Initialize(ID3D11Device* device, ID3D11DeviceContext* deviceContext, const char* filename, bool forceSRGB)
{
    D3D11_TEXTURE2D_DESC textureDesc{};
    HRESULT hResult;
    unsigned int rowPitch;
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};

    unsigned char* imageData = nullptr;
    int width = 0, height = 0, channels = 0;

    if (!LoadImage(filename, &imageData, &width, &height, &channels)) {
        return false;
    }

    m_width = width;
    m_height = height;
    m_channels = channels;

    textureDesc.Height = m_height;
    textureDesc.Width = m_width;
    textureDesc.MipLevels = 0;  // авто-генерация mip
    textureDesc.ArraySize = 1;
    // Если требуется SRGB — создаём SRGB формат, иначе обычный UNORM
    textureDesc.Format = forceSRGB ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Usage = D3D11_USAGE_DEFAULT;
    textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    textureDesc.CPUAccessFlags = 0;
    textureDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

    hResult = device->CreateTexture2D(&textureDesc, NULL, &m_texture);
    if (FAILED(hResult)) {
        stbi_image_free(imageData);
        return false;
    }

    rowPitch = (m_width * 4) * sizeof(unsigned char);
    deviceContext->UpdateSubresource(m_texture, 0, NULL, imageData, rowPitch, 0);

    srvDesc.Format = textureDesc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = -1;

    hResult = device->CreateShaderResourceView(m_texture, &srvDesc, &m_textureView);
    if (FAILED(hResult)) {
        stbi_image_free(imageData);
        return false;
    }

    deviceContext->GenerateMips(m_textureView);

    stbi_image_free(imageData);

    return true;
}

void TextureClass::Shutdown()
{
    if (m_textureView) { m_textureView->Release(); m_textureView = 0; }
    if (m_texture) { m_texture->Release(); m_texture = 0; }
    return;
}

ID3D11ShaderResourceView* TextureClass::GetTexture()
{
    return m_textureView;
}

int TextureClass::GetWidth()
{
    return m_width;
}

int TextureClass::GetHeight()
{
    return m_height;
}

bool TextureClass::LoadImage(const char* filename, unsigned char** outPixels, int* outWidth, int* outHeight, int* outChannels)
{
    unsigned char* imageData = stbi_load(filename, outWidth, outHeight, outChannels, 4);
    if (!imageData) {
        return false;
    }
    *outPixels = imageData;
    return true;
}