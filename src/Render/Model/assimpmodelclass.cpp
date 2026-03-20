#include "assimpmodelclass.h"
#include <iostream>
#include <Windows.h> // Для ZeroMemory, GetTempPathA/GetTempFileNameA
#include <fstream>

AssimpModelClass::AssimpModelClass() : m_scene(nullptr), m_Texture(nullptr), m_totalIndexCount(0)
{
}

AssimpModelClass::~AssimpModelClass()
{
    Shutdown();
}



static bool WriteTempFile(const std::string& filename, const char* data, size_t size)
{
    std::ofstream of(filename, std::ios::binary);
    if (!of.is_open()) return false;
    of.write(data, static_cast<std::streamsize>(size));
    return of.good();
}

ID3D11ShaderResourceView* AssimpModelClass::LoadEmbeddedTexture(ID3D11Device* device,
    const aiTexture* tex)
{
    if (!tex) return nullptr;

    // Если текстура указана как незапакованная (mHeight != 0) —
    // Assimp хранит RGBA8888 в mData. Если mHeight == 0 — это сжатый (png/jpg) поток.
    const bool compressed = (tex->mHeight == 0);

    // Создадим временный файл и используем существующий TextureClass::Initialize (stb_image) для простоты.
    char tmpPath[MAX_PATH] = { 0 };
    if (GetTempPathA(MAX_PATH, tmpPath) == 0) return nullptr;

    char tmpFile[MAX_PATH] = { 0 };
    if (GetTempFileNameA(tmpPath, "glb", 0, tmpFile) == 0) return nullptr;

    std::string filename(tmpFile);

    bool writeOk = false;
    if (compressed)
    {
        // mWidth содержит размер в байтах
        writeOk = WriteTempFile(filename, reinterpret_cast<const char*>(tex->pcData), tex->mWidth);
    }
    else
    {
        // Не-сжатая RGBA, нужно записать как PNG — но проще сформировать BMP/PNG требует кодек.
        // Попробуем записать как raw RGBA и затем загрузить вручную — но stbi не умеет raw без заголовка.
        // Поэтому в этом упрощении не пытаемся поддерживать raw неподдерживаемые форматы:
        // возвращаем nullptr — но чаще в GLB встроены сжатые (png/jpg).
        writeOk = false;
    }

    if (!writeOk)
    {
        // Удаляем созданный tmp файл если он пустой
        DeleteFileA(filename.c_str());
        return nullptr;
    }

    TextureClass* t = new TextureClass();
    if (!t) { DeleteFileA(filename.c_str()); return nullptr; }

    // Инициализируем TextureClass из временного файла
    unsigned char* imageData = nullptr;
    int w, h, channels;
    // Используем внутренний метод через Initialize (который вызывает stbi_load по имени файла).
    // TextureClass::Initialize ожидает device и deviceContext; нам нужен deviceContext рядом — но у нас его нет в аргументах.
    // Поэтому мы будем делать minimal: используем TextureClass::Initialize в вызывающем коде (у нас deviceContext доступен там).
    // Для этого создаём временный TextureClass и инициализируем его ниже — но функция имеет только device. Мы will handle in caller.
    // Для упрощения — здесь создаём TextureClass и инициализируем с nullptr deviceContext (она использует deviceContext для UpdateSubresource/GenerateMips).
    // Поэтому мы должны вызывать эту функцию только из кода который передаёт корректный deviceContext. (мы делаем это так)
    // NOTE: caller must pass valid deviceContext.
    // Здесь просто возвращаем TextureClass pointer через m_ownedTextures in caller after Initialize succeed.
    // But since Initialize requires deviceContext, we will not initialize here — instead move initialization logic into ExtractTexture.
    // To keep function coherent, we'll return nullptr here and implement embedded load in ExtractTexture.
    DeleteFileA(filename.c_str());
    delete t;
    return nullptr;
}

ID3D11ShaderResourceView* AssimpModelClass::ExtractTexture(ID3D11Device* device,
    const aiMaterial* mat,
    aiTextureType type1,
    aiTextureType type2)
{
    if (!mat) return nullptr;

    aiString path;
    if (mat->GetTextureCount(type1) > 0)
    {
        if (mat->GetTexture(type1, 0, &path) == AI_SUCCESS)
        {
            std::string texPath = path.C_Str();
            if (!texPath.empty())
            {
                // Встроенный индекс вида "*0"
                if (texPath.size() > 0 && texPath[0] == '*')
                {
                    int index = atoi(texPath.c_str() + 1);
                    if (m_scene && index >= 0 && index < static_cast<int>(m_scene->mNumTextures))
                    {
                        const aiTexture* tex = m_scene->mTextures[index];
                        // Если mHeight == 0 — это сжатый PNG/JPG -> используем временный файл + TextureClass
                        if (tex->mHeight == 0)
                        {
                            // создаём временный файл
                            char tmpPath[MAX_PATH] = { 0 };
                            if (GetTempPathA(MAX_PATH, tmpPath) == 0) return nullptr;
                            char tmpFile[MAX_PATH] = { 0 };
                            if (GetTempFileNameA(tmpPath, "glb", 0, tmpFile) == 0) return nullptr;
                            std::string filename(tmpFile);

                            bool writeOk = WriteTempFile(filename, reinterpret_cast<const char*>(tex->pcData), tex->mWidth);
                            if (!writeOk) { DeleteFileA(filename.c_str()); return nullptr; }

                            // Инициализируем TextureClass напрямую (нам нужен deviceContext — получим его у caller через device pointer? нет).
                            // В нашей архитектуре Initialize принимает (device, deviceContext, filename).
                            // Здесь нет deviceContext — но ExtractTexture вызывается из Initialize, где deviceContext есть.
                            // Изменим сигнатуру — проще: в Initialize мы будем вызывать ExtractTexture с deviceContext.
                            // Чтобы не кардинально менять — перегрузим локально: будем dynamic_casting device to ID3D11Device* only and assume caller will pass deviceContext via global? inconvenient.
                            // Решение: изменим прототип ExtractTexture чтобы принимать deviceContext. (реализовано ниже — перегрузку выполним).
                            // Но сейчас функция не может завершиться корректно без deviceContext — поэтому вызываем другой вариант в cpp (ниже).
                            DeleteFileA(filename.c_str());
                            return nullptr;
                        }
                        else
                        {
                            // raw RGBA data — создать texture2d напрямую (rare). Попробуем создать ID3D11Texture2D из tex->pcData (RGBA8888).
                            D3D11_TEXTURE2D_DESC desc = {};
                            desc.Width = tex->mWidth;
                            desc.Height = tex->mHeight;
                            desc.MipLevels = 1;
                            desc.ArraySize = 1;
                            // Выбираем SRGB формат для базового/диффузного цвета
                            desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                            desc.SampleDesc.Count = 1;
                            desc.Usage = D3D11_USAGE_DEFAULT;
                            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
                            D3D11_SUBRESOURCE_DATA data = {};
                            data.pSysMem = tex->pcData;
                            data.SysMemPitch = tex->mWidth * 4;
                            ID3D11Texture2D* tex2d = nullptr;
                            HRESULT hr = device->CreateTexture2D(&desc, &data, &tex2d);
                            if (FAILED(hr) || !tex2d) return nullptr;
                            ID3D11ShaderResourceView* srv = nullptr;
                            hr = device->CreateShaderResourceView(tex2d, nullptr, &srv);
                            tex2d->Release();
                            if (FAILED(hr)) { if (srv) srv->Release(); return nullptr; }
                            return srv;
                        }
                    }
                }
                else
                {
                    // Внешняя текстура — попробуем загрузить через TextureClass
                    TextureClass* t = new TextureClass();
                    // Initialize needs deviceContext — caller will call ExtractTexture with deviceContext in our improved overload.
                    // We'll leave this path to be handled by the overload used in Initialize.
                    delete t;
                }
            }
        }
    }

    // Попробуем вторую желаемую текстурную роль (type2) если задана
    if (type2 != aiTextureType_NONE)
    {
        if (mat->GetTextureCount(type2) > 0)
        {
            if (mat->GetTexture(type2, 0, &path) == AI_SUCCESS)
            {
                std::string texPath = path.C_Str();
                // Повторим логику выше — но ради краткости — вернём nullptr и позволим вызову выполнить альтернативные попытки.
                (void)texPath;
            }
        }
    }

    return nullptr;
}

bool AssimpModelClass::Initialize(ID3D11Device* device, ID3D11DeviceContext* deviceContext,
    const char* modelPath, const char* textureFilename)
{
    // 1. Загрузка геометрии через Assimp
    m_scene = m_importer.ReadFile(modelPath,
        aiProcess_Triangulate |
        aiProcess_FlipUVs |
        aiProcess_GenNormals |
        aiProcess_CalcTangentSpace);
    if (!m_scene || (m_scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !m_scene->mRootNode)
    {
        std::string err = m_importer.GetErrorString();
        MessageBoxA(NULL, err.c_str(), "Assimp Load Error", MB_OK);
        return false;
    }

    OutputDebugStringA(("Assimp: Model loaded successfully. Meshes count: " + std::to_string(m_scene->mNumMeshes) + "\n").c_str());

    m_totalIndexCount = 0;
    m_meshParts.clear();

    // 2. Проход по всем мешам -> создаём VB/IB
    for (unsigned int i = 0; i < m_scene->mNumMeshes; i++)
    {
        aiMesh* mesh = m_scene->mMeshes[i];

        OutputDebugStringA(("--- Processing Mesh # " + std::to_string(i) + " ---\n").c_str());
        OutputDebugStringA(("Vertices: " + std::to_string(mesh->mNumVertices) + "\n").c_str());
        OutputDebugStringA(("Faces: " + std::to_string(mesh->mNumFaces) + "\n").c_str());

        if (mesh->mNumVertices == 0)
        {
            OutputDebugStringA("WARNING: Skipping mesh with 0 vertices.\n");
            continue;
        }

        MeshPart part;
        part.indexCount = static_cast<int>(mesh->mNumFaces) * 3;
        m_totalIndexCount += part.indexCount;

        std::vector<VertexType> vertices;
        vertices.reserve(mesh->mNumVertices);

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

            if (mesh->HasTangentsAndBitangents())
            {
                vertex.tangent = XMFLOAT3(mesh->mTangents[j].x, mesh->mTangents[j].y, mesh->mTangents[j].z);
                vertex.binormal = XMFLOAT3(mesh->mBitangents[j].x, mesh->mBitangents[j].y, mesh->mBitangents[j].z);
            }
            else
            {
                vertex.tangent = XMFLOAT3(0.0f, 0.0f, 0.0f);
                vertex.binormal = XMFLOAT3(0.0f, 0.0f, 0.0f);
            }

            vertices.push_back(vertex);
        }

        std::vector<unsigned long> indices;
        indices.reserve(part.indexCount);
        for (unsigned int f = 0; f < mesh->mNumFaces; f++)
        {
            aiFace face = mesh->mFaces[f];
            for (unsigned int k = 0; k < face.mNumIndices; k++)
                indices.push_back(face.mIndices[k]);
        }

        // Создание буферов
        D3D11_BUFFER_DESC vbDesc = {};
        vbDesc.Usage = D3D11_USAGE_DEFAULT;
        vbDesc.ByteWidth = sizeof(VertexType) * static_cast<UINT>(vertices.size());
        vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA vbData = {};
        vbData.pSysMem = vertices.data();

        if (FAILED(device->CreateBuffer(&vbDesc, &vbData, &part.vertexBuffer)))
        {
            OutputDebugStringA("ERROR: Failed to create Vertex Buffer!\n");
            return false;
        }

        D3D11_BUFFER_DESC ibDesc = {};
        ibDesc.Usage = D3D11_USAGE_DEFAULT;
        ibDesc.ByteWidth = sizeof(unsigned long) * static_cast<UINT>(indices.size());
        ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

        D3D11_SUBRESOURCE_DATA ibData = {};
        ibData.pSysMem = indices.data();

        if (FAILED(device->CreateBuffer(&ibDesc, &ibData, &part.indexBuffer)))
        {
            OutputDebugStringA("ERROR: Failed to create Index Buffer!\n");
            return false;
        }

        // Попытаемся извлечь текстуры для материала этого меша (если есть)
       
            if (mesh->mMaterialIndex >= 0 && mesh->mMaterialIndex < static_cast<int>(m_scene->mNumMaterials))
            {
                const aiMaterial* mat = m_scene->mMaterials[mesh->mMaterialIndex];

                // Albedo / BaseColor
                part.texture = ExtractTexture_Internal(device, deviceContext, m_scene, mat,  aiTextureType_BASE_COLOR, aiTextureType_DIFFUSE);

                // Normal map
                part.normalMap = ExtractTexture_Internal(device, deviceContext, m_scene, mat,  aiTextureType_NORMALS, aiTextureType_HEIGHT);

                // Metalness & Roughness
                part.metalness = ExtractTexture_Internal(device, deviceContext, m_scene, mat, aiTextureType_METALNESS, aiTextureType_UNKNOWN);
                part.roughness = ExtractTexture_Internal(device, deviceContext, m_scene, mat,  aiTextureType_DIFFUSE_ROUGHNESS, aiTextureType_UNKNOWN);

                // --- НОВАЯ ЛОГИКА: Заглушка из цвета ТОЛЬКО если:
                // 1. part.texture == nullptr
                // 2. И модель НЕ загружена с внешней текстурой (m_Texture == nullptr)
                // Это предотвращает перезапись stone.tga для sphere.obj
                if (!part.texture && !m_Texture) // ← КЛЮЧЕВОЕ УСЛОВИЕ!
                {
                    aiColor4D albedoColor = GetPBRColorFromMaterial(mat, aiTextureType_BASE_COLOR);
                    part.texture = Create1x1SRVFromColor(device, albedoColor);
                    if (part.texture)
                    {
                        OutputDebugStringA(("Created 1x1 albedo SRV from color for mesh " + std::to_string(i) + "\n").c_str());
                    }
                }

                if (!part.metalness && !m_Texture)
                {
                    float metallicFactor = 0.0f;
                    if (AI_SUCCESS == aiGetMaterialFloat(mat, "$mat.metallicFactor", 0, 0, &metallicFactor))
                        metallicFactor = std::clamp(metallicFactor, 0.0f, 1.0f);
                    aiColor4D metalColor(metallicFactor, metallicFactor, metallicFactor, 1.0f);
                    part.metalness = Create1x1SRVFromColor(device, metalColor);
                    if (part.metalness)
                    {
                        OutputDebugStringA(("Created 1x1 metalness SRV for mesh " + std::to_string(i) + "\n").c_str());
                    }
                }

                if (!part.roughness && !m_Texture)
                {
                    float roughnessFactor = 1.0f;
                    if (AI_SUCCESS == aiGetMaterialFloat(mat, "$mat.roughnessFactor", 0, 0, &roughnessFactor))
                        roughnessFactor = std::clamp(roughnessFactor, 0.0f, 1.0f);
                    aiColor4D roughColor(roughnessFactor, roughnessFactor, roughnessFactor, 1.0f);
                    part.roughness = Create1x1SRVFromColor(device, roughColor);
                    if (part.roughness)
                    {
                        OutputDebugStringA(("Created 1x1 roughness SRV for mesh " + std::to_string(i) + "\n").c_str());
                    }
                }
            }

        // Добавляем созданный part в список мешей
        m_meshParts.push_back(part);
        OutputDebugStringA(("Mesh #" + std::to_string(i) + " created successfully.\n").c_str());
    }

    if (m_meshParts.empty())
    {
        MessageBoxA(NULL, "No valid meshes found in the file!", "Assimp Error", MB_OK);
        return false;
    }

    // Если был передан внешний textureFilename — загрузим fallback текстуру так же как раньше
    m_Texture = nullptr;
    if (textureFilename != nullptr && textureFilename[0] != '\0')
    {
        m_Texture = new TextureClass;
        if (!m_Texture) return false;

        bool result = m_Texture->Initialize(device, deviceContext, textureFilename);
        if (!result)
        {
            MessageBoxA(NULL, "Не удалось загрузить textureFilename для Assimp модели!", "Ошибка", MB_OK);
            delete m_Texture;
            m_Texture = nullptr;
            return false;
        }
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
        // part.texture, part.normalMap, part.metalness, part.roughness
        // теперь могут быть "сырыми" SRV, созданными Create1x1SRVFromColor.
        // Они все равно освобождаются через Release.
        if (part.texture) { part.texture->Release(); part.texture = nullptr; }
        if (part.normalMap) { part.normalMap->Release(); part.normalMap = nullptr; }
        if (part.metalness) { part.metalness->Release(); part.metalness = nullptr; }
        if (part.roughness) { part.roughness->Release(); part.roughness = nullptr; }
    }
    m_meshParts.clear();
    m_totalIndexCount = 0;

    // Удаляем созданные TextureClass (они в Shutdown вызовут Release внутренних SRV)
    for (auto* t : m_ownedTextures)
    {
        if (t)
        {
            t->Shutdown();
            delete t;
        }
    }
    m_ownedTextures.clear(); // Очищаем вектор после удаления

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

    // Устанавливаем буферы первого меша (совместимость со старым кодом)
    deviceContext->IASetVertexBuffers(0, 1, &m_meshParts[0].vertexBuffer, &stride, &offset);
    deviceContext->IASetIndexBuffer(m_meshParts[0].indexBuffer, DXGI_FORMAT_R32_UINT, 0);
    deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

int AssimpModelClass::GetIndexCount()
{
    if (!m_meshParts.empty())
        return m_meshParts[0].indexCount;
    return m_totalIndexCount;
}

ID3D11ShaderResourceView* AssimpModelClass::GetTexture()
{
    // Возвращаем текстуру первого meshPart, если есть; иначе fallback m_Texture
    if (!m_meshParts.empty() && m_meshParts[0].texture)
        return m_meshParts[0].texture;
    if (m_Texture) return m_Texture->GetTexture();
    return nullptr;
}


aiColor4D AssimpModelClass::GetPBRColorFromMaterial(const aiMaterial* mat, aiTextureType type)
{
    if (!mat) return aiColor4D(1.0f, 1.0f, 1.0f, 1.0f); // Белый по умолчанию

    aiColor4D color(1.0f, 1.0f, 1.0f, 1.0f); // Цвет по умолчанию

    // Сопоставление aiTextureType с ключами Assimp
    switch (type)
    {
    case aiTextureType_BASE_COLOR:
    case aiTextureType_DIFFUSE:
        // Пробуем получить $clr.base, затем $clr.diffuse
        if (AI_SUCCESS != aiGetMaterialColor(mat, "$clr.base", 0, 0, &color))
        {
            aiGetMaterialColor(mat, AI_MATKEY_COLOR_DIFFUSE, &color);
        }
        break;
    case aiTextureType_SPECULAR:
        aiGetMaterialColor(mat, AI_MATKEY_COLOR_SPECULAR, &color);
        break;
    case aiTextureType_EMISSIVE:
        aiGetMaterialColor(mat, AI_MATKEY_COLOR_EMISSIVE, &color);
        break;
    case aiTextureType_AMBIENT:
        aiGetMaterialColor(mat, AI_MATKEY_COLOR_AMBIENT, &color);
        break;
    case aiTextureType_UNKNOWN: // Часто используется для факторов, если не текстура
        if (AI_SUCCESS == aiGetMaterialFloat(mat, "$mat.metallicFactor", 0, 0, &color.b))
        {
            color.r = color.g = color.b = color.a = color.b;
        }
        else if (AI_SUCCESS == aiGetMaterialFloat(mat, "$mat.roughnessFactor", 0, 0, &color.g))
        {
            color.r = color.g = color.b = color.a = color.g;
        }
        // По умолчанию оставим белый
        color = aiColor4D(1.0f, 1.0f, 1.0f, 1.0f);
        break;
    default:
        aiGetMaterialColor(mat, AI_MATKEY_COLOR_DIFFUSE, &color);
        break;
    }

    // Если в glTF/Blender материал содержит opacity < 1 или alphaMode=BLEND —
    // он может выглядеть «приглушённым/прозрачным» в PBR. По умолчанию для BASE_COLOR
    // мы принудительно делаем альфу = 1.0, чтобы отображать непрозрачный результат.
    if (type == aiTextureType_BASE_COLOR || type == aiTextureType_DIFFUSE)
    {
        if (color.a < 0.999f)
        {
            char dbg[256];
            sprintf_s(dbg, "AssimpModelClass: material base color alpha = %f -> forcing 1.0 for opaque render\n", color.a);
            OutputDebugStringA(dbg);
        }
        color.a = 1.0f;
    }

    // Clamp values to [0, 1]
    color.r = std::clamp(color.r, 0.0f, 1.0f);
    color.g = std::clamp(color.g, 0.0f, 1.0f);
    color.b = std::clamp(color.b, 0.0f, 1.0f);
    color.a = std::clamp(color.a, 0.0f, 1.0f);

    return color;
}

// --- НОВАЯ ФУНКЦИЯ: Создание 1x1 SRV из цвета ---
ID3D11ShaderResourceView* AssimpModelClass::Create1x1SRVFromColor(ID3D11Device* device, const aiColor4D& color)
{
    if (!device) return nullptr;

    // Преобразуем aiColor4D (float [0..1]) в RGBA8UNORM (byte [0..255])
    unsigned char px[4] = {
        static_cast<unsigned char>(color.r * 255.0f),
        static_cast<unsigned char>(color.g * 255.0f),
        static_cast<unsigned char>(color.b * 255.0f),
        static_cast<unsigned char>(color.a * 255.0f)
    };

    D3D11_TEXTURE2D_DESC td{};
    td.Width = 1;
    td.Height = 1;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // Используем UNORM, не SRGB, так как это фактор
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA sd{};
    sd.pSysMem = px;
    sd.SysMemPitch = 4;

    ID3D11Texture2D* tex = nullptr;
    HRESULT hr = device->CreateTexture2D(&td, &sd, &tex);
    if (FAILED(hr) || !tex)
    {
        return nullptr;
    }

    ID3D11ShaderResourceView* srv = nullptr;
    hr = device->CreateShaderResourceView(tex, nullptr, &srv);
    tex->Release(); // Освобождаем текстуру, SRV держит ссылку

    return SUCCEEDED(hr) ? srv : nullptr;
}


// ============================================================================
// ExtractTexture_Internal: Извлекает встроенную или внешнюю текстуру из материала
// ============================================================================
ID3D11ShaderResourceView* AssimpModelClass::ExtractTexture_Internal(ID3D11Device* device, ID3D11DeviceContext* deviceContext,
    const aiScene* scene, const aiMaterial* mat, aiTextureType type1, aiTextureType type2)
{
    if (!mat) return nullptr;
    aiString path;
    if (mat->GetTextureCount(type1) > 0)
    {
        if (mat->GetTexture(type1, 0, &path) == AI_SUCCESS)
        {
            std::string texPath = path.C_Str();
            if (!texPath.empty())
            {
                if (texPath[0] == '*') // Встроенная текстура (например, *0)
                {
                    int index = atoi(texPath.c_str() + 1);
                    if (scene && index >= 0 && index < static_cast<int>(scene->mNumTextures))
                    {
                        const aiTexture* tex = scene->mTextures[index];
                        if (!tex) return nullptr;

                        if (tex->mHeight == 0) // Сжатый поток (png, jpg и т.д.)
                        {
                            char tmpPath[MAX_PATH] = { 0 };
                            if (GetTempPathA(MAX_PATH, tmpPath) == 0) return nullptr;
                            char tmpFile[MAX_PATH] = { 0 };
                            if (GetTempFileNameA(tmpPath, "glb", 0, tmpFile) == 0) return nullptr;
                            std::string filename(tmpFile);

                            // Записываем сжатые данные во временный файл
                            if (!WriteTempFile(filename, reinterpret_cast<const char*>(tex->pcData), tex->mWidth))
                            {
                                DeleteFileA(filename.c_str());
                                return nullptr;
                            }

                            TextureClass* t = new TextureClass();
                            if (!t)
                            {
                                DeleteFileA(filename.c_str());
                                return nullptr;
                            }

                            // Инициализируем TextureClass из временного файла
                            if (!t->Initialize(device, deviceContext, filename.c_str()))
                            {
                                t->Shutdown();
                                delete t;
                                DeleteFileA(filename.c_str());
                                return nullptr;
                            }

                            // Сохраняем владение TextureClass, чтобы SRV оставалась валидной
                            m_ownedTextures.push_back(t);

                            // Удаляем временный файл
                            DeleteFileA(filename.c_str());

                            // Возвращаем SRV из созданного TextureClass
                            return t->GetTexture();
                        }
                        else // Raw RGBA8888 данные
                        {
                            D3D11_TEXTURE2D_DESC desc = {};
                            desc.Width = tex->mWidth;
                            desc.Height = tex->mHeight;
                            desc.MipLevels = 1;
                            desc.ArraySize = 1;
                            // Выбираем SRGB формат для BASE_COLOR/DIFFUSE
                            desc.Format = (type1 == aiTextureType_BASE_COLOR || type1 == aiTextureType_DIFFUSE)
                                ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
                                : DXGI_FORMAT_R8G8B8A8_UNORM;
                            desc.SampleDesc.Count = 1;
                            desc.Usage = D3D11_USAGE_DEFAULT;
                            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
                            D3D11_SUBRESOURCE_DATA data = {};
                            data.pSysMem = tex->pcData;
                            data.SysMemPitch = tex->mWidth * 4;
                            ID3D11Texture2D* tex2d = nullptr;
                            HRESULT hr = device->CreateTexture2D(&desc, &data, &tex2d);
                            if (FAILED(hr) || !tex2d) return nullptr;
                            ID3D11ShaderResourceView* srv = nullptr;
                            hr = device->CreateShaderResourceView(tex2d, nullptr, &srv);
                            tex2d->Release();
                            if (FAILED(hr)) { if (srv) srv->Release(); return nullptr; }
                            return srv;
                        }
                    }
                }
                else // Внешний файл (относительный путь из .glb/.gltf)
                {
                    TextureClass* t = new TextureClass();
                    if (!t) return nullptr;

                    // Решаем, использовать ли SRGB для внешней текстуры
                    bool forceSRGB = (type1 == aiTextureType_BASE_COLOR || type1 == aiTextureType_DIFFUSE);

                    if (!t->Initialize(device, deviceContext, texPath.c_str(), forceSRGB))
                    {
                        t->Shutdown();
                        delete t;
                        return nullptr;
                    }

                    // Сохраняем владение TextureClass
                    m_ownedTextures.push_back(t);
                    return t->GetTexture();
                }
            }
        }
    }

    // Если не найдено по type1, пробуем type2
    if (type2 != aiTextureType_NONE)
    {
        if (mat->GetTextureCount(type2) > 0)
        {
            if (mat->GetTexture(type2, 0, &path) == AI_SUCCESS)
            {
                // Рекурсивный вызов для второй роли
                return ExtractTexture_Internal(device, deviceContext, scene, mat, type2, aiTextureType_NONE);
            }
        }
    }

    return nullptr; // Не найдено
}