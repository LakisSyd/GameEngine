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
    m_textures.assign(2, nullptr);  // t0 and t1 are always available.
    m_ownedTextures.clear();
    m_ownedTextures.resize(2);
    m_texturePaths.assign(2, {});
    m_diffuseColor = XMFLOAT3(1.0f, 1.0f, 1.0f);
    m_specularPower = 20.0f;
    m_pbrEnvironmentStrength = 1.0f;
    return true;
}
void MaterialClass::Shutdown()
{
    // Material only references shaders/textures. Ownership stays with ShaderManager,
    // TextureClass owners, AssimpModelClass, or the application-level defaults.
    m_shader = nullptr;
    m_ownedTextures.clear();
    m_textures.clear();
    m_texturePaths.clear();
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
    if (index >= m_ownedTextures.size())
        m_ownedTextures.resize(index + 1);
    if (index >= m_texturePaths.size())
        m_texturePaths.resize(index + 1);

    m_ownedTextures[index].reset();
    m_textures[index] = texture;
}

void MaterialClass::AdoptTexture(size_t index, std::unique_ptr<TextureClass> texture)
{
    if (index >= m_textures.size())
        m_textures.resize(index + 1, nullptr);
    if (index >= m_ownedTextures.size())
        m_ownedTextures.resize(index + 1);
    if (index >= m_texturePaths.size())
        m_texturePaths.resize(index + 1);

    m_ownedTextures[index] = std::move(texture);
    m_textures[index] = m_ownedTextures[index].get();
}

void MaterialClass::SetTexturePath(size_t index, const std::string& path)
{
    if (index >= m_texturePaths.size())
        m_texturePaths.resize(index + 1);
    m_texturePaths[index] = path;
}

std::string MaterialClass::GetTexturePath(size_t index) const
{
    if (index >= m_texturePaths.size())
        return "";
    return m_texturePaths[index];
}
