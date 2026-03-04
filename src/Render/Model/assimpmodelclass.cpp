#include "assimpmodelclass.h"
#include <iostream>
#include <Windows.h> // Для ZeroMemory

AssimpModelClass::AssimpModelClass() : m_scene(nullptr), m_Texture(nullptr), m_totalIndexCount(0)
{
}

AssimpModelClass::~AssimpModelClass()
{
    Shutdown();
}

bool AssimpModelClass::Initialize(ID3D11Device* device, ID3D11DeviceContext* deviceContext,
    const char* modelPath, const char* textureFilename)
{
    // 1. Загрузка геометрии через Assimp
    // Проверяем путь к файлу! Он должен быть относительно .exe или абсолютным.
    m_scene = m_importer.ReadFile(modelPath,
        aiProcess_Triangulate |
        aiProcess_FlipUVs |
        aiProcess_GenNormals |
        aiProcess_CalcTangentSpace);

    if (!m_scene || m_scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !m_scene->mRootNode)
    {
        std::string err = m_importer.GetErrorString();
        MessageBoxA(NULL, err.c_str(), "Assimp Load Error", MB_OK);
        return false;
    }

    OutputDebugStringA(("Assimp: Model loaded successfully. Meshes count: " + std::to_string(m_scene->mNumMeshes) + "\n").c_str());

    m_totalIndexCount = 0;

    // 2. Проход по всем мешам
    for (unsigned int i = 0; i < m_scene->mNumMeshes; i++)
    {
        aiMesh* mesh = m_scene->mMeshes[i];

        // ✅ ПЕРЕМЕСТИЛИ ПРОВЕРКУ И ЛОГИРОВАНИЕ ВНЕ ЦИКЛА ПО ВЕРШИНАМ
        OutputDebugStringA(("--- Processing Mesh #" + std::to_string(i) + " ---\n").c_str());
        OutputDebugStringA(("Vertices: " + std::to_string(mesh->mNumVertices) + "\n").c_str());
        OutputDebugStringA(("Faces: " + std::to_string(mesh->mNumFaces) + "\n").c_str());

        if (mesh->mNumVertices == 0)
        {
            OutputDebugStringA("WARNING: Skipping mesh with 0 vertices.\n");
            continue;
        }

        MeshPart part;
        part.indexCount = mesh->mNumFaces * 3;
        m_totalIndexCount += part.indexCount;

        vector<VertexType> vertices;
        vertices.reserve(mesh->mNumVertices);

        // Теперь цикл только заполняет данные
        for (unsigned int j = 0; j < mesh->mNumVertices; j++)
        {
            VertexType vertex;
            vertex.position = XMFLOAT3(mesh->mVertices[j].x, mesh->mVertices[j].y, mesh->mVertices[j].z);

            if (mesh->HasTextureCoords(0))
                vertex.texture = XMFLOAT2(mesh->mTextureCoords[0][j].x, mesh->mTextureCoords[0][j].y);
            else
                vertex.texture = XMFLOAT2(0.0f, 0.0f);

            if (mesh->HasNormals())
                vertex.normal = XMFLOAT3(mesh->mNormals[j].x, mesh->mNormals[j].y, mesh->mNormals[j].z);
            else
                vertex.normal = XMFLOAT3(0.0f, 1.0f, 0.0f);

            vertices.push_back(vertex);
        }

        // Подготовка индексов
        vector<unsigned long> indices;
        indices.reserve(part.indexCount);
        for (unsigned int j = 0; j < mesh->mNumFaces; j++)
        {
            aiFace face = mesh->mFaces[j];
            for (unsigned int k = 0; k < face.mNumIndices; k++)
                indices.push_back(face.mIndices[k]);
        }

        // ✅ СОЗДАНИЕ БУФЕРОВ С ZeroMemory
        D3D11_BUFFER_DESC vbDesc, ibDesc;
        D3D11_SUBRESOURCE_DATA vbData, ibData;
        HRESULT result;

        // Vertex Buffer
        ZeroMemory(&vbDesc, sizeof(vbDesc));
        vbDesc.Usage = D3D11_USAGE_DEFAULT;
        vbDesc.ByteWidth = sizeof(VertexType) * (UINT)vertices.size();
        vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        ZeroMemory(&vbData, sizeof(vbData));
        vbData.pSysMem = vertices.data();

        result = device->CreateBuffer(&vbDesc, &vbData, &part.vertexBuffer);
        if (FAILED(result))
        {
            OutputDebugStringA("ERROR: Failed to create Vertex Buffer!\n");
            OutputDebugStringA(("ByteWidth attempted: " + std::to_string(vbDesc.ByteWidth) + "\n").c_str());
            return false;
        }

        // Index Buffer
        ZeroMemory(&ibDesc, sizeof(ibDesc));
        ibDesc.Usage = D3D11_USAGE_DEFAULT;
        ibDesc.ByteWidth = sizeof(unsigned long) * (UINT)indices.size();
        ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

        ZeroMemory(&ibData, sizeof(ibData));
        ibData.pSysMem = indices.data();

        result = device->CreateBuffer(&ibDesc, &ibData, &part.indexBuffer);
        if (FAILED(result))
        {
            OutputDebugStringA("ERROR: Failed to create Index Buffer!\n");
            OutputDebugStringA(("ByteWidth attempted: " + std::to_string(ibDesc.ByteWidth) + "\n").c_str());
            return false;
        }

        m_meshParts.push_back(part);
        OutputDebugStringA(("Mesh #" + std::to_string(i) + " created successfully.\n").c_str());
    }

    if (m_meshParts.empty())
    {
        MessageBoxA(NULL, "No valid meshes found in the file!", "Assimp Error", MB_OK);
        return false;
    }

    // 3. Загрузка текстуры
    m_Texture = new TextureClass;
    if (!m_Texture) return false;

    bool result = m_Texture->Initialize(device, deviceContext, textureFilename);
    if (!result)
    {
        MessageBoxA(NULL, "Не удалось загрузить текстуру для Assimp модели!", "Ошибка", MB_OK);
        delete m_Texture;
        m_Texture = nullptr;
        return false;
    }

    OutputDebugStringA("AssimpModelClass initialized successfully.\n");
    return true;
}

void AssimpModelClass::Shutdown()
{
    for (auto& part : m_meshParts)
    {
        if (part.vertexBuffer) { part.vertexBuffer->Release(); part.vertexBuffer = nullptr; }
        if (part.indexBuffer) { part.indexBuffer->Release(); part.indexBuffer = nullptr; }
    }
    m_meshParts.clear();
    m_totalIndexCount = 0;

    if (m_Texture)
    {
        m_Texture->Shutdown();
        delete m_Texture;
        m_Texture = nullptr;
    }
}

void AssimpModelClass::Render(ID3D11DeviceContext* deviceContext)
{
    if (m_meshParts.empty()) return;

    unsigned int stride = sizeof(VertexType);
    unsigned int offset = 0;

    // Просто устанавливаем буферы первого меша (для совместимости)
    // Если нужно рисовать все меши, тут должен быть цикл, но DrawIndexed делать НЕЛЬЗЯ
    deviceContext->IASetVertexBuffers(0, 1, &m_meshParts[0].vertexBuffer, &stride, &offset);
    deviceContext->IASetIndexBuffer(m_meshParts[0].indexBuffer, DXGI_FORMAT_R32_UINT, 0);
    deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // ВАЖНО: Здесь НЕТ вызова shader->Render() и НЕТ DrawIndexed!
}


int AssimpModelClass::GetIndexCount()
{
    // Возвращаем количество индексов ПЕРВОГО меша для совместимости со старым кодом отрисовки
    if (!m_meshParts.empty())
        return m_meshParts[0].indexCount;

    return m_totalIndexCount;
}

ID3D11ShaderResourceView* AssimpModelClass::GetTexture()
{
    if (m_Texture) return m_Texture->GetTexture();
    return nullptr;
}