#pragma once
// ModelManager.h

#include <string>
#include <unordered_map>
#include <memory>
#include "assimpmodelclass.h"

class ModelManager
{
public:
    ModelManager() = default;
    ~ModelManager();

    // Загружает модель с заданной геометрией и текстурой. Если уже загружена, возвращает true.
    bool LoadModel(ID3D11Device* device, ID3D11DeviceContext* context,
        const std::string& meshFile, const std::string& textureFile);

    // Возвращает указатель на модель или nullptr, если не загружена.
    AssimpModelClass* GetModel(const std::string& meshFile, const std::string& textureFile);

    // Освобождает все модели.
    void Shutdown();

    // Запрещаем копирование
    ModelManager(const ModelManager&) = delete;
    ModelManager& operator=(const ModelManager&) = delete;

private:
    std::unordered_map<std::string, std::unique_ptr<AssimpModelClass>> m_models;
};