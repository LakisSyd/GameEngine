// ModelManager.cpp
#include "ModelManager.h"

ModelManager::~ModelManager()
{
    Shutdown();
}

bool ModelManager::LoadModel(ID3D11Device* device, ID3D11DeviceContext* context,
    const std::string& meshFile, const std::string& textureFile)
{
    std::string key = meshFile + "|" + textureFile; // уникальный ключ
    auto it = m_models.find(key);
    if (it != m_models.end())
        return true; // уже есть

    auto model = std::make_unique<AssimpModelClass>();
    if (!model->Initialize(device, context, meshFile.c_str(), textureFile.c_str()))
        return false;

    m_models[key] = std::move(model);
    return true;
}

AssimpModelClass* ModelManager::GetModel(const std::string& meshFile, const std::string& textureFile)
{
    std::string key = meshFile + "|" + textureFile;
    auto it = m_models.find(key);
    return (it != m_models.end()) ? it->second.get() : nullptr;
}

void ModelManager::Shutdown()
{
    for (auto& pair : m_models)
    {
        pair.second->Shutdown();
    }
    m_models.clear();
}