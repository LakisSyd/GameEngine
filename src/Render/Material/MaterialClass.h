#pragma once
#include <d3d11.h>
#include <DirectXMath.h>
#include <vector>
#include <string>
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
        if (m_textures.empty()) m_textures.resize(2, nullptr);
        m_textures[0] = texture;
    }
    TextureClass* GetMainTexture() const {
        return (!m_textures.empty() && m_textures[0]) ? m_textures[0] : nullptr;
    }

    void AddTexture(TextureClass* texture) {
        if (m_textures.empty()) m_textures.resize(2, nullptr);
        for (auto& t : m_textures) {
            if (t == nullptr) { t = texture; return; }
        }
        m_textures.push_back(texture);
    }
    TextureClass* GetTexture(size_t index) const
    {
        if (index >= m_textures.size()) return nullptr;
        return m_textures[index];
    }
    void SetTexture(size_t index, TextureClass* texture);

    void SetDiffuseColor(const XMFLOAT3& color) { m_diffuseColor = color; }
    XMFLOAT3 GetDiffuseColor() const { return m_diffuseColor; }

    void SetSpecularPower(float power) { m_specularPower = power; }
    float GetSpecularPower() const { return m_specularPower; }

    void SetName(const std::wstring& name) { m_name = name; }
    const std::wstring& GetName() const { return m_name; }

    // ── Clip Plane ───────────────────────────────────────────────────────────
    void       SetClipPlane(const XMFLOAT4& plane) { m_clipPlane = plane; m_clipEnabled = true; }
    XMFLOAT4   GetClipPlane() const { return m_clipPlane; }
    void       SetClipEnabled(bool enabled) { m_clipEnabled = enabled; }
    bool       IsClipEnabled() const { return m_clipEnabled; }

    // ── Reflection ───────────────────────────────────────────────────────────
    void     SetReflectionMatrix(const XMMATRIX& m) { m_reflectionMatrix = m; }
    XMMATRIX GetReflectionMatrix() const { return m_reflectionMatrix; }

    // Добавь рядом с SetReflectionMatrix:
    void     SetProjectorProjection(const XMMATRIX& m) { m_projectorProjection = m; }
    XMMATRIX GetProjectorProjection() const { return m_projectorProjection; }



    void                      SetReflectionSRV(ID3D11ShaderResourceView* srv) { m_reflectionSRV = srv; }
    ID3D11ShaderResourceView* GetReflectionSRV() const { return m_reflectionSRV; }

    void  SetReflectivity(float r) { m_reflectivity = r; }
    float GetReflectivity() const { return m_reflectivity; }

    // ── Water ────────────────────────────────────────────────────────────────
    // waterTranslation    — UV-смещение нормал-карты, обновляется каждый кадр:
    //                       m_waterTranslation += deltaTime * speed; (wrap в [0,1])
    // reflectRefractScale — сила искажения отражения/преломления рябью (0.01–0.03)
    // refractionTint      — синеватый оттенок воды: XMFLOAT4(0, 0.3f, 0.5f, 1)
    // refractionSRV       — RenderTexture с подводной сценой (не владеет!)
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

private:

    XMMATRIX m_projectorProjection = XMMatrixIdentity(); // новое поле
    std::wstring     m_name;
    ShaderBaseClass* m_shader = nullptr;
    std::vector<TextureClass*> m_textures;

    XMFLOAT3 m_diffuseColor = { 1, 1, 1 };
    float    m_specularPower = 16.0f;

    // ── Clip Plane ───────────────────────────────────────────────────────────
    XMFLOAT4 m_clipPlane = { 0.0f, 1.0f, 0.0f, 0.0f };
    bool     m_clipEnabled = false;

    // ── Reflection ───────────────────────────────────────────────────────────
    XMMATRIX                  m_reflectionMatrix = XMMatrixIdentity();
    ID3D11ShaderResourceView* m_reflectionSRV = nullptr; // не владеет!
    float                     m_reflectivity = 0.5f;

    // ── Water ────────────────────────────────────────────────────────────────
    float                     m_waterTranslation = 0.0f;
    float                     m_reflectRefractScale = 0.02f;
    XMFLOAT4                  m_refractionTint = { 0.0f, 0.3f, 0.5f, 1.0f };
    ID3D11ShaderResourceView* m_refractionSRV = nullptr; // не владеет!

    XMMATRIX m_view2 = XMMatrixIdentity();
    XMMATRIX m_proj2 = XMMatrixIdentity();
};