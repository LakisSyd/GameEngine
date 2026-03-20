#pragma once
#include <unordered_map>
#include <string>
#include <memory>
#include "BaseShader.h"

////////////////////////////////////////////////////////////////////////////////
// ShaderManager — создаёт, хранит и предоставляет доступ к шейдерам
//
// Использование:
//   ShaderManager::Get().Load<LightShaderClass>("light", device, hwnd);
//   auto* s = ShaderManager::Get().GetAs<LightShaderClass>("light");
//   ShaderManager::Get().Shutdown(); // в ApplicationClass::Shutdown()
////////////////////////////////////////////////////////////////////////////////
class ShaderManager
{
public:
    static ShaderManager& Get()
    {
        static ShaderManager instance;
        return instance;
    }

    // Создаёт шейдер, инициализирует и регистрирует по имени
    // Возвращает указатель или nullptr при ошибке
    template<typename TShader>
    TShader* Load(const std::string& name, ID3D11Device* device, HWND hwnd)
    {
        // Если уже загружен — возвращаем существующий
        if (m_shaders.count(name))
            return dynamic_cast<TShader*>(m_shaders[name].get());

        auto shader = std::make_unique<TShader>();
        if (!shader->Initialize(device, hwnd))
        {
            std::wstring msg = L"ShaderManager: не удалось загрузить шейдер: ";
            msg += std::wstring(name.begin(), name.end());
            MessageBoxW(hwnd, msg.c_str(), L"Shader Error", MB_OK);
            return nullptr;
        }

        TShader* ptr = shader.get();
        m_shaders[name] = std::unique_ptr<ShaderBaseClass>(shader.release()); // ShaderManager владеет шейдером
        return ptr;
    }

    // Возвращает список имён всех загруженных шейдеров
    std::vector<std::string> GetNames() const
    {
        std::vector<std::string> names;
        names.reserve(m_shaders.size());
        for (const auto& [name, shader] : m_shaders)
            names.push_back(name);
        return names;
    }

    // Возвращает шейдер с приведением к нужному типу
    template<typename TShader>
    TShader* GetAs(const std::string& name) const
    {
        return dynamic_cast<TShader*>(Get(name));
    }

    // Возвращает базовый указатель
    ShaderBaseClass* Get(const std::string& name) const
    {
        auto it = m_shaders.find(name);
        return (it != m_shaders.end()) ? it->second.get() : nullptr;
    }

    bool Has(const std::string& name) const
    {
        return m_shaders.count(name) > 0;
    }

    // ? Вызывает fn для каждого загруженного шейдера
    // Используется для обновления глобальных буферов (например, FogBuffer)
    template<typename Fn>
    void ForEach(Fn&& fn)
    {
        for (auto& [name, shader] : m_shaders)
            fn(shader.get());
    }

    // Удаляет все шейдеры (вызвать в ApplicationClass::Shutdown())
    void Shutdown()
    {
        for (auto& [name, shader] : m_shaders)
            shader->Shutdown();
        m_shaders.clear();
    }

private:
    ShaderManager() = default;
    ShaderManager(const ShaderManager&) = delete;
    ShaderManager& operator=(const ShaderManager&) = delete;

    // unique_ptr — ShaderManager теперь ВЛАДЕЕТ шейдерами
    std::unordered_map<std::string, std::unique_ptr<ShaderBaseClass>> m_shaders;
};