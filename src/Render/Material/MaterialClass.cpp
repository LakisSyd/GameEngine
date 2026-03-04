////////////////////////////////////////////////////////////////////////////////
// Имя файла: materialclass.cpp
////////////////////////////////////////////////////////////////////////////////

#include "materialclass.h"

MaterialClass::MaterialClass()
    : m_shader(nullptr)
    , m_diffuseColor(1.0f, 1.0f, 1.0f)
    , m_specularPower(20.0f)
{
}

MaterialClass::~MaterialClass()
{
    Shutdown();
}

bool MaterialClass::Initialize(ID3D11Device* device, const std::wstring& name)
{
    m_name = name;
    m_shader = nullptr;
    m_textures.resize(1, nullptr);  // 🔥 Теперь SetMainTexture() будет работать!
    m_diffuseColor = XMFLOAT3(1.0f, 1.0f, 1.0f);
    m_specularPower = 20.0f;
    return true;
}

void MaterialClass::Shutdown()
{
    // ⚠️ НЕ освобождаем m_shader и m_textures!
    // Они управляются ResourceManager / SceneManager
    m_shader = nullptr;
    m_textures.clear();
}

void MaterialClass::Bind(ID3D11DeviceContext* context)
{
    if (!m_shader) return;

    // 🔥 АКТИВИРУЕМ ШЕЙДЕР (главное исправление!)
    m_shader->BindShader(context);  // Устанавливает VS, PS, InputLayout, Sampler

    // Привязываем текстуры
    if (!m_textures.empty() && m_textures[0]) {
        ID3D11ShaderResourceView* srv = m_textures[0]->GetTexture();
        if (srv) context->PSSetShaderResources(0, 1, &srv);
    }
}


void MaterialClass::Unbind(ID3D11DeviceContext* context)
{
    // Опционально: очистить привязки, если нужно
    ID3D11ShaderResourceView* nullSRV = nullptr;
    context->PSSetShaderResources(0, 1, &nullSRV);
}