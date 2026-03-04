#pragma once
//////////////
// INCLUDES //
//////////////
// ПОДКЛЮЧЕНИЯ

#include <d3d11.h>
#include <stdio.h>


#include "stb_image.h"

////////////////////////////////////////////////////////////////////////////////
// Class name: TextureClass
////////////////////////////////////////////////////////////////////////////////
// Имя класса: TextureClass
class TextureClass
{
private:
    // Мы определяем структуру заголовка файла Targa здесь, 
    // чтобы облегчить чтение данных.
   
public:
    TextureClass();
    TextureClass(const TextureClass&);
    ~TextureClass();

    bool Initialize(ID3D11Device* device, ID3D11DeviceContext* deviceContext, const char* filename);
    void Shutdown();

    
    ID3D11ShaderResourceView* GetTexture();
    int GetWidth();
    int GetHeight();

private:
    // Здесь у нас есть наша функция чтения Targa. 
    // Если бы вы хотели поддержать больше форматов, вы бы добавили функции чтения здесь.
    bool LoadImage(const char* filename, unsigned char** outPixels, int* outWidth, int* outHeight, int* outChannels);

private:
    // У этого класса есть пять переменных-членов. 
    // Первая хранит сырые данные Targa, прочитанные прямо из файла. 
    // Вторая переменная called m_texture будет хранить структурированные данные текстуры, 
    // которые DirectX будет использовать для рендеринга. 
    // И третья переменная — это представление ресурса (resource view), 
    // которое шейдер использует для доступа к данным текстуры при рисовании. 
    // Width и Height — это размеры текстуры.
    unsigned char* m_targaData;
    ID3D11Texture2D* m_texture;
    ID3D11ShaderResourceView* m_textureView;
    int m_width, m_height;
    int m_channels;  // Количество каналов из stb_image (для отладки, опционально)

};
