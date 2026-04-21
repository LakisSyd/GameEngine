#include "assimpmodelclass.h"
#include <iostream>
#include <Windows.h> // Для ZeroMemory, GetTempPathA/GetTempFileNameA
#include <fstream>
#include <GltfMaterial.h>

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
        // Попробуем записать как raw RGBA и затем загружать вручную — но stbi не умеет raw без заголовка.
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
        aiProcess_GenSmoothNormals |
        aiProcess_CalcTangentSpace);
    if (!m_scene || (m_scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !m_scene->mRootNode)
    {
        std::string err = m_importer.GetErrorString();
        MessageBoxA(NULL, err.c_str(), "Assimp Load Error", MB_OK);
        return false;
    }

    std::string path(modelPath);
    if (path.size() >= 4) {
        std::string ext = path.substr(path.size() - 4);
        // Приводим к нижнему регистру для надёжности
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        m_isEmbeddedFormat = (ext == ".glb" || ext == ".gltf");
    }
    else {
        m_isEmbeddedFormat = false;
    }


    OutputDebugStringA(("Assimp: Model loaded successfully. Meshes count: " + std::to_string(m_scene->mNumMeshes) + "\n").c_str());

    m_totalIndexCount = 0;
    m_meshParts.clear();

    // 🔥 ЗАГРУЖАЕМ КОСТИ ПЕРЕД СОЗДАНИЕМ МЕШЕЙ
    LoadBones(m_scene);

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

            // 🔥 ИНИЦИАЛИЗАЦИЯ ВЕСОВ КОСТЕЙ
            for (int b = 0; b < 4; ++b) {
                vertex.boneIDs[b] = -1;
                vertex.boneWeights[b] = 0.0f;
            }

            // 🔥 ЗАПОЛНЯЕМ ВЕСА КОСТЕЙ ИЗ ДАННЫХ ASSIMP
            for (unsigned int b = 0; b < mesh->mNumBones; ++b)
            {
                aiBone* bone = mesh->mBones[b];
                for (unsigned int w = 0; w < bone->mNumWeights; ++w)
                {
                    if (bone->mWeights[w].mVertexId == j)
                    {
                        int boneIndex = m_boneMap[bone->mName.C_Str()];
                        float weight = bone->mWeights[w].mWeight;

                        // Ищем свободный слот или заменяем наименьший вес
                        for (int slot = 0; slot < 4; ++slot)
                        {
                            if (vertex.boneIDs[slot] == -1)
                            {
                                vertex.boneIDs[slot] = boneIndex;
                                vertex.boneWeights[slot] = weight;
                                break;
                            }
                            else if (vertex.boneWeights[slot] < weight)
                            {
                                vertex.boneIDs[slot] = boneIndex;
                                vertex.boneWeights[slot] = weight;
                                break;
                            }
                        }
                        break; // Переходим к следующей кости для этой вершины
                    }
                }
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
                {
                    // Читаем alphaMode (glTF: "OPAQUE", "MASK", "BLEND")
                    aiString alphaMode;
                    mat->Get(AI_MATKEY_GLTF_ALPHAMODE, alphaMode);
                    std::string alphaModeStr = alphaMode.C_Str();

                    // Читаем opacity (0..1)
                    float opacity = 1.0f;
                    aiGetMaterialFloat(mat, AI_MATKEY_OPACITY, &opacity);

                    // Читаем transmission (glTF: KHR_materials_transmission)
                    // Правильный ключ: AI_MATKEY_TRANSMISSION_FACTOR = "$mat.transmission.factor"
                    float transmission = 0.0f;
                    aiGetMaterialFloat(mat, AI_MATKEY_TRANSMISSION_FACTOR, &transmission);

                    part.isTransparent = (alphaModeStr == "BLEND") || (opacity < 0.999f) || (transmission > 0.01f);
                    part.opacity = opacity;

                    // ── Определяем стекло ────────────────────────────────────
                    float metallicFactor = 0.0f;
                    aiGetMaterialFloat(mat, AI_MATKEY_METALLIC_FACTOR, &metallicFactor);

                    // IOR: пробуем стандартный ключ Assimp (классические форматы)
                    float ior = 1.0f;
                    aiGetMaterialFloat(mat, AI_MATKEY_REFRACTI, &ior);

                    // Проверяем имя материала на наличие "glass"
                    aiString matName;
                    mat->Get(AI_MATKEY_NAME, matName);
                    std::string matNameStr = matName.C_Str();
                    std::string matNameLower = matNameStr;
                    for (auto& c : matNameLower) c = static_cast<char>(tolower(c));
                    bool nameIsGlass = (matNameLower.find("glass") != std::string::npos);

                    // ── Логика определения стекла ────────────────────────────
                    // 1) Прозрачный + (высокий metallic или transmission)
                    bool isGlassOriginal = part.isTransparent && (metallicFactor > 0.5f || transmission > 0.01f);
                    // 2) Имя содержит "glass" (Blender glass trick: metallic=1)
                    bool isGlassByName   = nameIsGlass;
                    // 3) IOR > 1.0 + высокий metallic
                    bool isGlassByIOR    = (ior > 1.01f) && (metallicFactor > 0.5f);
                    // 4) Высокий metallic + Render Method Blended (Blender ставит alphaMode=BLEND)
                    bool isGlassByMetal  = (metallicFactor > 0.9f) && (alphaModeStr == "BLEND");

                    part.isGlass = isGlassOriginal || isGlassByName || isGlassByIOR || isGlassByMetal;

                    // Если стекло обнаружено — принудительно помечаем прозрачным
                    if (part.isGlass && !part.isTransparent)
                    {
                        part.isTransparent = true;
                    }
                    // Назначаем разумную прозрачность для стекла
                    if (part.isGlass && part.opacity > 0.5f)
                    {
                        part.opacity = 0.12f; // Чистое стекло: почти прозрачное
                    }

                    char dbg[512];
                    sprintf_s(dbg, "Mesh #%u: name='%s' alphaMode='%s' opacity=%.3f metallic=%.3f "
                        "transmission=%.3f ior=%.3f isTransparent=%d isGlass=%d "
                        "(byOriginal=%d byName=%d byIOR=%d byMetal=%d)\n",
                        i, matNameStr.c_str(), alphaModeStr.c_str(), opacity, metallicFactor,
                        transmission, ior,
                        (int)part.isTransparent, (int)part.isGlass,
                        (int)isGlassOriginal, (int)isGlassByName,
                        (int)isGlassByIOR, (int)isGlassByMetal);
                    OutputDebugStringA(dbg);
                }
            }

           
                for (unsigned int m = 0; m < m_scene->mNumMeshes; ++m)
                {
                    aiMesh* mesh = m_scene->mMeshes[m];
                    if (mesh->mNumBones == 0) continue;

                    for (unsigned int b = 0; b < mesh->mNumBones; ++b)
                    {
                        aiBone* bone = mesh->mBones[b];
                        if (m_boneMap.find(bone->mName.C_Str()) == m_boneMap.end())
                        {
                            int boneIndex = m_maxBones++;
                            m_boneMap[bone->mName.C_Str()] = boneIndex;
                            // ✅ Создаём XMFLOAT4X4 и записываем в него единичную матрицу
                            XMFLOAT4X4 identityMatrix;
                            XMStoreFloat4x4(&identityMatrix, XMMatrixIdentity());
                            m_boneMatrices.push_back(identityMatrix);
                        }

                        int boneIndex = m_boneMap[bone->mName.C_Str()];

                        for (unsigned int w = 0; w < bone->mNumWeights; ++w)
                        {
                            unsigned int vertexID = bone->mWeights[w].mVertexId;
                            float weight = bone->mWeights[w].mWeight;

                            // Находим свободный слот (до 4 костей)
                            for (int slot = 0; slot < 4; ++slot)
                            {
                                if (m_meshParts[m].vertexBuffer == nullptr) break; // Защита
                                // Тут нужно обратиться к вершинам, которые ты создавал выше.
                                // Лучше сохранить vertices[] в локальную переменную перед созданием VB
                                // Пример доступа (адаптируй под свой код):
                                // vertices[vertexID].boneIDs[slot] = boneIndex;
                                // vertices[vertexID].boneWeights[slot] = weight;
                                break;
                            }
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


    // Предварительно вычисляем bounds для кэширования
    XMFLOAT3 min, max, center;
    float radius;
    GetBoundingBox(&min, &max);
    GetBoundingSphere(&center, &radius);

    LoadAnimations(m_scene);

    return true;

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
    m_boundsCached = false;
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

    // 🔥 Освобождаем буфер матриц костей
    if (m_boneMatrixBuffer)
    {
        m_boneMatrixBuffer->Release();
        m_boneMatrixBuffer = nullptr;
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

void AssimpModelClass::RenderAll(ID3D11DeviceContext* deviceContext,
    TextureShaderClass* shader,
    XMMATRIX world, XMMATRIX view, XMMATRIX proj)
{
    if (m_meshParts.empty()) return;

    unsigned int stride = sizeof(VertexType);
    unsigned int offset = 0;

    // 🔥 Обновляем анимацию (если есть)
    if (HasAnimations())
    {
        static float animationTime = 0.0f;
        animationTime += 0.016f; // ~60 FPS
        UpdateAnimation(animationTime, 0);
        SetBoneMatrices(deviceContext);
    }

    // Рендерим все mesh parts
    for (const auto& part : m_meshParts)
    {
        deviceContext->IASetVertexBuffers(0, 1, &part.vertexBuffer, &stride, &offset);
        deviceContext->IASetIndexBuffer(part.indexBuffer, DXGI_FORMAT_R32_UINT, 0);
        deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        // Устанавливаем текстуры
        if (shader)
        {
            // Передаём матрицы в шейдер
            shader->SetMatrixBuffers(deviceContext, world, view, proj);

            // Устанавливаем текстуры для PBR
            if (part.texture)
                deviceContext->PSSetShaderResources(0, 1, &part.texture);
            if (part.normalMap)
                deviceContext->PSSetShaderResources(1, 1, &part.normalMap);
            if (part.metalness)
                deviceContext->PSSetShaderResources(2, 1, &part.metalness);
            if (part.roughness)
                deviceContext->PSSetShaderResources(3, 1, &part.roughness);

            // Рендерим
            shader->RenderShader(deviceContext, part.indexCount);
        }
        else
        {
            // Fallback: просто рендерим без шейдера
            deviceContext->DrawIndexed(part.indexCount, 0, 0);
        }
    }
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



// ============================================================================
// GetBoundingBox — вычисляет AABB (Axis-Aligned Bounding Box)
// ============================================================================
void AssimpModelClass::GetBoundingBox(XMFLOAT3* min, XMFLOAT3* max) const
{
    if (!min || !max) return;

    // Если уже вычислены — возвращаем кэшированные
    if (m_boundsCached)
    {
        *min = m_cachedBBMin;
        *max = m_cachedBBMax;
        return;
    }

    // Инициализация экстремальных значений
    XMFLOAT3 bbMin(FLT_MAX, FLT_MAX, FLT_MAX);
    XMFLOAT3 bbMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    // Проходим по всем мешам модели
    if (m_scene)
    {
        for (unsigned int i = 0; i < m_scene->mNumMeshes; i++)
        {
            aiMesh* mesh = m_scene->mMeshes[i];
            for (unsigned int j = 0; j < mesh->mNumVertices; j++)
            {
                aiVector3D& v = mesh->mVertices[j];

                // Обновляем минимум
                if (v.x < bbMin.x) bbMin.x = v.x;
                if (v.y < bbMin.y) bbMin.y = v.y;
                if (v.z < bbMin.z) bbMin.z = v.z;

                // Обновляем максимум
                if (v.x > bbMax.x) bbMax.x = v.x;
                if (v.y > bbMax.y) bbMax.y = v.y;
                if (v.z > bbMax.z) bbMax.z = v.z;
            }
        }
    }

    // Кэшируем результат
    m_cachedBBMin = bbMin;
    m_cachedBBMax = bbMax;
    m_boundsCached = true;

    *min = bbMin;
    *max = bbMax;

    // DEBUG: выводим размеры
    char buf[256];
    sprintf_s(buf, "BoundingBox: Min(%.2f, %.2f, %.2f) Max(%.2f, %.2f, %.2f)\n",
        bbMin.x, bbMin.y, bbMin.z, bbMax.x, bbMax.y, bbMax.z);
    OutputDebugStringA(buf);
}

// ============================================================================
// GetBoundingSphere — вычисляет описанную сферу вокруг модели
// ============================================================================
void AssimpModelClass::GetBoundingSphere(XMFLOAT3* center, float* radius) const
{
    if (!center || !radius) return;

    // Если уже вычислены — возвращаем кэшированные
    if (m_boundsCached && m_cachedSphereRadius > 0.0f)
    {
        *center = m_cachedSphereCenter;
        *radius = m_cachedSphereRadius;
        return;
    }

    // Сначала вычисляем AABB
    XMFLOAT3 bbMin, bbMax;
    GetBoundingBox(&bbMin, &bbMax);

    // Центр сферы = центр AABB
    XMFLOAT3 sphereCenter(
        (bbMin.x + bbMax.x) * 0.5f,
        (bbMin.y + bbMax.y) * 0.5f,
        (bbMin.z + bbMax.z) * 0.5f
    );

    // Радиус = максимальное расстояние от центра до любой вершины
    float maxDistSq = 0.0f;

    if (m_scene)
    {
        for (unsigned int i = 0; i < m_scene->mNumMeshes; i++)
        {
            aiMesh* mesh = m_scene->mMeshes[i];
            for (unsigned int j = 0; j < mesh->mNumVertices; j++)
            {
                aiVector3D& v = mesh->mVertices[j];

                float dx = v.x - sphereCenter.x;
                float dy = v.y - sphereCenter.y;
                float dz = v.z - sphereCenter.z;
                float distSq = dx * dx + dy * dy + dz * dz;

                if (distSq > maxDistSq)
                    maxDistSq = distSq;
            }
        }
    }

    float sphereRadius = sqrtf(maxDistSq);

    // Кэшируем результат
    m_cachedSphereCenter = sphereCenter;
    m_cachedSphereRadius = sphereRadius;

    *center = sphereCenter;
    *radius = sphereRadius;

    // DEBUG: выводим радиус
    char buf[256];
    sprintf_s(buf, "BoundingSphere: Center(%.2f, %.2f, %.2f) Radius=%.3f\n",
        sphereCenter.x, sphereCenter.y, sphereCenter.z, sphereRadius);
    OutputDebugStringA(buf);
}

void AssimpModelClass::GetVertices(std::vector<XMFLOAT3>& outVertices) const
{
    outVertices.clear();
    if (!m_scene) return;

    for (unsigned int i = 0; i < m_scene->mNumMeshes; ++i)
    {
        const aiMesh* mesh = m_scene->mMeshes[i];
        if (!mesh || !mesh->HasPositions()) continue;

        for (unsigned int j = 0; j < mesh->mNumVertices; ++j)
        {
            const aiVector3D& v = mesh->mVertices[j];
            outVertices.emplace_back(v.x, v.y, v.z);
        }
    }
}
void AssimpModelClass::GetRawVertices(std::vector<XMFLOAT3>& outVertices) const
{
    outVertices.clear();
    if (!m_scene) return;

    for (unsigned int i = 0; i < m_scene->mNumMeshes; ++i)
    {
        const aiMesh* mesh = m_scene->mMeshes[i];
        if (!mesh || !mesh->HasPositions()) continue;

        for (unsigned int j = 0; j < mesh->mNumVertices; ++j)
        {
            const aiVector3D& v = mesh->mVertices[j];
            outVertices.emplace_back(v.x, v.y, v.z);
        }
    }
}

void AssimpModelClass::GetRawTriangles(std::vector<XMFLOAT3>& outTriangles) const
{
    outTriangles.clear();
    if (!m_scene) return;

    for (unsigned int i = 0; i < m_scene->mNumMeshes; ++i)
    {
        const aiMesh* mesh = m_scene->mMeshes[i];
        if (!mesh || !mesh->HasPositions() || !mesh->HasFaces()) continue;

        for (unsigned int f = 0; f < mesh->mNumFaces; f++)
        {
            const aiFace& face = mesh->mFaces[f];
            if (face.mNumIndices != 3) continue; // только треугольники

            for (unsigned int k = 0; k < 3; ++k)
            {
                const aiVector3D& v = mesh->mVertices[face.mIndices[k]];
                outTriangles.emplace_back(v.x, v.y, v.z);
            }
        }
    }
}

void AssimpModelClass::LoadBones(const aiScene* scene)
{
    m_boneMap.clear();
    m_boneOffsets.clear();
    m_boneMatrices.clear();
    m_maxBones = 0;

    for (unsigned int m = 0; m < scene->mNumMeshes; ++m)
    {
        aiMesh* mesh = scene->mMeshes[m];

        for (unsigned int b = 0; b < mesh->mNumBones; ++b)
        {
            aiBone* bone = mesh->mBones[b];
            std::string boneName = bone->mName.C_Str();

            // Регистрируем новую кость
            if (m_boneMap.find(boneName) == m_boneMap.end())
            {
                int boneIndex = m_maxBones++;
                m_boneMap[boneName] = boneIndex;

                // Инициализируем финальную матрицу (единичную)
                XMFLOAT4X4 identity;
                XMStoreFloat4x4(&identity, XMMatrixIdentity());
                m_boneMatrices.push_back(identity);

                // Сохраняем offset matrix (inverse bind pose)
                m_boneOffsets.push_back(ConvertMatrix(bone->mOffsetMatrix));
            }
        }
    }

    OutputDebugStringA(("Loaded " + std::to_string(m_maxBones) + " bones\n").c_str());
}

void AssimpModelClass::LoadAnimations(const aiScene* scene)
{
    m_animations.clear();

    for (unsigned int i = 0; i < scene->mNumAnimations; ++i)
    {
        aiAnimation* anim = scene->mAnimations[i];
        Animation newAnim;

        newAnim.name = anim->mName.C_Str();
        newAnim.duration = (float)anim->mDuration;
        newAnim.ticksPerSecond = (anim->mTicksPerSecond == 0.0) ? 24.0f : (float)anim->mTicksPerSecond;

        for (unsigned int c = 0; c < anim->mNumChannels; ++c)
        {
            aiNodeAnim* channel = anim->mChannels[c];
            BoneAnimation boneAnim;

            boneAnim.name = channel->mNodeName.C_Str();

            // Позиции
            for (unsigned int k = 0; k < channel->mNumPositionKeys; ++k)
            {
                KeyPosition kp;
                kp.position = XMFLOAT3(
                    channel->mPositionKeys[k].mValue.x,
                    channel->mPositionKeys[k].mValue.y,
                    channel->mPositionKeys[k].mValue.z
                );
                kp.timeStamp = (float)channel->mPositionKeys[k].mTime;
                boneAnim.positions.push_back(kp);
            }

            // Вращения (кватернионы)
            for (unsigned int k = 0; k < channel->mNumRotationKeys; ++k)
            {
                KeyRotation kr;
                kr.orientation = XMFLOAT4(
                    channel->mRotationKeys[k].mValue.x,
                    channel->mRotationKeys[k].mValue.y,
                    channel->mRotationKeys[k].mValue.z,
                    channel->mRotationKeys[k].mValue.w
                );
                kr.timeStamp = (float)channel->mRotationKeys[k].mTime;
                boneAnim.rotations.push_back(kr);
            }

            // Масштаб
            for (unsigned int k = 0; k < channel->mNumScalingKeys; ++k)
            {
                KeyScale ks;
                ks.scale = XMFLOAT3(
                    channel->mScalingKeys[k].mValue.x,
                    channel->mScalingKeys[k].mValue.y,
                    channel->mScalingKeys[k].mValue.z
                );
                ks.timeStamp = (float)channel->mScalingKeys[k].mTime;
                boneAnim.scales.push_back(ks);
            }

            newAnim.channels.push_back(boneAnim);
        }

        m_animations.push_back(newAnim);
        OutputDebugStringA(("Loaded animation: " + newAnim.name + "\n").c_str());
    }
}
XMFLOAT4X4 AssimpModelClass::InterpolatePosition(const std::vector<KeyPosition>& keys, float animTime)
{
    if (keys.empty()) {
        XMFLOAT4X4 identity; XMStoreFloat4x4(&identity, XMMatrixIdentity());
        return identity;
    }
    if (keys.size() == 1) {
        XMMATRIX mat = XMMatrixTranslation(keys[0].position.x, keys[0].position.y, keys[0].position.z);
        XMFLOAT4X4 result; XMStoreFloat4x4(&result, mat);
        return result;
    }

    // Находим два ключа, между которыми лежит animTime
    int idx = 0;
    while (idx < (int)keys.size() - 1 && keys[idx + 1].timeStamp <= animTime)
        ++idx;

    const KeyPosition& k1 = keys[idx];
    const KeyPosition& k2 = keys[(idx + 1) % keys.size()];

    float dt = k2.timeStamp - k1.timeStamp;
    if (dt < 0.0001f) dt = 1.0f;
    float factor = (animTime - k1.timeStamp) / dt;
    factor = std::clamp(factor, 0.0f, 1.0f);

    XMFLOAT3 interp;
    interp.x = k1.position.x + factor * (k2.position.x - k1.position.x);
    interp.y = k1.position.y + factor * (k2.position.y - k1.position.y);
    interp.z = k1.position.z + factor * (k2.position.z - k1.position.z);

    XMMATRIX mat = XMMatrixTranslation(interp.x, interp.y, interp.z);
    XMFLOAT4X4 result; XMStoreFloat4x4(&result, mat);
    return result;
}


XMFLOAT4X4 AssimpModelClass::InterpolateRotation(const std::vector<AssimpModelClass::KeyRotation>& keys, float animTime)
{
    if (keys.empty()) {
        XMFLOAT4X4 identity; XMStoreFloat4x4(&identity, XMMatrixIdentity());
        return identity;
    }
    if (keys.size() == 1) {
        XMMATRIX mat = XMMatrixRotationQuaternion(XMLoadFloat4(&keys[0].orientation));
        XMFLOAT4X4 result; XMStoreFloat4x4(&result, mat);
        return result;
    }

    int idx = 0;
    while (idx < (int)keys.size() - 1 && keys[idx + 1].timeStamp <= animTime)
        ++idx;

    const KeyRotation& k1 = keys[idx];
    const KeyRotation& k2 = keys[(idx + 1) % keys.size()];

    float dt = k2.timeStamp - k1.timeStamp;
    if (dt < 0.0001f) dt = 1.0f;
    float factor = (animTime - k1.timeStamp) / dt;
    factor = std::clamp(factor, 0.0f, 1.0f);

    XMVECTOR q1 = XMLoadFloat4(&k1.orientation);
    XMVECTOR q2 = XMLoadFloat4(&k2.orientation);
    XMVECTOR interp = XMQuaternionSlerp(q1, q2, factor);

    XMMATRIX mat = XMMatrixRotationQuaternion(interp);
    XMFLOAT4X4 result; XMStoreFloat4x4(&result, mat);
    return result;
}

// ── Интерполяция масштаба ──────────────────────────────────────────────
XMFLOAT4X4 AssimpModelClass::InterpolateScale(const std::vector<KeyScale>& keys, float animTime)
{
    if (keys.empty()) {
        XMFLOAT4X4 identity; XMStoreFloat4x4(&identity, XMMatrixIdentity());
        return identity;
    }
    if (keys.size() == 1) {
        XMMATRIX mat = XMMatrixScaling(keys[0].scale.x, keys[0].scale.y, keys[0].scale.z);
        XMFLOAT4X4 result; XMStoreFloat4x4(&result, mat);
        return result;
    }

    int idx = 0;
    while (idx < (int)keys.size() - 1 && keys[idx + 1].timeStamp <= animTime)
        ++idx;

    const KeyScale& k1 = keys[idx];
    const KeyScale& k2 = keys[(idx + 1) % keys.size()];

    float dt = k2.timeStamp - k1.timeStamp;
    if (dt < 0.0001f) dt = 1.0f;
    float factor = (animTime - k1.timeStamp) / dt;
    factor = std::clamp(factor, 0.0f, 1.0f);

    XMFLOAT3 interp;
    interp.x = k1.scale.x + factor * (k2.scale.x - k1.scale.x);
    interp.y = k1.scale.y + factor * (k2.scale.y - k1.scale.y);
    interp.z = k1.scale.z + factor * (k2.scale.z - k1.scale.z);

    XMMATRIX mat = XMMatrixScaling(interp.x, interp.y, interp.z);
    XMFLOAT4X4 result; XMStoreFloat4x4(&result, mat);
    return result;
}

XMFLOAT4X4 AssimpModelClass::InterpolateTransform(const BoneAnimation& channel, float animTime)
{
    XMFLOAT4X4 posMat = InterpolatePosition(channel.positions, animTime);
    XMFLOAT4X4 rotMat = InterpolateRotation(channel.rotations, animTime);
    XMFLOAT4X4 scaleMat = InterpolateScale(channel.scales, animTime);

    // Порядок: Scale → Rotate → Translate
    XMMATRIX s = XMLoadFloat4x4(&scaleMat);
    XMMATRIX r = XMLoadFloat4x4(&rotMat);
    XMMATRIX t = XMLoadFloat4x4(&posMat);

    XMMATRIX result = s * r * t;
    XMFLOAT4X4 out; XMStoreFloat4x4(&out, result);
    return out;
}

void AssimpModelClass::ReadNodeHierarchy(const aiNode* node,
    const XMFLOAT4X4& parentTransform, const Animation& anim, float animTime)
{
    std::string nodeName(node->mName.C_Str());

    // Текущая трансформация узла (из сцены)
    XMMATRIX nodeTransform = XMLoadFloat4x4(&ConvertMatrix(node->mTransformation));

    // Если эта кость анимируется — переопределяем трансформ ключевыми кадрами
    auto it = m_boneMap.find(nodeName);
    if (it != m_boneMap.end())
    {
        int boneIdx = it->second;

        // Находим канал анимации для этой кости
        for (const auto& channel : anim.channels)
        {
            if (channel.name == nodeName)
            {
                XMFLOAT4X4 interp = InterpolateTransform(channel, animTime);
                nodeTransform = XMLoadFloat4x4(&interp);
                break;
            }
        }
    }

    // Глобальная трансформация = родитель * локальная
    XMMATRIX parentMat = XMLoadFloat4x4(&parentTransform);
    XMMATRIX globalTransform = nodeTransform * parentMat;

    // Если это кость — вычисляем финальную матрицу для шейдера
    if (it != m_boneMap.end())
    {
        int boneIdx = it->second;
        XMMATRIX offsetMat = XMLoadFloat4x4(&m_boneOffsets[boneIdx]);

        // Финальная матрица = глобальная * offset (inverse bind)
        XMMATRIX finalMat = globalTransform * offsetMat;
        XMStoreFloat4x4(&m_boneMatrices[boneIdx], finalMat);
    }

    // Рекурсия по детям
    for (unsigned int i = 0; i < node->mNumChildren; ++i)
    {
        XMFLOAT4X4 globalFloat;
        XMStoreFloat4x4(&globalFloat, globalTransform);
        ReadNodeHierarchy(node->mChildren[i], globalFloat, anim, animTime);
    }
}

void AssimpModelClass::UpdateAnimation(float currentTime, int animIndex)
{
    if (m_animations.empty() || animIndex >= (int)m_animations.size())
        return;

    const Animation& anim = m_animations[animIndex];

    // Нормализуем время (циклическое воспроизведение)
    float ticksPerSecond = (anim.ticksPerSecond > 0.0f) ? anim.ticksPerSecond : 24.0f;
    float timeInSeconds = currentTime * ticksPerSecond;
    float animTime = fmod(timeInSeconds, anim.duration);

    // Запускаем рекурсивную сборку матриц от корня
    if (m_scene && m_scene->mRootNode)
    {
        XMFLOAT4X4 identity;
        XMStoreFloat4x4(&identity, XMMatrixIdentity());
        ReadNodeHierarchy(m_scene->mRootNode, identity, anim, animTime);
    }
}

void AssimpModelClass::SetBoneMatrices(ID3D11DeviceContext* deviceContext)
{
    if (m_boneMatrices.empty())
        return;

    // 🔥 Создаём константный буфер для матриц костей, если ещё не создан
    if (!m_boneMatrixBuffer)
    {
        D3D11_BUFFER_DESC cbDesc = {};
        cbDesc.Usage = D3D11_USAGE_DYNAMIC;
        cbDesc.ByteWidth = sizeof(XMFLOAT4X4) * 128;  // Максимум 128 костей
        cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        HRESULT hr = deviceContext->GetDevice()->CreateBuffer(&cbDesc, nullptr, &m_boneMatrixBuffer);
        if (FAILED(hr))
        {
            OutputDebugStringA("ERROR: Failed to create bone matrix buffer!\n");
            return;
        }
    }

    // 🔥 Обновляем буфер матрицами костей
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    HRESULT hr = deviceContext->Map(m_boneMatrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (SUCCEEDED(hr))
    {
        XMFLOAT4X4* boneMatrices = reinterpret_cast<XMFLOAT4X4*>(mappedResource.pData);
        
        // Копируем матрицы костей в буфер
        size_t numBones = min(m_boneMatrices.size(), 128);
        for (size_t i = 0; i < numBones; ++i)
        {
            boneMatrices[i] = m_boneMatrices[i];
        }
        
        deviceContext->Unmap(m_boneMatrixBuffer, 0);

        // 🔥 Устанавливаем буфер в слот b8 (как указано в pbr.vs)
        deviceContext->VSSetConstantBuffers(8, 1, &m_boneMatrixBuffer);
    }
}

XMFLOAT4X4 AssimpModelClass::ConvertMatrix(const aiMatrix4x4& from)
{
    XMFLOAT4X4 to;
    // Assimp: row-major, DirectX: по умолчанию тоже (если не менял компилятор)
    to._11 = from.a1; to._12 = from.a2; to._13 = from.a3; to._14 = from.a4;
    to._21 = from.b1; to._22 = from.b2; to._23 = from.b3; to._24 = from.b4;
    to._31 = from.c1; to._32 = from.c2; to._33 = from.c3; to._34 = from.c4;
    to._41 = from.d1; to._42 = from.d2; to._43 = from.d3; to._44 = from.d4;
    return to;
}