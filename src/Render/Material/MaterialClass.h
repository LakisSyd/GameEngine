#pragma once
////////////////////////////////////////////////////////////////////////////////
// Имя файла: materialclass.h
// 📌 КЛАСС: MaterialClass
// 💡 Назначение: Инкапсуляция визуальных свойств объекта (шейдер + текстуры + параметры)
////////////////////////////////////////////////////////////////////////////////

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

    // === Инициализация и очистка ===
    bool Initialize(ID3D11Device* device, const std::wstring& name);
    void Shutdown();

    // === Геттеры и сеттеры ===
    void SetShader(ShaderBaseClass* shader) { m_shader = shader; }
    ShaderBaseClass* GetShader() const { return m_shader; }

    void SetMainTexture(TextureClass* texture) {
        if (!m_textures.empty()) m_textures[0] = texture;
    }
    TextureClass* GetMainTexture() const {
        return (!m_textures.empty() && m_textures[0]) ? m_textures[0] : nullptr;
    }

    void AddTexture(TextureClass* texture) { m_textures.push_back(texture); }
    TextureClass* GetTexture(size_t index) const {
        return (index < m_textures.size()) ? m_textures[index] : nullptr;
    }

    // Параметры материала
    void SetDiffuseColor(const XMFLOAT3& color) { m_diffuseColor = color; }
    XMFLOAT3 GetDiffuseColor() const { return m_diffuseColor; }

    void SetSpecularPower(float power) { m_specularPower = power; }
    float GetSpecularPower() const { return m_specularPower; }

    void SetName(const std::wstring& name) { m_name = name; }
    const std::wstring& GetName() const { return m_name; }

    // === Рендеринг (вызывается из GameObject) ===
    // Подготовка шейдера и текстур перед отрисовкой
    void Bind(ID3D11DeviceContext* context);

    // Очистка привязок после отрисовки (опционально)
    void Unbind(ID3D11DeviceContext* context);

private:
    std::wstring m_name;              // Имя материала (для редактора)
    ShaderBaseClass* m_shader;        // Шейдер (может быть LightShader, TextureShader и т.д.)
    std::vector<TextureClass*> m_textures; // Текстуры: [0] = основная, [1] = нормаль-мапа и т.д.

    // Параметры, которые могут использоваться шейдерами
    XMFLOAT3 m_diffuseColor;          // Базовый цвет {1.0f, 1.0f, 1.0f}
    float m_specularPower;            // Сила блика {20.0f}
};