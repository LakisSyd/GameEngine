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
    m_textures.resize(2, nullptr);  // ← было 1, теперь 2: слот t0 и t1 всегда готовы
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
    for (auto& t : m_textures)
    {
		t->GetTexture()->Release(); 
        t = nullptr;
    }
}

void MaterialClass::Bind(ID3D11DeviceContext* context)
{
    if (!m_shader) return;

    m_shader->BindShader(context);

    // Отладка: проверяем, есть ли текстура
    if (m_textures.size() > 0 && m_textures[0])
    {
        ID3D11ShaderResourceView* srv = m_textures[0]->GetTexture();
        if (!srv)
        {
            OutputDebugStringA("ERROR: Texture SRV is NULL!\n");
        }
        else
        {
            context->PSSetShaderResources(0, 1, &srv);
        }
    }
    else
    {
        OutputDebugStringA("WARNING: No texture assigned to slot 0!\n");
        ID3D11ShaderResourceView* nullSRV = nullptr;
        context->PSSetShaderResources(0, 1, &nullSRV);
    }
}

void MaterialClass::Unbind(ID3D11DeviceContext* context)
{
    // Опционально: очистить привязки, если нужно
    ID3D11ShaderResourceView* nullSRV = nullptr;
    context->PSSetShaderResources(0, 1, &nullSRV);
}

void MaterialClass::SetTexture(size_t index, TextureClass* texture)
{
    if (index >= m_textures.size())
        m_textures.resize(index + 1, nullptr);
    m_textures[index] = texture;
}