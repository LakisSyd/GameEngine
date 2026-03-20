#pragma once
#include <d3d11.h>
#include <string>

class TextureClass
{
public:
    TextureClass();
    TextureClass(const TextureClass& other);
    ~TextureClass();

    // Добавлен optional параметр forceSRGB (по умолчанию false)
    bool Initialize(ID3D11Device* device, ID3D11DeviceContext* deviceContext, const char* filename, bool forceSRGB = false);
    void Shutdown();

    ID3D11ShaderResourceView* GetTexture();

    int GetWidth();
    int GetHeight();

    // Вспомогательная функция для загрузки через stb_image
    bool LoadImage(const char* filename, unsigned char** outPixels, int* outWidth, int* outHeight, int* outChannels);

private:
    ID3D11Texture2D* m_texture = nullptr;
    ID3D11ShaderResourceView* m_textureView = nullptr;
    int m_width = 0;
    int m_height = 0;
    int m_channels = 0;
};