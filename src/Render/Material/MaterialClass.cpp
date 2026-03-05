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

    // Активируем шейдер (VS, PS, input layout, sampler и т.д.)
    m_shader->BindShader(context);

    // Привязываем текстуры в слоты t0..t3 (или меньше)
    ID3D11ShaderResourceView* nullSRV = nullptr;
    for (size_t i = 0; i < m_textures.size() && i < 4; ++i)
    {
        ID3D11ShaderResourceView* srv = nullptr;
        if (m_textures[i]) srv = m_textures[i]->GetTexture();
        // Если texture отсутствует — привязываем null (чтобы не оставить старый SRV)
        context->PSSetShaderResources(static_cast<UINT>(i), 1, srv ? &srv : &nullSRV);
    }
}


void MaterialClass::Unbind(ID3D11DeviceContext* context)
{
    // Опционально: очистить привязки, если нужно
    ID3D11ShaderResourceView* nullSRV = nullptr;
    context->PSSetShaderResources(0, 1, &nullSRV);
}