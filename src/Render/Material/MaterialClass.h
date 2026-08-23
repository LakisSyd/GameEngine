#pragma once
#include <d3d11.h>
#include <DirectXMath.h>
#include <vector>
#include <string>
#include <memory>
#include "BaseShader.h"
#include "TextureClass.h"

using namespace DirectX;

class MaterialClass
{
public:
    MaterialClass();
    ~MaterialClass();

    bool Initialize(ID3D11Device* device, const std::wstring& name);
    void Shutdown();

    void SetShader(ShaderBaseClass* shader) { m_shader = shader; }
    ShaderBaseClass* GetShader() const { return m_shader; }

    void SetMainTexture(TextureClass* texture) {
        SetTexture(0, texture);
    }
    TextureClass* GetMainTexture() const {
        return (!m_textures.empty() && m_textures[0]) ? m_textures[0] : nullptr;
    }

    void AddTexture(TextureClass* texture) {
        if (m_textures.empty()) m_textures.resize(2, nullptr);
        for (size_t index = 0; index < m_textures.size(); ++index) {
            if (m_textures[index] == nullptr) {
                SetTexture(index, texture);
                return;
            }
        }
        SetTexture(m_textures.size(), texture);
    }
    TextureClass* GetTexture(size_t index) const
    {
        if (index >= m_textures.size()) return nullptr;
        return m_textures[index];
    }
    void SetTexture(size_t index, TextureClass* texture);
    void AdoptTexture(size_t index, std::unique_ptr<TextureClass> texture);
    void SetTexturePath(size_t index, const std::string& path);
    std::string GetTexturePath(size_t index) const;

    void SetDiffuseColor(const XMFLOAT3& color) { m_diffuseColor = color; }
    XMFLOAT3 GetDiffuseColor() const { return m_diffuseColor; }

    void SetPBREnvironmentStrength(float strength) { m_pbrEnvironmentStrength = strength; }
    float GetPBREnvironmentStrength() const { return m_pbrEnvironmentStrength; }

    // пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ: пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅ пїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅ.
    void SetEmissiveColor(const XMFLOAT3& color) { m_emissiveColor = color; }
    XMFLOAT3 GetEmissiveColor() const { return m_emissiveColor; }
    void SetEmissiveStrength(float strength) { m_emissiveStrength = strength; }
    float GetEmissiveStrength() const { return m_emissiveStrength; }

    void SetSpecularPower(float power) { m_specularPower = power; }
    float GetSpecularPower() const { return m_specularPower; }

    void SetName(const std::wstring& name) { m_name = name; }
    const std::wstring& GetName() const { return m_name; }

    // -- Clip Plane -----------------------------------------------------------
    void       SetClipPlane(const XMFLOAT4& plane) { m_clipPlane = plane; m_clipEnabled = true; }
    XMFLOAT4   GetClipPlane() const { return m_clipPlane; }
    void       SetClipEnabled(bool enabled) { m_clipEnabled = enabled; }
    bool       IsClipEnabled() const { return m_clipEnabled; }

    // -- Reflection -----------------------------------------------------------
    void     SetReflectionMatrix(const XMMATRIX& m) { m_reflectionMatrix = m; }
    XMMATRIX GetReflectionMatrix() const { return m_reflectionMatrix; }

    // пїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅ пїЅ SetReflectionMatrix:
    void     SetProjectorProjection(const XMMATRIX& m) { m_projectorProjection = m; }
    XMMATRIX GetProjectorProjection() const { return m_projectorProjection; }



    void                      SetReflectionSRV(ID3D11ShaderResourceView* srv) { m_reflectionSRV = srv; }
    ID3D11ShaderResourceView* GetReflectionSRV() const { return m_reflectionSRV; }

    void  SetReflectivity(float r) { m_reflectivity = r; }
    float GetReflectivity() const { return m_reflectivity; }

    // -- Water ----------------------------------------------------------------
    // waterTranslation    пїЅ UV-пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅ-пїЅпїЅпїЅпїЅпїЅ, пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅ:
    //                       m_waterTranslation += deltaTime * speed; (wrap пїЅ [0,1])
    // reflectRefractScale пїЅ пїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ/пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅ (0.01пїЅ0.03)
    // refractionTint      пїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅ: XMFLOAT4(0, 0.3f, 0.5f, 1)
    // refractionSRV       пїЅ RenderTexture пїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅ (пїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅ!)
    void  SetWaterTranslation(float t) { m_waterTranslation = t; }
    float GetWaterTranslation()     const { return m_waterTranslation; }

    void  SetReflectRefractScale(float s) { m_reflectRefractScale = s; }
    float GetReflectRefractScale()  const { return m_reflectRefractScale; }

    void     SetRefractionTint(const XMFLOAT4& tint) { m_refractionTint = tint; }
    XMFLOAT4 GetRefractionTint()               const { return m_refractionTint; }

    void                      SetRefractionSRV(ID3D11ShaderResourceView* srv) { m_refractionSRV = srv; }
    ID3D11ShaderResourceView* GetRefractionSRV()                        const { return m_refractionSRV; }

    void Bind(ID3D11DeviceContext* context);
    void Unbind(ID3D11DeviceContext* context);

    void SetProjectionView2(const XMMATRIX& view) { m_view2 = view; }
    XMMATRIX GetProjectionView2() const { return m_view2; }
    void SetProjectionProj2(const XMMATRIX& proj) { m_proj2 = proj; }
    XMMATRIX GetProjectionProj2() const { return m_proj2; }

    // -- Glass / Transparency -------------------------------------------------
    // isGlass = true > PBR пїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅ (пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ, пїЅпїЅпїЅпїЅпїЅпїЅ Fresnel)
    // glassOpacity = 0.0 пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ, 1.0 = пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ
    void  SetGlass(bool glass, float opacity = 0.15f) { m_isGlass = glass; m_glassOpacity = opacity; }
    bool  IsGlass()        const { return m_isGlass; }
    float GetGlassOpacity() const { return m_glassOpacity; }

    void SetTerrainTileScale(float scale) { m_terrainTileScale = scale; }
    float GetTerrainTileScale() const { return m_terrainTileScale; }
    void SetTerrainNormalStrength(float strength) { m_terrainNormalStrength = strength; }
    float GetTerrainNormalStrength() const { return m_terrainNormalStrength; }
    void SetTerrainVariationScale(float scale) { m_terrainVariationScale = scale; }
    float GetTerrainVariationScale() const { return m_terrainVariationScale; }
    void SetTerrainVariationStrength(float strength) { m_terrainVariationStrength = strength; }
    float GetTerrainVariationStrength() const { return m_terrainVariationStrength; }
    void SetTerrainBrightness(float brightness) { m_terrainBrightness = brightness; }
    float GetTerrainBrightness() const { return m_terrainBrightness; }
    void SetTerrainAmbientBoost(float boost) { m_terrainAmbientBoost = boost; }
    float GetTerrainAmbientBoost() const { return m_terrainAmbientBoost; }

    void SetTerrainBounds(const XMFLOAT2& minXZ, const XMFLOAT2& sizeXZ)
    {
        m_terrainMinXZ = minXZ;
        m_terrainSizeXZ = sizeXZ;
    }
    XMFLOAT2 GetTerrainMinXZ() const { return m_terrainMinXZ; }
    XMFLOAT2 GetTerrainSizeXZ() const { return m_terrainSizeXZ; }

private:

    XMMATRIX m_projectorProjection = XMMatrixIdentity(); // пїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅ
    std::wstring     m_name;
    ShaderBaseClass* m_shader = nullptr;
    std::vector<TextureClass*> m_textures;
    std::vector<std::unique_ptr<TextureClass>> m_ownedTextures;
    std::vector<std::string> m_texturePaths;

    XMFLOAT3 m_diffuseColor = { 1, 1, 1 };
    float    m_specularPower = 16.0f;
    float    m_pbrEnvironmentStrength = 1.0f;
    XMFLOAT3 m_emissiveColor = { 0.0f, 0.0f, 0.0f };
    float    m_emissiveStrength = 0.0f;

    // -- Clip Plane -----------------------------------------------------------
    XMFLOAT4 m_clipPlane = { 0.0f, 1.0f, 0.0f, 0.0f };
    bool     m_clipEnabled = false;

    // -- Reflection -----------------------------------------------------------
    XMMATRIX                  m_reflectionMatrix = XMMatrixIdentity();
    ID3D11ShaderResourceView* m_reflectionSRV = nullptr; // пїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅ!
    float                     m_reflectivity = 0.5f;

    // -- Water ----------------------------------------------------------------
    float                     m_waterTranslation = 0.0f;
    float                     m_reflectRefractScale = 0.02f;
    XMFLOAT4                  m_refractionTint = { 0.0f, 0.3f, 0.5f, 1.0f };
    ID3D11ShaderResourceView* m_refractionSRV = nullptr; // пїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅ!

    XMMATRIX m_view2 = XMMatrixIdentity();
    XMMATRIX m_proj2 = XMMatrixIdentity();

    // -- Glass ----------------------------------------------------------------
    bool  m_isGlass      = false;
    float m_glassOpacity = 0.15f;  // пїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅ (0=пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ, 1=пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ)

    float m_terrainTileScale = 2.0f;
    float m_terrainNormalStrength = 1.0f;
    float m_terrainVariationScale = 0.08f;
    float m_terrainVariationStrength = 1.0f;
    float m_terrainBrightness = 1.0f;
    float m_terrainAmbientBoost = 1.0f;
    XMFLOAT2 m_terrainMinXZ = { -50.0f, -50.0f };
    XMFLOAT2 m_terrainSizeXZ = { 100.0f, 100.0f };
};
