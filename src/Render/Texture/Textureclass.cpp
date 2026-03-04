
#define STB_IMAGE_IMPLEMENTATION
#include "textureclass.h"

TextureClass::TextureClass()
{
    // ❌ УДАЛЯЕМ: m_targaData = 0;
    m_texture = 0;
    m_textureView = 0;
    m_channels = 0;  // опционально
}

TextureClass::TextureClass(const TextureClass& other)
{
}


TextureClass::~TextureClass()
{
}

// Функции Initialize принимают на вход устройство Direct3D и имя файла изображения Targa. 
// Сначала она загрузит данные Targa в массив. 
// Затем она создаст текстуру и загрузит данные Targa в нее в правильном формате 
// (изображения Targa по умолчанию перевернуты и нуждаются в инверсии). 
// Затем, как только текстура загружена, она создаст представление ресурса текстуры 
// для использования шейдером для рисования.
bool TextureClass::Initialize(ID3D11Device* device, ID3D11DeviceContext* deviceContext, const char* filename)
{
    D3D11_TEXTURE2D_DESC textureDesc;
    HRESULT hResult;
    unsigned int rowPitch;
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;

    // 🔹 1. Загружаем изображение через stb_image
    unsigned char* imageData = nullptr;
    int width, height, channels;

    if (!LoadImage(filename, &imageData, &width, &height, &channels)) {
        return false;
    }

    m_width = width;
    m_height = height;
    m_channels = channels; // опционально, для отладки

    // 🔹 2. Настраиваем описание текстуры (без изменений)
    textureDesc.Height = m_height;
    textureDesc.Width = m_width;
    textureDesc.MipLevels = 0;  // 0 = авто-генерация при GenerateMips
    textureDesc.ArraySize = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;  // stbi_load с параметром 4 всегда даёт RGBA
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Usage = D3D11_USAGE_DEFAULT;
    textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    textureDesc.CPUAccessFlags = 0;
    textureDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

    // 🔹 3. Создаём пустую текстуру
    hResult = device->CreateTexture2D(&textureDesc, NULL, &m_texture);
    if (FAILED(hResult)) {
        stbi_image_free(imageData);  // 🔥 Не забываем освободить!
        return false;
    }

    // 🔹 4. Копируем данные в текстуру
    // ⚠️ stb_image загружает изображения TOP-DOWN (строка 0 = верх), 
    // а TGA был BOTTOM-UP. DirectX ожидает TOP-DOWN, так что всё совпадает! 🎉
    rowPitch = (m_width * 4) * sizeof(unsigned char);  // 4 байта на пиксель (RGBA)

    deviceContext->UpdateSubresource(m_texture, 0, NULL, imageData, rowPitch, 0);

    // 🔹 5. Создаём Shader Resource View
    srvDesc.Format = textureDesc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = -1;  // -1 = все уровни

    hResult = device->CreateShaderResourceView(m_texture, &srvDesc, &m_textureView);
    if (FAILED(hResult)) {
        stbi_image_free(imageData);
        return false;
    }

    // 🔹 6. Генерируем мипмапы
    deviceContext->GenerateMips(m_textureView);

    // 🔹 7. Освобождаем RAM-буфер (ВАЖНО: stbi_image_free, а не delete[])
    stbi_image_free(imageData);

    return true;
}

// Функция Shutdown освобождает данные текстуры, 
// и три указателя устанавливаются в null.
void TextureClass::Shutdown()
{
    // Освобождаем DirectX-ресурсы (без изменений)
    if (m_textureView) { m_textureView->Release(); m_textureView = 0; }
    if (m_texture) { m_texture->Release(); m_texture = 0; }

   

    return;
}

// GetTexture — это вспомогательная функция для обеспечения легкого доступа 
// к представлению текстуры для любых шейдеров, 
// которые требуют ее для рендеринга.
ID3D11ShaderResourceView* TextureClass::GetTexture()
{
	if (!this) {
		return nullptr;  // Безопасно возвращаем nullptr
	}
	return m_textureView;
}

// Это наша функция загрузки изображения Targa. 
// Еще раз обратите внимание, что изображения Targa хранятся перевернутыми 
// и нуждаются в перевороте перед использованием. 
// Итак, здесь мы откроем файл, прочитаем его в массив, 
// а затем возьмем данные этого массива и загрузим их в массив m_targaData 
// в правильном порядке. 
// Обратите внимание, мы намеренно работаем только с 32-битными файлами Targa, 
// имеющими альфа-каналы, эта функция отклонит Targa, сохраненные как 24-битные.


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
	// stbi_load автоматически определит формат: PNG, JPG, BMP, TGA, PSD, HDR...
	// Последний параметр 4 = требовать 4 канала (RGBA) всегда
	unsigned char* imageData = stbi_load(filename, outWidth, outHeight, outChannels, 4);

	if (!imageData) {
		// Можно вывести ошибку: stbi_failure_reason()
		return false;
	}

	*outPixels = imageData;
	return true;
}