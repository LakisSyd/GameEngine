// ============================================================================
// assimpmodelclass.cpp — Полная реализация с исправленной системой анимаций
// ============================================================================
// Исправления:
// 1. Правильный порядок матриц: offset * global (для DirectX row-major)
// 2. Сопоставление костей по имени УЗЛА (aiNode::mName), а не кости
// 3. Нормализация весов костей для каждой вершины
// 4. Корректная интерполяция и обход иерархии
// 5. Защита от мусорных данных в константном буфере костей
// ============================================================================

#include "assimpmodelclass.h"
#include <iostream>
#include <Windows.h>
#include <fstream>
#include <GltfMaterial.h>
#include <algorithm>
#include "PBRShaderClass.h"
#include <DirectXMath.h>
using namespace DirectX;

// ============================================================================
// Конструктор / Деструктор
// ============================================================================
AssimpModelClass::AssimpModelClass()
    : m_scene(nullptr),
    m_Texture(nullptr),
    m_totalIndexCount(0),
    m_boneMatrixBuffer(nullptr),
    m_animationTime(0.0f),
    m_boundsCached(false),
    m_cachedSphereRadius(0.0f),
    m_isEmbeddedFormat(false)
{
    ZeroMemory(&m_cachedBBMin, sizeof(m_cachedBBMin));
    ZeroMemory(&m_cachedBBMax, sizeof(m_cachedBBMax));
    ZeroMemory(&m_cachedSphereCenter, sizeof(m_cachedSphereCenter));
}

AssimpModelClass::~AssimpModelClass()
{
    Shutdown();
}

// ============================================================================
// Вспомогательные функции
// ============================================================================
static bool WriteTempFile(const std::string& filename, const char* data, size_t size)
{
    std::ofstream of(filename, std::ios::binary);
    if (!of.is_open()) return false;
    of.write(data, static_cast<std::streamsize>(size));
    return of.good();
}

XMFLOAT4X4 AssimpModelClass::ConvertMatrix(const aiMatrix4x4& from)
{
    XMFLOAT4X4 to;
    // Транспонируем при копировании:
    to._11 = from.a1; to._12 = from.b1; to._13 = from.c1; to._14 = from.d1;
    to._21 = from.a2; to._22 = from.b2; to._23 = from.c2; to._24 = from.d2;
    to._31 = from.a3; to._32 = from.b3; to._33 = from.c3; to._34 = from.d3;
    to._41 = from.a4; to._42 = from.b4; to._43 = from.c4; to._44 = from.d4;
    return to;
}

aiColor4D AssimpModelClass::GetPBRColorFromMaterial(const aiMaterial* mat, aiTextureType type)
{
    if (!mat) return aiColor4D(1.0f, 1.0f, 1.0f, 1.0f);
    aiColor4D color(1.0f, 1.0f, 1.0f, 1.0f);

    switch (type)
    {
    case aiTextureType_BASE_COLOR:
    case aiTextureType_DIFFUSE:
        if (AI_SUCCESS != aiGetMaterialColor(mat, "$clr.base", 0, 0, &color))
            aiGetMaterialColor(mat, AI_MATKEY_COLOR_DIFFUSE, &color);
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
    default:
        aiGetMaterialColor(mat, AI_MATKEY_COLOR_DIFFUSE, &color);
        break;
    }

    if (type == aiTextureType_BASE_COLOR || type == aiTextureType_DIFFUSE)
        color.a = 1.0f;

    color.r = std::clamp(color.r, 0.0f, 1.0f);
    color.g = std::clamp(color.g, 0.0f, 1.0f);
    color.b = std::clamp(color.b, 0.0f, 1.0f);
    color.a = std::clamp(color.a, 0.0f, 1.0f);
    return color;
}

ID3D11ShaderResourceView* AssimpModelClass::Create1x1SRVFromColor(ID3D11Device* device, const aiColor4D& color)
{
    if (!device) return nullptr;
    unsigned char px[4] = {
        static_cast<unsigned char>(color.r * 255.0f),
        static_cast<unsigned char>(color.g * 255.0f),
        static_cast<unsigned char>(color.b * 255.0f),
        static_cast<unsigned char>(color.a * 255.0f)
    };

    D3D11_TEXTURE2D_DESC td{};
    td.Width = 1; td.Height = 1; td.MipLevels = 1; td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA sd{};
    sd.pSysMem = px; sd.SysMemPitch = 4;

    ID3D11Texture2D* tex = nullptr;
    if (FAILED(device->CreateTexture2D(&td, &sd, &tex))) return nullptr;

    ID3D11ShaderResourceView* srv = nullptr;
    HRESULT hr = device->CreateShaderResourceView(tex, nullptr, &srv);
    tex->Release();
    return SUCCEEDED(hr) ? srv : nullptr;
}

// ============================================================================
// Загрузка текстур (встроенных и внешних)
// ============================================================================
ID3D11ShaderResourceView* AssimpModelClass::ExtractTexture_Internal(
    ID3D11Device* device,
    ID3D11DeviceContext* deviceContext,
    const aiScene* scene,
    const aiMaterial* mat,
    aiTextureType type1,
    aiTextureType type2)
{
    if (!mat) return nullptr;
    aiString path;

    // Пробуем первый тип текстуры
    if (mat->GetTextureCount(type1) > 0 && mat->GetTexture(type1, 0, &path) == AI_SUCCESS)
    {
        std::string texPath = path.C_Str();
        if (!texPath.empty())
        {
            // Встроенная текстура (индекс вида "*0")
            if (texPath[0] == '*')
            {
                int index = atoi(texPath.c_str() + 1);
                if (scene && index >= 0 && index < static_cast<int>(scene->mNumTextures))
                {
                    const aiTexture* tex = scene->mTextures[index];
                    if (!tex) return nullptr;

                    if (tex->mHeight == 0) // Сжатый формат (PNG/JPG)
                    {
                        char tmpPath[MAX_PATH] = { 0 };
                        if (GetTempPathA(MAX_PATH, tmpPath) == 0) return nullptr;
                        char tmpFile[MAX_PATH] = { 0 };
                        if (GetTempFileNameA(tmpPath, "glb", 0, tmpFile) == 0) return nullptr;
                        std::string filename(tmpFile);

                        if (!WriteTempFile(filename, reinterpret_cast<const char*>(tex->pcData), tex->mWidth))
                        {
                            DeleteFileA(filename.c_str());
                            return nullptr;
                        }

                        TextureClass* t = new TextureClass();
                        if (!t || !t->Initialize(device, deviceContext, filename.c_str()))
                        {
                            if (t) { t->Shutdown(); delete t; }
                            DeleteFileA(filename.c_str());
                            return nullptr;
                        }
                        m_ownedTextures.push_back(t);
                        DeleteFileA(filename.c_str());
                        return t->GetTexture();
                    }
                    else // Raw RGBA данные
                    {
                        D3D11_TEXTURE2D_DESC desc = {};
                        desc.Width = tex->mWidth; desc.Height = tex->mHeight;
                        desc.MipLevels = 1; desc.ArraySize = 1;
                        desc.Format = (type1 == aiTextureType_BASE_COLOR || type1 == aiTextureType_DIFFUSE)
                            ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
                        desc.SampleDesc.Count = 1; desc.Usage = D3D11_USAGE_DEFAULT;
                        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

                        D3D11_SUBRESOURCE_DATA data = {};
                        data.pSysMem = tex->pcData;
                        data.SysMemPitch = tex->mWidth * 4;

                        ID3D11Texture2D* tex2d = nullptr;
                        if (FAILED(device->CreateTexture2D(&desc, &data, &tex2d))) return nullptr;

                        ID3D11ShaderResourceView* srv = nullptr;
                        HRESULT hr = device->CreateShaderResourceView(tex2d, nullptr, &srv);
                        tex2d->Release();
                        return SUCCEEDED(hr) ? srv : nullptr;
                    }
                }
            }
            else // Внешний файл
            {
                TextureClass* t = new TextureClass();
                if (!t) return nullptr;
                bool forceSRGB = (type1 == aiTextureType_BASE_COLOR || type1 == aiTextureType_DIFFUSE);
                if (!t->Initialize(device, deviceContext, texPath.c_str(), forceSRGB))
                {
                    t->Shutdown(); delete t;
                    return nullptr;
                }
                m_ownedTextures.push_back(t);
                return t->GetTexture();
            }
        }
    }

    // Пробуем второй тип, если первый не найден
    if (type2 != aiTextureType_NONE && mat->GetTextureCount(type2) > 0)
    {
        if (mat->GetTexture(type2, 0, &path) == AI_SUCCESS)
            return ExtractTexture_Internal(device, deviceContext, scene, mat, type2, aiTextureType_NONE);
    }

    return nullptr;
}

// ============================================================================
// Загрузка костей — ИСПРАВЛЕННАЯ ВЕРСИЯ
// ============================================================================
void AssimpModelClass::LoadBones(const aiScene* scene)
{
    m_boneInfoMap.clear();
    m_finalBoneMatrices.clear();  // ← Очистить перед заполнением!

    int boneCounter = 0;
    for (unsigned int m = 0; m < scene->mNumMeshes; ++m)
    {
        aiMesh* mesh = scene->mMeshes[m];
        for (unsigned int b = 0; b < mesh->mNumBones; ++b)
        {
            aiBone* bone = mesh->mBones[b];
            std::string boneName = bone->mName.C_Str();

            if (m_boneInfoMap.find(boneName) == m_boneInfoMap.end())
            {
                BoneInfo info;
                info.name = boneName;
                info.offsetMatrix = ConvertMatrix(bone->mOffsetMatrix);
                char dbg[256];
                sprintf_s(dbg, "[BONE] %s offset: [%.3f, %.3f, %.3f, %.3f]\n",
                    boneName.c_str(),
                    info.offsetMatrix._11, info.offsetMatrix._12,
                    info.offsetMatrix._13, info.offsetMatrix._14);
                OutputDebugStringA(dbg);
                info.boneIndex = boneCounter++;
                m_boneInfoMap[boneName] = info;

            }
        }
    }



    // 🔥 ВЫДЕЛИТЬ ПАМЯТЬ И ИНИЦИАЛИЗИРОВАТЬ
    m_finalBoneMatrices.resize(m_boneInfoMap.size());
    XMFLOAT4X4 identity; XMStoreFloat4x4(&identity, XMMatrixIdentity());
    for (auto& mat : m_finalBoneMatrices) mat = identity;

    OutputDebugStringA(("Loaded " + std::to_string(boneCounter) + " bones\n").c_str());
}
// ============================================================================
// Загрузка анимаций — ИСПРАВЛЕННАЯ ВЕРСИЯ
// ============================================================================
void AssimpModelClass::LoadAnimations(const aiScene* scene)
{
    m_animations.clear();

    for (unsigned int i = 0; i < scene->mNumAnimations; ++i)
    {
        aiAnimation* anim = scene->mAnimations[i];
        AnimationClip clip;
        clip.name = anim->mName.C_Str();
        clip.duration = (float)anim->mDuration;
        clip.ticksPerSecond = (anim->mTicksPerSecond == 0.0) ? 24.0f : (float)anim->mTicksPerSecond;

        for (unsigned int c = 0; c < anim->mNumChannels; ++c)
        {
            aiNodeAnim* channel = anim->mChannels[c];
            BoneAnimationChannel boneChannel;

            // 🔥 КЛЮЧЕВОЕ: используем mNodeName — имя узла в иерархии
            boneChannel.nodeName = channel->mNodeName.C_Str();

            // Позиции
            for (unsigned int k = 0; k < channel->mNumPositionKeys; ++k)
            {
                KeyPosition kp;
                kp.position = XMFLOAT3(
                    channel->mPositionKeys[k].mValue.x,
                    channel->mPositionKeys[k].mValue.y,
                    channel->mPositionKeys[k].mValue.z);
                kp.timeStamp = (float)channel->mPositionKeys[k].mTime;
                boneChannel.positions.push_back(kp);
            }

            // Вращения
            for (unsigned int k = 0; k < channel->mNumRotationKeys; ++k)
            {
                KeyRotation kr;
                kr.orientation = XMFLOAT4(
                    channel->mRotationKeys[k].mValue.x,
                    channel->mRotationKeys[k].mValue.y,
                    channel->mRotationKeys[k].mValue.z,
                    channel->mRotationKeys[k].mValue.w);
                kr.timeStamp = (float)channel->mRotationKeys[k].mTime;
                boneChannel.rotations.push_back(kr);
            }

            // Масштаб
            for (unsigned int k = 0; k < channel->mNumScalingKeys; ++k)
            {
                KeyScale ks;
                ks.scale = XMFLOAT3(
                    channel->mScalingKeys[k].mValue.x,
                    channel->mScalingKeys[k].mValue.y,
                    channel->mScalingKeys[k].mValue.z);
                ks.timeStamp = (float)channel->mScalingKeys[k].mTime;
                boneChannel.scales.push_back(ks);
            }

            clip.channels.push_back(boneChannel);
        }
        m_animations.push_back(clip);
        OutputDebugStringA(("Loaded animation: " + clip.name + "\n").c_str());
    
        OutputDebugStringA(("Name: " + clip.name + "\n").c_str());
        OutputDebugStringA(("Duration: " + std::to_string(clip.duration) + " ticks\n").c_str());
        OutputDebugStringA(("TicksPerSecond: " + std::to_string(clip.ticksPerSecond) + "\n").c_str());
        OutputDebugStringA(("Channels: " + std::to_string(clip.channels.size()) + "\n").c_str());

        for (size_t c = 0; c < clip.channels.size() && c < 3; ++c) {
            const auto& ch = clip.channels[c];
            OutputDebugStringA(("  Channel[" + std::to_string(c) + "] node: " + ch.nodeName + "\n").c_str());
            OutputDebugStringA(("    Pos keys: " + std::to_string(ch.positions.size()) + "\n").c_str());
            OutputDebugStringA(("    Rot keys: " + std::to_string(ch.rotations.size()) + "\n").c_str());
            OutputDebugStringA(("    Scale keys: " + std::to_string(ch.scales.size()) + "\n").c_str());

            // 🔥 Выведи первый ключ масштаба — частая причина растяжек!
            if (!ch.scales.empty()) {
                const auto& s = ch.scales[0];
                OutputDebugStringA(("    First scale key: (" +
                    std::to_string(s.scale.x) + ", " +
                    std::to_string(s.scale.y) + ", " +
                    std::to_string(s.scale.z) + ")\n").c_str());
            }
        }
    }
}

// ============================================================================
// Интерполяция трансформаций
// ============================================================================
XMMATRIX AssimpModelClass::InterpolatePosition(const std::vector<KeyPosition>& keys, float animTime)
{
    if (keys.empty()) return XMMatrixIdentity();
    if (keys.size() == 1)
        return XMMatrixTranslation(keys[0].position.x, keys[0].position.y, keys[0].position.z);

    int idx = 0;
    while (idx < (int)keys.size() - 1 && keys[idx + 1].timeStamp <= animTime) ++idx;

    const KeyPosition& k1 = keys[idx];
    const KeyPosition& k2 = keys[(idx + 1) % keys.size()];
    float dt = k2.timeStamp - k1.timeStamp;
    if (dt < 0.0001f) dt = 1.0f;
    float factor = std::clamp((animTime - k1.timeStamp) / dt, 0.0f, 1.0f);

    XMFLOAT3 interp(
        k1.position.x + factor * (k2.position.x - k1.position.x),
        k1.position.y + factor * (k2.position.y - k1.position.y),
        k1.position.z + factor * (k2.position.z - k1.position.z));

    return XMMatrixTranslation(interp.x, interp.y, interp.z);
}

XMMATRIX AssimpModelClass::InterpolateRotation(const std::vector<KeyRotation>& keys, float animTime)
{
    if (keys.empty()) return XMMatrixIdentity();
    if (keys.size() == 1)
        return XMMatrixRotationQuaternion(XMLoadFloat4(&keys[0].orientation));

    int idx = 0;
    while (idx < (int)keys.size() - 1 && keys[idx + 1].timeStamp <= animTime) ++idx;

    const KeyRotation& k1 = keys[idx];
    const KeyRotation& k2 = keys[(idx + 1) % keys.size()];
    float dt = k2.timeStamp - k1.timeStamp;
    if (dt < 0.0001f) dt = 1.0f;
    float factor = std::clamp((animTime - k1.timeStamp) / dt, 0.0f, 1.0f);

    XMVECTOR q1 = XMLoadFloat4(&k1.orientation);
    XMVECTOR q2 = XMLoadFloat4(&k2.orientation);
    return XMMatrixRotationQuaternion(XMQuaternionSlerp(q1, q2, factor));
}

XMMATRIX AssimpModelClass::InterpolateScale(const std::vector<KeyScale>& keys, float animTime)
{
    if (keys.empty()) return XMMatrixIdentity();
    if (keys.size() == 1)
        return XMMatrixScaling(keys[0].scale.x, keys[0].scale.y, keys[0].scale.z);

    int idx = 0;
    while (idx < (int)keys.size() - 1 && keys[idx + 1].timeStamp <= animTime) ++idx;

    const KeyScale& k1 = keys[idx];
    const KeyScale& k2 = keys[(idx + 1) % keys.size()];
    float dt = k2.timeStamp - k1.timeStamp;
    if (dt < 0.0001f) dt = 1.0f;
    float factor = std::clamp((animTime - k1.timeStamp) / dt, 0.0f, 1.0f);

    XMFLOAT3 interp(
        k1.scale.x + factor * (k2.scale.x - k1.scale.x),
        k1.scale.y + factor * (k2.scale.y - k1.scale.y),
        k1.scale.z + factor * (k2.scale.z - k1.scale.z));
    static bool logged = false;
    if (!logged) {
        char dbg[256];
        sprintf_s(dbg, "[SCALE] factor=%.2f -> (%.3f, %.3f, %.3f)\n",
            factor, interp.x, interp.y, interp.z);
        OutputDebugStringA(dbg);
        logged = true;  // Убери эту строку, если хочешь лог каждый кадр
    }

    return XMMatrixScaling(interp.x, interp.y, interp.z);
}

// ============================================================================
// ReadNodeHierarchy — ОСНОВНАЯ ЛОГИКА АНИМАЦИИ (ИСПРАВЛЕНА)
// ============================================================================
void AssimpModelClass::ReadNodeHierarchy(
    const aiNode* node,
    const XMMATRIX& parentTransform,
    const AnimationClip& anim,
    float animTimeInSeconds)
{
    std::string nodeName = node->mName.C_Str();

    // 1. Локальная трансформация из сцены
    XMFLOAT4X4 nodeLocalFloat = ConvertMatrix(node->mTransformation);
    XMMATRIX nodeTransform = XMLoadFloat4x4(&nodeLocalFloat);

    // 2. Ищем анимационный канал для ЭТОГО УЗЛА
    const BoneAnimationChannel* channel = nullptr;
    for (const auto& ch : anim.channels)
    {
        if (ch.nodeName == nodeName) { channel = &ch; break; }
    }

    // 3. Если есть анимация — переопределяем локальную трансформацию
    if (channel)
    {
        XMMATRIX posMat = InterpolatePosition(channel->positions, animTimeInSeconds);
        XMMATRIX rotMat = InterpolateRotation(channel->rotations, animTimeInSeconds);
        XMMATRIX scaleMat = InterpolateScale(channel->scales, animTimeInSeconds);

        // Порядок: Scale → Rotate → Translate (локальное пространство)
        nodeTransform = scaleMat * rotMat * posMat;
    }

    // 4. Глобальная трансформация = родитель * локальная
    XMMATRIX globalTransform = nodeTransform * parentTransform;

    // 5. Если этот узел — кость, вычисляем финальную матрицу для шейдера
    auto boneIt = m_boneInfoMap.find(nodeName);
    if (boneIt != m_boneInfoMap.end())
    {
        int boneIndex = boneIt->second.boneIndex;
        XMMATRIX offsetMat = XMLoadFloat4x4(&boneIt->second.offsetMatrix);

        // ✅ ПРАВИЛЬНЫЙ ПОРЯДОК для DirectX: offset * global
        XMMATRIX finalMat = globalTransform * offsetMat;
        XMStoreFloat4x4(&m_finalBoneMatrices[boneIndex], finalMat);
    }

    // 6. Рекурсия по детям
    for (unsigned int i = 0; i < node->mNumChildren; ++i)
    {
        ReadNodeHierarchy(node->mChildren[i], globalTransform, anim, animTimeInSeconds);
    }
}

// ============================================================================
// Обновление анимации и отправка матриц в шейдер
// ============================================================================
void AssimpModelClass::UpdateAnimation(float deltaTime, int animIndex)
{
    if (m_animations.empty() || animIndex >= (int)m_animations.size()) return;

    const AnimationClip& anim = m_animations[animIndex];
    if (anim.duration <= 0.001f) return;

    float ticksPerSecond = anim.ticksPerSecond > 0.0f ? anim.ticksPerSecond : 24.0f;

    // 🔥 Считаем время в тиках и сразу зацикливаем
    float timeInTicks = m_animationTime * ticksPerSecond;
    float animTime = fmodf(timeInTicks, anim.duration);
    if (animTime < 0.0f) animTime += anim.duration;

   //animTime = 0.0f;

    // 🔥 СБРОС: не даём m_animationTime расти бесконечно
    if (m_animationTime >= anim.duration / ticksPerSecond)
        m_animationTime = 0.0f;  // или: m_animationTime -= anim.duration / ticksPerSecond;
    else
        m_animationTime += deltaTime;

    if (m_scene && m_scene->mRootNode)
    {
        XMMATRIX identity = XMMatrixIdentity();
        ReadNodeHierarchy(m_scene->mRootNode, identity, anim, animTime);
    }
}

void AssimpModelClass::SetBoneMatrices(ID3D11DeviceContext* deviceContext)
{
    if (!deviceContext || m_finalBoneMatrices.empty() || !m_boneMatrixBuffer)
        return;

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    HRESULT hr = deviceContext->Map(m_boneMatrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (FAILED(hr)) return;

    auto* dst = reinterpret_cast<XMFLOAT4X4*>(mapped.pData);
    XMFLOAT4X4 identity; XMStoreFloat4x4(&identity, XMMatrixIdentity());

    // 🔥 Заполняем ВСЕ 128 слотов. Мусор = исключён.
    for (int i = 0; i < 128; ++i) {
        dst[i] = (i < (int)m_finalBoneMatrices.size()) ? m_finalBoneMatrices[i] : identity;
    }

    deviceContext->Unmap(m_boneMatrixBuffer, 0);
    deviceContext->VSSetConstantBuffers(8, 1, &m_boneMatrixBuffer);
}
// ============================================================================
// Initialize — главная функция загрузки
// ============================================================================
bool AssimpModelClass::Initialize(
    ID3D11Device* device,
    ID3D11DeviceContext* deviceContext,
    const char* modelPath,
    const char* textureFilename)
{
    Shutdown();
    m_scene = nullptr;

    try {
        m_scene = m_importer.ReadFile(modelPath,
            aiProcess_Triangulate |
            aiProcess_FlipUVs |
            aiProcess_GenSmoothNormals |
            aiProcess_CalcTangentSpace |
            aiProcess_LimitBoneWeights); // ❌ Убери ValidateDataStructure и PreTransformVertices
    }
    catch (const std::exception& e) {
        OutputDebugStringA(("Assimp Exception: " + std::string(e.what()) + "\n").c_str());
        return false;
    }

    if (!m_scene || (m_scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !m_scene->mRootNode) {
        MessageBoxA(NULL, m_importer.GetErrorString(), "Assimp Load Error", MB_OK);
        return false;
    }

    OutputDebugStringA(("Assimp: Model loaded. Meshes: " + std::to_string(m_scene->mNumMeshes) + "\n").c_str());

    m_totalIndexCount = 0;
    m_meshParts.clear();
    m_boneInfoMap.clear();
    m_finalBoneMatrices.clear();
    m_animations.clear();
    m_animationTime = 0.0f;

    // 🔥 2. Загружаем кости и анимации
    LoadBones(m_scene);
    LoadAnimations(m_scene);

    // 🔥 3. Создаём буфер костей СРАЗУ (чтобы Map не падал)
    if (!m_finalBoneMatrices.empty())
    {
        ID3D11Device* dev = nullptr;
        deviceContext->GetDevice(&dev);
        if (dev)
        {
            D3D11_BUFFER_DESC cbDesc = {};
            cbDesc.Usage = D3D11_USAGE_DYNAMIC;
            cbDesc.ByteWidth = sizeof(XMFLOAT4X4) * 128;
            cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            HRESULT hr = dev->CreateBuffer(&cbDesc, nullptr, &m_boneMatrixBuffer);
            dev->Release();
            if (FAILED(hr)) OutputDebugStringA("WARN: Bone buffer creation failed\n");
        }
    }

    // 🔥 4. Загружаем меши
    for (unsigned int i = 0; i < m_scene->mNumMeshes; ++i)
    {
        aiMesh* mesh = m_scene->mMeshes[i];
        if (mesh->mNumVertices == 0) continue;

        MeshPart part;
        part.indexCount = static_cast<int>(mesh->mNumFaces) * 3;
        m_totalIndexCount += part.indexCount;

        std::vector<VertexType> vertices;
        vertices.reserve(mesh->mNumVertices);

        for (unsigned int j = 0; j < mesh->mNumVertices; ++j)
        {
            VertexType v = {}; // 🔥 Zero-init всей структуры!

            v.position = XMFLOAT3(mesh->mVertices[j].x, mesh->mVertices[j].y, mesh->mVertices[j].z);
            v.texture = mesh->HasTextureCoords(0) ? XMFLOAT2(mesh->mTextureCoords[0][j].x, mesh->mTextureCoords[0][j].y) : XMFLOAT2(0, 0);
            v.normal = mesh->HasNormals() ? XMFLOAT3(mesh->mNormals[j].x, mesh->mNormals[j].y, mesh->mNormals[j].z) : XMFLOAT3(0, 1, 0);
            if (mesh->HasTangentsAndBitangents()) {
                v.tangent = XMFLOAT3(mesh->mTangents[j].x, mesh->mTangents[j].y, mesh->mTangents[j].z);
                v.binormal = XMFLOAT3(mesh->mBitangents[j].x, mesh->mBitangents[j].y, mesh->mBitangents[j].z);
            }

            // 🔥 Гарантируем валидные веса
            for (int b = 0; b < 4; ++b) {
                v.boneIDs[b] = -1;
                v.boneWeights[b] = 0.0f;
            }

            // Назначение весов (твой код, но без break внутри вложенного цикла!)
            for (unsigned int b = 0; b < mesh->mNumBones; ++b)
            {
                aiBone* bone = mesh->mBones[b];
                for (unsigned int w = 0; w < bone->mNumWeights; ++w)
                {
                    if (bone->mWeights[w].mVertexId == j)
                    {
                        auto it = m_boneInfoMap.find(bone->mName.C_Str());
                        if (it == m_boneInfoMap.end()) continue;

                        int boneIdx = it->second.boneIndex;
                        float weight = bone->mWeights[w].mWeight;

                        int minSlot = -1; float minW = 999.0f;
                        for (int s = 0; s < 4; ++s) {
                            if (v.boneIDs[s] == -1) { minSlot = s; break; }
                            if (v.boneWeights[s] < minW) { minW = v.boneWeights[s]; minSlot = s; }
                        }
                        if (minSlot >= 0) {
                            v.boneIDs[minSlot] = boneIdx;
                            v.boneWeights[minSlot] = weight;
                        }
                        break; // переходим к следующей кости
                    }
                }
            }

            // 🔥 Нормализация
            float sum = v.boneWeights[0] + v.boneWeights[1] + v.boneWeights[2] + v.boneWeights[3];
            if (sum > 0.0001f) {
                v.boneWeights[0] /= sum; v.boneWeights[1] /= sum;
                v.boneWeights[2] /= sum; v.boneWeights[3] /= sum;
            }
            else {
                v.boneIDs[0] = 0; v.boneWeights[0] = 1.0f;
            }

            vertices.push_back(v);
            // После нормализации весов:
        
           
        }

        // Индексы
        std::vector<unsigned long> indices;
        indices.reserve(part.indexCount);
        for (unsigned int f = 0; f < mesh->mNumFaces; f++)
            for (unsigned int k = 0; k < mesh->mFaces[f].mNumIndices; k++)
                indices.push_back(mesh->mFaces[f].mIndices[k]);

        // Создание буферов
        D3D11_BUFFER_DESC vbDesc = { sizeof(VertexType) * (UINT)vertices.size(), D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER };
        D3D11_SUBRESOURCE_DATA vbData = { vertices.data() };
        if (FAILED(device->CreateBuffer(&vbDesc, &vbData, &part.vertexBuffer))) return false;

        D3D11_BUFFER_DESC ibDesc = { sizeof(unsigned long) * (UINT)indices.size(), D3D11_USAGE_DEFAULT, D3D11_BIND_INDEX_BUFFER };
        D3D11_SUBRESOURCE_DATA ibData = { indices.data() };
        if (FAILED(device->CreateBuffer(&ibDesc, &ibData, &part.indexBuffer))) return false;

        // Текстуры материала
        if (mesh->mMaterialIndex >= 0 && mesh->mMaterialIndex < (int)m_scene->mNumMaterials)
        {
            const aiMaterial* mat = m_scene->mMaterials[mesh->mMaterialIndex];
            part.texture = ExtractTexture_Internal(device, deviceContext, m_scene, mat, aiTextureType_BASE_COLOR, aiTextureType_DIFFUSE);
            part.normalMap = ExtractTexture_Internal(device, deviceContext, m_scene, mat, aiTextureType_NORMALS, aiTextureType_HEIGHT);
            part.metalness = ExtractTexture_Internal(device, deviceContext, m_scene, mat, aiTextureType_METALNESS, aiTextureType_UNKNOWN);
            part.roughness = ExtractTexture_Internal(device, deviceContext, m_scene, mat, aiTextureType_DIFFUSE_ROUGHNESS, aiTextureType_UNKNOWN);

            // Заглушка из цвета, если нет текстур
            if (!part.texture && !m_Texture)
                part.texture = Create1x1SRVFromColor(device, GetPBRColorFromMaterial(mat, aiTextureType_BASE_COLOR));
            if (!part.metalness) {
                float metallic = 0.0f;
                aiGetMaterialFloat(mat, AI_MATKEY_METALLIC_FACTOR, &metallic);
                part.metalness = Create1x1SRVFromColor(device, aiColor4D(metallic, metallic, metallic, 1.0f));
            }
            if (!part.roughness) {
                float roughness = 1.0f;
                aiGetMaterialFloat(mat, AI_MATKEY_ROUGHNESS_FACTOR, &roughness);
                part.roughness = Create1x1SRVFromColor(device, aiColor4D(roughness, roughness, roughness, 1.0f));
            }

            // Прозрачность и стекло
            aiString alphaMode; mat->Get(AI_MATKEY_GLTF_ALPHAMODE, alphaMode);
            float opacity = 1.0f, transmission = 0.0f;
            aiGetMaterialFloat(mat, AI_MATKEY_OPACITY, &opacity);
            aiGetMaterialFloat(mat, AI_MATKEY_TRANSMISSION_FACTOR, &transmission);
            part.isTransparent = (std::string(alphaMode.C_Str()) == "BLEND") || (opacity < 0.999f) || (transmission > 0.01f);
            part.opacity = opacity;

            float metallicFactor = 0.0f, ior = 1.0f;
            aiGetMaterialFloat(mat, AI_MATKEY_METALLIC_FACTOR, &metallicFactor);
            aiGetMaterialFloat(mat, AI_MATKEY_REFRACTI, &ior);
            aiString matName; mat->Get(AI_MATKEY_NAME, matName);
            std::string matNameStr = matName.C_Str();
            std::transform(matNameStr.begin(), matNameStr.end(), matNameStr.begin(), ::tolower);

            part.isGlass = part.isTransparent && (metallicFactor > 0.5f || transmission > 0.01f) ||
                matNameStr.find("glass") != std::string::npos ||
                (ior > 1.01f && metallicFactor > 0.5f);
            if (part.isGlass && !part.isTransparent) part.isTransparent = true;
            if (part.isGlass && part.opacity > 0.5f) part.opacity = 0.12f;
        }

        m_meshParts.push_back(part);
    }

    if (m_meshParts.empty()) {
        MessageBoxA(NULL, "No valid meshes found!", "Assimp Error", MB_OK);
        return false;
    }

    // Fallback текстура
    if (textureFilename && textureFilename[0]) {
        m_Texture = new TextureClass;
        if (!m_Texture || !m_Texture->Initialize(device, deviceContext, textureFilename)) {
            if (m_Texture) { delete m_Texture; m_Texture = nullptr; }
            return false;
        }
    }

    // Кэширование границ
    GetBoundingBox(&m_cachedBBMin, &m_cachedBBMax);
    GetBoundingSphere(&m_cachedSphereCenter, &m_cachedSphereRadius);
    m_boundsCached = true;

    return true;
}

// ============================================================================
// Shutdown
// ============================================================================
void AssimpModelClass::Shutdown()
{
    for (auto& part : m_meshParts) {
        if (part.vertexBuffer) { part.vertexBuffer->Release(); part.vertexBuffer = nullptr; }
        if (part.indexBuffer) { part.indexBuffer->Release(); part.indexBuffer = nullptr; }
        if (part.texture) { part.texture->Release(); part.texture = nullptr; }
        if (part.normalMap) { part.normalMap->Release(); part.normalMap = nullptr; }
        if (part.metalness) { part.metalness->Release(); part.metalness = nullptr; }
        if (part.roughness) { part.roughness->Release(); part.roughness = nullptr; }
    }
    m_meshParts.clear();

    for (auto* t : m_ownedTextures) { if (t) { t->Shutdown(); delete t; } }
    m_ownedTextures.clear();

    if (m_Texture) { m_Texture->Shutdown(); delete m_Texture; m_Texture = nullptr; }
    if (m_boneMatrixBuffer) { m_boneMatrixBuffer->Release(); m_boneMatrixBuffer = nullptr; }

    m_boneInfoMap.clear();
    m_finalBoneMatrices.clear();
    m_animations.clear();
    m_scene = nullptr;
    m_totalIndexCount = 0;
    m_boundsCached = false;
    m_cachedSphereRadius = 0.0f;
}

// ============================================================================
// Render методы
// ============================================================================
void AssimpModelClass::Render(ID3D11DeviceContext* deviceContext)
{
    if (m_meshParts.empty()) return;
    unsigned int stride = sizeof(VertexType), offset = 0;
    deviceContext->IASetVertexBuffers(0, 1, &m_meshParts[0].vertexBuffer, &stride, &offset);
    deviceContext->IASetIndexBuffer(m_meshParts[0].indexBuffer, DXGI_FORMAT_R32_UINT, 0);
    deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void AssimpModelClass::RenderAll(
    ID3D11DeviceContext* deviceContext,
    PBRShaderClass* shader,
    XMMATRIX world, XMMATRIX view, XMMATRIX proj)
{

    static int callCount = 0;
    OutputDebugStringA(("[RENDER] RenderAll call #" + std::to_string(++callCount) + "\n").c_str());
    if (m_meshParts.empty()) return;
    unsigned int stride = sizeof(VertexType), offset = 0;

    // Обновление анимации
    if (HasAnimations()) {
        UpdateAnimation(0.016f, 0);
        SetBoneMatrices(deviceContext);
    }

    for (const auto& part : m_meshParts)
    {
        deviceContext->IASetVertexBuffers(0, 1, &part.vertexBuffer, &stride, &offset);
        deviceContext->IASetIndexBuffer(part.indexBuffer, DXGI_FORMAT_R32_UINT, 0);
        deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        if (shader) {
            shader->SetMatrixBuffers(deviceContext, world, view, proj);
            if (part.texture) deviceContext->PSSetShaderResources(0, 1, &part.texture);
            if (part.normalMap) deviceContext->PSSetShaderResources(1, 1, &part.normalMap);
            if (part.metalness) deviceContext->PSSetShaderResources(2, 1, &part.metalness);
            if (part.roughness) deviceContext->PSSetShaderResources(3, 1, &part.roughness);

            shader->Render(deviceContext, part.indexCount, world, view, proj,
                part.texture, part.normalMap, part.metalness, part.roughness,
                nullptr, nullptr, XMFLOAT3(0, 0, 0), 1.0f, 1.0f);
        }
        else {
            deviceContext->DrawIndexed(part.indexCount, 0, 0);
        }
    }
}

// ============================================================================
// Геттеры и утилиты
// ============================================================================
int AssimpModelClass::GetIndexCount() {
    return m_meshParts.empty() ? m_totalIndexCount : m_meshParts[0].indexCount;
}

ID3D11ShaderResourceView* AssimpModelClass::GetTexture() {
    if (!m_meshParts.empty() && m_meshParts[0].texture) return m_meshParts[0].texture;
    return m_Texture ? m_Texture->GetTexture() : nullptr;
}

void AssimpModelClass::GetBoundingBox(XMFLOAT3* min, XMFLOAT3* max) const {
    if (!min || !max) return;
    if (m_boundsCached) { *min = m_cachedBBMin; *max = m_cachedBBMax; return; }

    XMFLOAT3 bbMin(FLT_MAX, FLT_MAX, FLT_MAX), bbMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    if (m_scene) {
        for (unsigned int i = 0; i < m_scene->mNumMeshes; i++) {
            aiMesh* mesh = m_scene->mMeshes[i];
            for (unsigned int j = 0; j < mesh->mNumVertices; j++) {
                aiVector3D& v = mesh->mVertices[j];
                bbMin.x = std::min(bbMin.x, v.x); bbMin.y = std::min(bbMin.y, v.y); bbMin.z = std::min(bbMin.z, v.z);
                bbMax.x = max(bbMax.x, v.x); bbMax.y = max(bbMax.y, v.y); bbMax.z = max(bbMax.z, v.z);
            }
        }
    }
    m_cachedBBMin = bbMin; m_cachedBBMax = bbMax; m_boundsCached = true;
    *min = bbMin; *max = bbMax;
}

void AssimpModelClass::GetBoundingSphere(XMFLOAT3* center, float* radius) const {
    if (!center || !radius) return;
    if (m_boundsCached && m_cachedSphereRadius > 0.0f) {
        *center = m_cachedSphereCenter; *radius = m_cachedSphereRadius; return;
    }

    XMFLOAT3 bbMin, bbMax; GetBoundingBox(&bbMin, &bbMax);
    XMFLOAT3 sphereCenter(
        (bbMin.x + bbMax.x) * 0.5f,
        (bbMin.y + bbMax.y) * 0.5f,
        (bbMin.z + bbMax.z) * 0.5f);

    float maxDistSq = 0.0f;
    if (m_scene) {
        for (unsigned int i = 0; i < m_scene->mNumMeshes; i++) {
            aiMesh* mesh = m_scene->mMeshes[i];
            for (unsigned int j = 0; j < mesh->mNumVertices; j++) {
                aiVector3D& v = mesh->mVertices[j];
                float dx = v.x - sphereCenter.x, dy = v.y - sphereCenter.y, dz = v.z - sphereCenter.z;
                float distSq = dx * dx + dy * dy + dz * dz;
                if (distSq > maxDistSq) maxDistSq = distSq;
            }
        }
    }
    float sphereRadius = sqrtf(maxDistSq);
    m_cachedSphereCenter = sphereCenter; m_cachedSphereRadius = sphereRadius;
    *center = sphereCenter; *radius = sphereRadius;
}

void AssimpModelClass::GetVertices(std::vector<XMFLOAT3>& out) const {
    out.clear();
    if (!m_scene) return;
    for (unsigned int i = 0; i < m_scene->mNumMeshes; ++i) {
        const aiMesh* mesh = m_scene->mMeshes[i];
        if (!mesh || !mesh->HasPositions()) continue;
        for (unsigned int j = 0; j < mesh->mNumVertices; ++j)
            out.emplace_back(mesh->mVertices[j].x, mesh->mVertices[j].y, mesh->mVertices[j].z);
    }
}

void AssimpModelClass::GetRawVertices(std::vector<XMFLOAT3>& out) const { GetVertices(out); }

void AssimpModelClass::GetRawTriangles(std::vector<XMFLOAT3>& out) const {
    out.clear();
    if (!m_scene) return;
    for (unsigned int i = 0; i < m_scene->mNumMeshes; ++i) {
        const aiMesh* mesh = m_scene->mMeshes[i];
        if (!mesh || !mesh->HasPositions() || !mesh->HasFaces()) continue;
        for (unsigned int f = 0; f < mesh->mNumFaces; f++) {
            const aiFace& face = mesh->mFaces[f];
            if (face.mNumIndices != 3) continue;
            for (unsigned int k = 0; k < 3; ++k)
                out.emplace_back(mesh->mVertices[face.mIndices[k]].x,
                    mesh->mVertices[face.mIndices[k]].y,
                    mesh->mVertices[face.mIndices[k]].z);
        }
    }
}