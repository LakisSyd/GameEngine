// ============================================================================
// assimpmodelclass.cpp — Полная реализация с расширенной отладкой
// ============================================================================

#include "assimpmodelclass.h"
#include <iostream>
#include <Windows.h>
#include <fstream>
#include <GltfMaterial.h>
#include <algorithm>
#include "PBRShaderClass.h"
#include <DirectXMath.h>
#include <functional>
#include <iomanip>
using namespace DirectX;

// ============================================================================
// Глобальные настройки отладки
// ============================================================================
#define DEBUG_VERBOSE 1          // Подробный вывод
#define DEBUG_BONE_MATCHING 1    // Отладка сопоставления костей
#define DEBUG_TRANSFORMS 1       // Отладка трансформаций
#define DEBUG_ANIMATION_KEYS 1   // Отладка ключей анимации

static std::string SafeReadAiString(const aiString& aiStr) {
    if (aiStr.length == 0) return "";
    // Читаем ровно aiStr.length байт, игнорируя отсутствие \0
    return std::string(aiStr.C_Str(), aiStr.length);
}


static std::string NormalizeBoneName(std::string name) {
    // 1. Убираем суффиксы масштабной компенсации
    size_t pos = name.find("_scaleCompensation");
    if (pos != std::string::npos) name = name.substr(0, pos);

    // 2. Убираем суффиксы вида _01, _02, _14 и т.д.
    pos = name.find_last_of('_');
    if (pos != std::string::npos) {
        std::string suffix = name.substr(pos + 1);
        bool isNumber = true;
        for (char c : suffix) if (!std::isdigit(static_cast<unsigned char>(c))) { isNumber = false; break; }
        if (isNumber && !suffix.empty()) name = name.substr(0, pos);
    }
    return name;
}

// ============================================================================
// Вспомогательные функции отладки
// ============================================================================
static void PrintMatrix(const XMFLOAT4X4& matrix, const std::string& label) {
#if DEBUG_VERBOSE
    char dbg[512];
    sprintf_s(dbg, "%s:\n"
        "[%.4f, %.4f, %.4f, %.4f]\n"
        "[%.4f, %.4f, %.4f, %.4f]\n"
        "[%.4f, %.4f, %.4f, %.4f]\n"
        "[%.4f, %.4f, %.4f, %.4f]\n",
        label.c_str(),
        matrix._11, matrix._12, matrix._13, matrix._14,
        matrix._21, matrix._22, matrix._23, matrix._24,
        matrix._31, matrix._32, matrix._33, matrix._34,
        matrix._41, matrix._42, matrix._43, matrix._44);
    OutputDebugStringA(dbg);
#endif
}

static void PrintVector3(const XMFLOAT3& vec, const std::string& label) {
#if DEBUG_VERBOSE
    char dbg[256];
    sprintf_s(dbg, "%s: (%.4f, %.4f, %.4f)\n", label.c_str(), vec.x, vec.y, vec.z);
    OutputDebugStringA(dbg);
#endif
}

static void PrintQuaternion(const XMFLOAT4& quat, const std::string& label) {
#if DEBUG_VERBOSE
    char dbg[256];
    sprintf_s(dbg, "%s: (%.4f, %.4f, %.4f, %.4f) [w=%.4f]\n",
        label.c_str(), quat.x, quat.y, quat.z, quat.w);
    OutputDebugStringA(dbg);
#endif
}

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

#if DEBUG_VERBOSE
    OutputDebugStringA("[AssimpModelClass] Constructor called\n");
#endif
}

AssimpModelClass::~AssimpModelClass()
{
#if DEBUG_VERBOSE
    OutputDebugStringA("[AssimpModelClass] Destructor called\n");
#endif
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

XMFLOAT4X4 AssimpModelClass::ConvertMatrix(const aiMatrix4x4& from) const
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
// Загрузка костей — ИСПРАВЛЕННАЯ ВЕРСИЯ с отладкой
// ============================================================================
void AssimpModelClass::LoadBones(const aiScene* scene)
{
    m_boneInfoMap.clear();
    m_finalBoneMatrices.clear();

#if DEBUG_VERBOSE
    OutputDebugStringA("\n========== LOADING BONES ==========\n");
    OutputDebugStringA(("Total meshes: " + std::to_string(scene->mNumMeshes) + "\n").c_str());
#endif

    int boneCounter = 0;
    for (unsigned int m = 0; m < scene->mNumMeshes; ++m)
    {
        aiMesh* mesh = scene->mMeshes[m];
#if DEBUG_VERBOSE
        OutputDebugStringA(("Mesh " + std::to_string(m) + ": " + (mesh->mName.C_Str()[0] ? mesh->mName.C_Str() : "unnamed") + "\n").c_str());
        OutputDebugStringA(("  Vertices: " + std::to_string(mesh->mNumVertices) + ", Bones: " + std::to_string(mesh->mNumBones) + "\n").c_str());
#endif

        for (unsigned int b = 0; b < mesh->mNumBones; ++b)
        {
            aiBone* bone = mesh->mBones[b];
            std::string boneName = NormalizeBoneName(bone->mName.C_Str());
            std::string originalName = bone->mName.C_Str();

#if DEBUG_BONE_MATCHING
            char dbg[512];
            sprintf_s(dbg, "  Bone[%d]: original='%s' -> normalized='%s'\n",
                b, originalName.c_str(), boneName.c_str());
            OutputDebugStringA(dbg);
#endif

            if (m_boneInfoMap.find(boneName) == m_boneInfoMap.end())
            {
                BoneInfo info;
                info.name = boneName;
                info.offsetMatrix = ConvertMatrix(bone->mOffsetMatrix);

#if DEBUG_BONE_MATCHING
                PrintMatrix(info.offsetMatrix, "    OffsetMatrix");
#endif

                info.boneIndex = boneCounter++;
                m_boneInfoMap[boneName] = info;

#if DEBUG_VERBOSE
                OutputDebugStringA(("    [NEW] Added bone: " + boneName + " (index: " + std::to_string(info.boneIndex) + ")\n").c_str());
#endif
            }
        }
    }

    // Выделяем память и инициализируем
    m_finalBoneMatrices.resize(m_boneInfoMap.size());
    XMFLOAT4X4 identity; XMStoreFloat4x4(&identity, XMMatrixIdentity());
    for (auto& mat : m_finalBoneMatrices) mat = identity;

#if DEBUG_VERBOSE
    OutputDebugStringA(("Loaded " + std::to_string(boneCounter) + " unique bones\n").c_str());
    OutputDebugStringA("========== END BONES ==========\n\n");
#endif
}

// ============================================================================
// Загрузка анимаций — ИСПРАВЛЕННАЯ ВЕРСИЯ с отладкой
// ============================================================================
void AssimpModelClass::LoadAnimations(const aiScene* scene)
{
    m_animations.clear();

#if DEBUG_VERBOSE
    OutputDebugStringA("\n========== LOADING ANIMATIONS ==========\n");
    OutputDebugStringA(("Total animations: " + std::to_string(scene->mNumAnimations) + "\n").c_str());
#endif

    for (unsigned int i = 0; i < scene->mNumAnimations; ++i)
    {
        aiAnimation* anim = scene->mAnimations[i];
        AnimationClip clip;
        clip.name = anim->mName.C_Str();
        clip.duration = (float)anim->mDuration;
        clip.ticksPerSecond = (anim->mTicksPerSecond == 0.0) ? 24.0f : (float)anim->mTicksPerSecond;

#if DEBUG_VERBOSE
        OutputDebugStringA(("\nAnimation[" + std::to_string(i) + "]: " + clip.name + "\n").c_str());
        OutputDebugStringA(("  Duration: " + std::to_string(clip.duration) + " ticks\n").c_str());
        OutputDebugStringA(("  TicksPerSecond: " + std::to_string(clip.ticksPerSecond) + "\n").c_str());
        OutputDebugStringA(("  Channels: " + std::to_string(anim->mNumChannels) + "\n").c_str());
#endif

        for (unsigned int c = 0; c < anim->mNumChannels; ++c)
        {
            aiNodeAnim* channel = anim->mChannels[c];
            BoneAnimationChannel boneChannel;

            // КЛЮЧЕВОЕ: используем mNodeName — имя узла в иерархии
            boneChannel.nodeName = NormalizeBoneName(channel->mNodeName.C_Str());
            std::string originalNodeName = channel->mNodeName.C_Str();

#if DEBUG_ANIMATION_KEYS
            char dbg[512];
            sprintf_s(dbg, "  Channel[%d]: original='%s' -> normalized='%s'\n",
                c, originalNodeName.c_str(), boneChannel.nodeName.c_str());
            OutputDebugStringA(dbg);
            OutputDebugStringA(("    Pos keys: " + std::to_string(channel->mNumPositionKeys) + "\n").c_str());
            OutputDebugStringA(("    Rot keys: " + std::to_string(channel->mNumRotationKeys) + "\n").c_str());
            OutputDebugStringA(("    Scale keys: " + std::to_string(channel->mNumScalingKeys) + "\n").c_str());
#endif

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

#if DEBUG_ANIMATION_KEYS && k < 3
                PrintVector3(kp.position, "      Pos key[" + std::to_string(k) + "]");
#endif
            }

            // Вращения
            for (unsigned int k = 0; k < channel->mNumRotationKeys; ++k)
            {
                KeyRotation kr;
                aiQuaternion q = channel->mRotationKeys[k].mValue;

                kr.orientation = XMFLOAT4(
                    channel->mRotationKeys[k].mValue.x,
                    channel->mRotationKeys[k].mValue.y,
                    channel->mRotationKeys[k].mValue.z,
                    channel->mRotationKeys[k].mValue.w);
                kr.timeStamp = (float)channel->mRotationKeys[k].mTime;
                boneChannel.rotations.push_back(kr);

#if DEBUG_ANIMATION_KEYS && k < 3
                PrintQuaternion(kr.orientation, "      Rot key[" + std::to_string(k) + "]");
#endif
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

#if DEBUG_ANIMATION_KEYS && k < 3
                PrintVector3(ks.scale, "      Scale key[" + std::to_string(k) + "]");
#endif
            }

            clip.channels.push_back(boneChannel);
        }
        m_animations.push_back(clip);
    }

#if DEBUG_VERBOSE
    OutputDebugStringA("========== END ANIMATIONS ==========\n\n");
#endif
}

// ============================================================================
// Интерполяция трансформаций с отладкой
// ============================================================================
XMMATRIX AssimpModelClass::InterpolatePosition(const std::vector<KeyPosition>& keys, float animTime) const
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

#if DEBUG_TRANSFORMS && false // Включите для отладки интерполяции
    char dbg[256];
    sprintf_s(dbg, "[InterpPos] factor=%.3f, t1=%.3f, t2=%.3f, animTime=%.3f\n",
        factor, k1.timeStamp, k2.timeStamp, animTime);
    OutputDebugStringA(dbg);
#endif

    XMFLOAT3 interp(
        k1.position.x + factor * (k2.position.x - k1.position.x),
        k1.position.y + factor * (k2.position.y - k1.position.y),
        k1.position.z + factor * (k2.position.z - k1.position.z));

    return XMMatrixTranslation(interp.x, interp.y, interp.z);
}

XMMATRIX AssimpModelClass::InterpolateRotation(const std::vector<KeyRotation>& keys, float animTime) const
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

XMMATRIX AssimpModelClass::InterpolateScale(const std::vector<KeyScale>& keys, float animTime) const
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

#if DEBUG_TRANSFORMS && false
    char dbg[256];
    sprintf_s(dbg, "[InterpScale] factor=%.3f, scale1=(%.3f,%.3f,%.3f), scale2=(%.3f,%.3f,%.3f)\n",
        factor, k1.scale.x, k1.scale.y, k1.scale.z, k2.scale.x, k2.scale.y, k2.scale.z);
    OutputDebugStringA(dbg);
#endif

    XMFLOAT3 interp(
        k1.scale.x + factor * (k2.scale.x - k1.scale.x),
        k1.scale.y + factor * (k2.scale.y - k1.scale.y),
        k1.scale.z + factor * (k2.scale.z - k1.scale.z));

    return XMMatrixScaling(interp.x, interp.y, interp.z);
}

// ============================================================================
// ReadNodeHierarchy — ОСНОВНАЯ ЛОГИКА АНИМАЦИИ с отладкой
// ============================================================================
void AssimpModelClass::ReadNodeHierarchy(
    const aiNode* node,
    const XMMATRIX& parentTransform,
    const AnimationClip& anim,
    float animTimeInSeconds)
{
    std::string nodeName = NormalizeBoneName(node->mName.C_Str());
    std::string originalNodeName = node->mName.C_Str();

#if DEBUG_TRANSFORMS
    static int depth = 0;
    std::string indent(depth * 2, ' ');
    char dbg[512];
    sprintf_s(dbg, "%s[Node] '%s' (orig: '%s'), depth=%d\n",
        indent.c_str(), nodeName.c_str(), originalNodeName.c_str(), depth);
    OutputDebugStringA(dbg);
    depth++;
#endif

    // 1. Локальная трансформация из сцены
    XMFLOAT4X4 nodeLocalFloat = ConvertMatrix(node->mTransformation);
    XMMATRIX nodeTransform = XMLoadFloat4x4(&nodeLocalFloat);

#if DEBUG_TRANSFORMS && false
    PrintMatrix(nodeLocalFloat, indent + "  NodeLocal");
#endif

    // 2. Ищем анимационный канал для ЭТОГО УЗЛА
    const BoneAnimationChannel* channel = nullptr;
    for (const auto& ch : anim.channels)
    {
        if (ch.nodeName == nodeName) {
            channel = &ch;
#if DEBUG_TRANSFORMS
            OutputDebugStringA((indent + "  ✓ Found animation channel\n").c_str());
#endif
            break;
        }
    }

    // 3. Если есть анимация — переопределяем локальную трансформацию
    if (channel)
    {
        XMMATRIX posMat = InterpolatePosition(channel->positions, animTimeInSeconds);
        XMMATRIX rotMat = InterpolateRotation(channel->rotations, animTimeInSeconds);
        XMMATRIX scaleMat = InterpolateScale(channel->scales, animTimeInSeconds);

        // Порядок: Scale → Rotate → Translate (локальное пространство)
        nodeTransform = scaleMat * rotMat * posMat;

#if DEBUG_TRANSFORMS && false
        PrintMatrix(nodeTransform, indent + "  AnimatedLocal");
#endif
    }

    // 4. Глобальная трансформация = родитель * локальная
    XMMATRIX globalTransform = nodeTransform * parentTransform;

#if DEBUG_TRANSFORMS && false
    PrintMatrix(globalTransform, indent + "  Global");
#endif

    // 5. Если этот узел — кость, вычисляем финальную матрицу для шейдера
    auto boneIt = m_boneInfoMap.find(nodeName);
    if (boneIt != m_boneInfoMap.end())
    {
        int boneIndex = boneIt->second.boneIndex;
        XMMATRIX offsetMat = XMLoadFloat4x4(&boneIt->second.offsetMatrix);

        // ✅ ПРАВИЛЬНЫЙ ПОРЯДОК для DirectX: offset * global
        XMMATRIX finalMat = globalTransform * offsetMat;
        XMStoreFloat4x4(&m_finalBoneMatrices[boneIndex], finalMat);

#if DEBUG_TRANSFORMS
        sprintf_s(dbg, "%s  [Bone] '%s' (idx=%d) -> final matrix set\n",
            indent.c_str(), nodeName.c_str(), boneIndex);
        OutputDebugStringA(dbg);

        // Выводим первую матрицу для отладки
        static bool firstBonePrinted = false;
        if (!firstBonePrinted && boneIndex == 0) {
            XMFLOAT4X4 temp; XMStoreFloat4x4(&temp, nodeTransform);
            PrintMatrix(temp, indent + "  NodeLocal");
            firstBonePrinted = true;
        }
#endif
    }

    // 6. Рекурсия по детям
    for (unsigned int i = 0; i < node->mNumChildren; ++i)
    {
        ReadNodeHierarchy(node->mChildren[i], globalTransform, anim, animTimeInSeconds);
    }

#if DEBUG_TRANSFORMS
    depth--;
#endif
}

// ============================================================================
// Initialize — главная функция загрузки с отладкой
// ============================================================================
bool AssimpModelClass::Initialize(
    ID3D11Device* device,
    ID3D11DeviceContext* deviceContext,
    const char* modelPath,
    const char* textureFilename)
{
#if DEBUG_VERBOSE
    OutputDebugStringA(("\n========== INITIALIZING MODEL: " + std::string(modelPath) + " ==========\n").c_str());
#endif

    Shutdown();
    m_scene = nullptr;

    try {
        m_scene = m_importer.ReadFile(modelPath,
            aiProcess_Triangulate |
            aiProcess_ConvertToLeftHanded |
            aiProcess_CalcTangentSpace |
            aiProcess_GenSmoothNormals |
            aiProcess_LimitBoneWeights 
            
            );
    }
    catch (const std::exception& e) {
        OutputDebugStringA(("Assimp Exception: " + std::string(e.what()) + "\n").c_str());
        return false;
    }

    if (!m_scene || (m_scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !m_scene->mRootNode) {
        MessageBoxA(NULL, m_importer.GetErrorString(), "Assimp Load Error", MB_OK);
        return false;
    }

#if DEBUG_VERBOSE
    OutputDebugStringA(("Assimp: Model loaded. Meshes: " + std::to_string(m_scene->mNumMeshes) +
        ", Materials: " + std::to_string(m_scene->mNumMaterials) +
        ", Animations: " + std::to_string(m_scene->mNumAnimations) + "\n").c_str());
#endif

    m_totalIndexCount = 0;
    m_meshParts.clear();
    m_boneInfoMap.clear();
    m_finalBoneMatrices.clear();
    m_animations.clear();
    m_animationTime = 0.0f;

    // Загружаем кости и анимации
    LoadBones(m_scene);
    LoadAnimations(m_scene);



    // Создаём буфер костей
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

    // Загружаем меши
    for (unsigned int i = 0; i < m_scene->mNumMeshes; ++i)
    {
        aiMesh* mesh = m_scene->mMeshes[i];
        if (mesh->mNumVertices == 0) continue;

#if DEBUG_VERBOSE
        OutputDebugStringA(("Processing mesh " + std::to_string(i) + ": " +
            (mesh->mName.C_Str()[0] ? mesh->mName.C_Str() : "unnamed") + "\n").c_str());
#endif

        MeshPart part;
        part.indexCount = static_cast<int>(mesh->mNumFaces) * 3;
        m_totalIndexCount += part.indexCount;

        std::vector<VertexType> vertices;
        vertices.reserve(mesh->mNumVertices);

        for (unsigned int j = 0; j < mesh->mNumVertices; ++j)
        {
            VertexType v = {};

            v.position = XMFLOAT3(mesh->mVertices[j].x, mesh->mVertices[j].y, mesh->mVertices[j].z);
            v.texture = mesh->HasTextureCoords(0) ? XMFLOAT2(mesh->mTextureCoords[0][j].x, mesh->mTextureCoords[0][j].y) : XMFLOAT2(0, 0);
            v.normal = mesh->HasNormals() ? XMFLOAT3(mesh->mNormals[j].x, mesh->mNormals[j].y, mesh->mNormals[j].z) : XMFLOAT3(0, 1, 0);
            if (mesh->HasTangentsAndBitangents()) {
                v.tangent = XMFLOAT3(mesh->mTangents[j].x, mesh->mTangents[j].y, mesh->mTangents[j].z);
                v.binormal = XMFLOAT3(mesh->mBitangents[j].x, mesh->mBitangents[j].y, mesh->mBitangents[j].z);
            }

            // Гарантируем валидные веса
            for (int b = 0; b < 4; ++b) {
                v.boneIDs[b] = -1;
                v.boneWeights[b] = 0.0f;
            }

            // Назначение весов
            for (unsigned int b = 0; b < mesh->mNumBones; ++b)
            {
                aiBone* bone = mesh->mBones[b];
                for (unsigned int w = 0; w < bone->mNumWeights; ++w)
                {
                    if (bone->mWeights[w].mVertexId == j)
                    {
                        auto it = m_boneInfoMap.find(NormalizeBoneName(bone->mName.C_Str()));
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
                        break;
                    }
                }
            }

            // Нормализация весов
            float sum = v.boneWeights[0] + v.boneWeights[1] + v.boneWeights[2] + v.boneWeights[3];
            if (sum > 0.0001f) {
                v.boneWeights[0] /= sum; v.boneWeights[1] /= sum;
                v.boneWeights[2] /= sum; v.boneWeights[3] /= sum;
            }
            else {
                v.boneIDs[0] = 0; v.boneWeights[0] = 1.0f;
            }
            if (mesh->mNumBones == 0) {
                // 🔥 Устанавливаем корневую кость для нодовой анимации
                v.boneIDs[0] = 0;      // вместо -1
                v.boneWeights[0] = 1.0f;
            }
            vertices.push_back(v);
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

#ifdef _DEBUG
    DebugCompareNames();
#endif

#if DEBUG_VERBOSE
    OutputDebugStringA("========== MODEL INITIALIZED SUCCESSFULLY ==========\n\n");
#endif
    return true;
}

void AssimpModelClass::DebugCompareNames()
{
    OutputDebugStringA("=== NAME COMPARISON DEBUG ===\n");

    // 1. Кости (из мешей)
    OutputDebugStringA("--- Bones (aiBone::mName) ---\n");
    for (const auto& [name, info] : m_boneInfoMap) {
        OutputDebugStringA(("  BONE: [" + name + "] (index: " + std::to_string(info.boneIndex) + ")\n").c_str());
    }

    // 2. Узлы иерархии (aiNode::mName)
    std::function<void(const aiNode*, int)> dumpNodes = [&](const aiNode* node, int depth) {
        std::string indent(depth * 2, ' ');
        std::string name = node->mName.C_Str();
        bool isBone = m_boneInfoMap.count(NormalizeBoneName(name)) > 0;
        OutputDebugStringA((indent + "NODE: [" + name + "]" + (isBone ? " ✓bone" : "") + "\n").c_str());
        for (unsigned i = 0; i < node->mNumChildren; ++i)
            dumpNodes(node->mChildren[i], depth + 1);
        };
    if (m_scene && m_scene->mRootNode) {
        OutputDebugStringA("--- Nodes (aiNode::mName) ---\n");
        dumpNodes(m_scene->mRootNode, 0);
    }

    // 3. Анимационные каналы (aiNodeAnim::mNodeName)
    OutputDebugStringA("--- Animation Channels (aiNodeAnim::mNodeName) ---\n");
    for (const auto& anim : m_animations) {
        OutputDebugStringA(("  Anim: " + anim.name + "\n").c_str());
        for (size_t c = 0; c < anim.channels.size() && c < 10; ++c) {
            OutputDebugStringA(("    CH[" + std::to_string(c) + "]: [" + anim.channels[c].nodeName + "]\n").c_str());
        }
    }
    OutputDebugStringA("=== END DEBUG ===\n");
}

// ============================================================================
// Shutdown
// ============================================================================
void AssimpModelClass::Shutdown()
{
#if DEBUG_VERBOSE
    OutputDebugStringA("[Shutdown] Cleaning up...\n");
#endif

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
    if (m_meshParts.empty()) return;
    unsigned int stride = sizeof(VertexType), offset = 0;

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

// ============================================================================
// Обновление анимации с компонентом — с расширенной отладкой
// ============================================================================
void AssimpModelClass::UpdateAnimation(AnimationComponent& animComp, float deltaTime)
{
    if (!animComp.isPlaying) return;
    if (m_animations.empty() || animComp.animationIndex < 0 || animComp.animationIndex >= (int)m_animations.size()) return;

    const AnimationClip& anim = m_animations[animComp.animationIndex];
    if (anim.duration <= 0.001f) return;

    float ticksPerSecond = (anim.ticksPerSecond > 0.0f) ? anim.ticksPerSecond : 24.0f;

    // 1. Накапливаем время
    animComp.currentTime += deltaTime;

    // 2. Зацикливание в секундах
    float animDurationSec = anim.duration / ticksPerSecond;
    if (animDurationSec > 0.0f) {
        animComp.currentTime = fmodf(animComp.currentTime, animDurationSec);
        if (animComp.currentTime < 0.0f) animComp.currentTime += animDurationSec;
    }
    else {
        animComp.currentTime = 0.0f;
    }

    // 3. Конвертируем в тики для интерполяции (уже зациклено!)
    float animTimeTicks = animComp.currentTime * ticksPerSecond;
    animTimeTicks = fmodf(animTimeTicks, anim.duration);
    if (animTimeTicks < 0.0f) animTimeTicks += anim.duration;

    // 4. 🔥 КРИТИЧЕСКОЕ: гарантируем, что буфер нужного размера
    size_t boneCount = m_boneInfoMap.size();
    if (animComp.finalBoneMatrices.size() != boneCount) {
        animComp.finalBoneMatrices.resize(boneCount);
    }

    // Инициализируем идентичными матрицами
    XMFLOAT4X4 identity; XMStoreFloat4x4(&identity, XMMatrixIdentity());
    for (size_t i = 0; i < boneCount; ++i) {
        animComp.finalBoneMatrices[i] = identity;
    }

    // 5. Вызываем вашу РАБОЧУЮ версию функции
    if (m_scene && m_scene->mRootNode) {
        ReadNodeHierarchyToComponent(m_scene->mRootNode, XMMatrixIdentity(), anim, animTimeTicks, animComp.finalBoneMatrices);
    }
}

void AssimpModelClass::SetBoneMatricesFromComponent(ID3D11DeviceContext* context, const AnimationComponent& animComp) {
    if (!context || animComp.finalBoneMatrices.empty() || !m_boneMatrixBuffer) return;

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    HRESULT hr = context->Map(m_boneMatrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (FAILED(hr)) return;

    auto* dst = reinterpret_cast<XMFLOAT4X4*>(mapped.pData);
    DirectX::XMMATRIX identity = DirectX::XMMatrixIdentity();

    // Заполняем ВСЕ 128 слотов
    for (int i = 0; i < 128; ++i) {
        dst[i] = (i < (int)animComp.finalBoneMatrices.size()) ? animComp.finalBoneMatrices[i]
            : *reinterpret_cast<XMFLOAT4X4*>(&identity);
    }

    context->Unmap(m_boneMatrixBuffer, 0);
    context->VSSetConstantBuffers(8, 1, &m_boneMatrixBuffer);

#if DEBUG_TRANSFORMS
    // Выводим первую матрицу для проверки
    if (!animComp.finalBoneMatrices.empty()) {
        PrintMatrix(animComp.finalBoneMatrices[0], "[SetBoneMatrices] First bone matrix");
    }
#endif
}

void AssimpModelClass::ReadNodeHierarchyToComponent(
    const aiNode* node,
    const DirectX::XMMATRIX& parentTransform,
    const AnimationClip& anim,
    float animTime,
    std::vector<DirectX::XMFLOAT4X4>& outMatrices) const
{
    if (outMatrices.empty()) {
        OutputDebugStringA("[ReadNodeHierarchy] ⚠️ outMatrices is EMPTY!\n");
        return;
    }

    // Безопасное чтение имени
    std::string rawName = (node->mName.length > 0) ? std::string(node->mName.C_Str(), node->mName.length) : "";
    std::string nodeName = NormalizeBoneName(rawName);

    // 🔥 ЛОГИРУЕМ ПЕРВЫЕ 20 УЗЛОВ для отладки
    static int debugCount = 0;
    if (debugCount < 20) {
        char dbg[512];
        sprintf_s(dbg, "[Node %d] Raw='%s' | Norm='%s' | Children=%u\n",
            debugCount, rawName.c_str(), nodeName.c_str(), node->mNumChildren);
        OutputDebugStringA(dbg);
        debugCount++;
    }

    // Загрузка локальной трансформации
    XMFLOAT4X4 nodeLocalFloat = ConvertMatrix(node->mTransformation);
    DirectX::XMMATRIX nodeTransform = DirectX::XMLoadFloat4x4(&nodeLocalFloat);

    // Поиск канала анимации
    const BoneAnimationChannel* channel = nullptr;
    for (const auto& ch : anim.channels) {
        if (ch.nodeName == nodeName) {
            channel = &ch;
            if (debugCount <= 20) OutputDebugStringA(("  ✓ Channel found: '" + ch.nodeName + "'\n").c_str());
            break;
        }
    }

    // Применяем анимацию
    if (channel) {
        XMMATRIX posMat = (channel->positions.size() >= 2) ? InterpolatePosition(channel->positions, animTime) :
            (channel->positions.empty() ? XMMatrixIdentity() : XMMatrixTranslation(channel->positions[0].position.x, channel->positions[0].position.y, channel->positions[0].position.z));
        XMMATRIX rotMat = (channel->rotations.size() >= 2) ? InterpolateRotation(channel->rotations, animTime) :
            (channel->rotations.empty() ? XMMatrixIdentity() : XMMatrixRotationQuaternion(XMLoadFloat4(&channel->rotations[0].orientation)));
        XMMATRIX scaleMat = (channel->scales.size() >= 2) ? InterpolateScale(channel->scales, animTime) :
            (channel->scales.empty() ? XMMatrixIdentity() : XMMatrixScaling(channel->scales[0].scale.x, channel->scales[0].scale.y, channel->scales[0].scale.z));

        nodeTransform = scaleMat * rotMat * posMat;
    }

    DirectX::XMMATRIX globalTransform = nodeTransform * parentTransform;

    // 🔥 Ищем кость — с ДВУМЯ попытками
    auto boneIt = m_boneInfoMap.find(nodeName);

    // Попытка 2: ищем по оригинальному имени
    if (boneIt == m_boneInfoMap.end() && !rawName.empty()) {
        boneIt = m_boneInfoMap.find(rawName);
        if (boneIt != m_boneInfoMap.end() && debugCount <= 20) {
            OutputDebugStringA(("  🔁 Fallback: '" + nodeName + "' -> '" + rawName + "'\n").c_str());
        }
    }

    // Попытка 3: ищем по имени канала
    if (boneIt == m_boneInfoMap.end() && channel) {
        boneIt = m_boneInfoMap.find(channel->nodeName);
        if (boneIt != m_boneInfoMap.end() && debugCount <= 20) {
            OutputDebugStringA(("  🔁 Fallback: '" + nodeName + "' -> Channel='" + channel->nodeName + "'\n").c_str());
        }
    }

    if(boneIt != m_boneInfoMap.end()) {
        // === СКЕЛЕТНАЯ АНИМАЦИЯ ===
        int boneIndex = boneIt->second.boneIndex;
        if (boneIndex >= 0 && boneIndex < (int)outMatrices.size()) {
            DirectX::XMMATRIX offsetMat = DirectX::XMLoadFloat4x4(&boneIt->second.offsetMatrix);
            DirectX::XMMATRIX finalMat = offsetMat * globalTransform;
            DirectX::XMStoreFloat4x4(&outMatrices[boneIndex], finalMat);
        }
    }
    else {
        // === НОДОВАЯ АНИМАЦИЯ (без костей) ===
        // Записываем ТОЛЬКО локальную анимированную трансформацию, без global/parent
        if (!outMatrices.empty() && channel != nullptr) {
            // nodeTransform уже содержит анимацию (scale*rot*pos)
            // НЕ умножаем на parentTransform — это сделает шейдер через worldMatrix
            DirectX::XMStoreFloat4x4(&outMatrices[0], globalTransform);

            static bool logged = false;
            if (!logged) {
                OutputDebugStringA(("  [NodeAnim] Writing LOCAL transform to slot 0: '" + nodeName + "'\n").c_str());
                logged = true;
            }
        }
    }

    // Рекурсия
    for (unsigned int i = 0; i < node->mNumChildren; ++i) {
        ReadNodeHierarchyToComponent(node->mChildren[i], globalTransform, anim, animTime, outMatrices);
    }
    
}
void AssimpModelClass::ComputeBoneMatrices(int animIndex, float animTime,
    std::vector<XMFLOAT4X4>& outMatrices) const {
    if (animIndex < 0 || animIndex >= (int)m_animations.size()) return;
    if (m_scene && m_scene->mRootNode) {
        outMatrices.resize(m_boneInfoMap.size());
        XMMATRIX identity = XMMatrixIdentity();
        for (auto& m : outMatrices) XMStoreFloat4x4(&m, identity);

        ReadNodeHierarchyToComponent(m_scene->mRootNode, identity,
            m_animations[animIndex], animTime, outMatrices);

    }
}

// ============================================================================
// Функция для вывода полной информации о модели (для отладки)
// ============================================================================
void AssimpModelClass::PrintModelDebugInfo() {
#if DEBUG_VERBOSE
    OutputDebugStringA("\n========== MODEL DEBUG INFO ==========\n");

    // Информация о сцене
    if (m_scene) {
        OutputDebugStringA(("Scene: meshes=" + std::to_string(m_scene->mNumMeshes) +
            ", materials=" + std::to_string(m_scene->mNumMaterials) +
            ", textures=" + std::to_string(m_scene->mNumTextures) +
            ", animations=" + std::to_string(m_scene->mNumAnimations) + "\n").c_str());

        // Корневой узел
        if (m_scene->mRootNode) {
            OutputDebugStringA(("Root node: '" + std::string(m_scene->mRootNode->mName.C_Str()) +
                "', children=" + std::to_string(m_scene->mRootNode->mNumChildren) + "\n").c_str());
        }
    }

    // Информация о костях
    OutputDebugStringA(("Bones: " + std::to_string(m_boneInfoMap.size()) + "\n").c_str());
    for (const auto& [name, info] : m_boneInfoMap) {
        OutputDebugStringA(("  Bone[" + std::to_string(info.boneIndex) + "]: '" + name + "'\n").c_str());
    }

    // Информация об анимациях
    OutputDebugStringA(("Animations: " + std::to_string(m_animations.size()) + "\n").c_str());
    for (size_t i = 0; i < m_animations.size(); ++i) {
        const auto& anim = m_animations[i];
        OutputDebugStringA(("  Anim[" + std::to_string(i) + "]: '" + anim.name +
            "', duration=" + std::to_string(anim.duration) +
            ", channels=" + std::to_string(anim.channels.size()) + "\n").c_str());

        for (size_t c = 0; c < anim.channels.size() && c < 5; ++c) {
            const auto& ch = anim.channels[c];
            OutputDebugStringA(("    Channel[" + std::to_string(c) + "]: '" + ch.nodeName +
                "', pos=" + std::to_string(ch.positions.size()) +
                ", rot=" + std::to_string(ch.rotations.size()) +
                ", scale=" + std::to_string(ch.scales.size()) + "\n").c_str());
        }
    }

    // Информация о меши
    OutputDebugStringA(("Mesh parts: " + std::to_string(m_meshParts.size()) + "\n").c_str());
    for (size_t i = 0; i < m_meshParts.size(); ++i) {
        const auto& part = m_meshParts[i];
        OutputDebugStringA(("  Mesh[" + std::to_string(i) + "]: indices=" + std::to_string(part.indexCount) +
            ", texture=" + (part.texture ? "yes" : "no") +
            ", normalMap=" + (part.normalMap ? "yes" : "no") + "\n").c_str());
    }

    OutputDebugStringA("========== END DEBUG INFO ==========\n\n");
#endif
}

// ============================================================================
// 🔥 КРИТИЧЕСКАЯ ДИАГНОСТИКА для нефтяного насоса
// ============================================================================
void AssimpModelClass::DebugAnimationBinding(const AnimationClip& anim) {
    OutputDebugStringA("\n========== ANIMATION BINDING DEBUG ==========\n");

    // 1. Список всех костей
    OutputDebugStringA("--- Registered Bones ---\n");
    for (const auto& [name, info] : m_boneInfoMap) {
        OutputDebugStringA(("  BONE[" + std::to_string(info.boneIndex) + "]: '" + name + "'\n").c_str());
    }

    // 2. Список каналов анимации и их привязка
    OutputDebugStringA("--- Animation Channels Binding ---\n");
    int bound = 0, unbound = 0;

    for (size_t c = 0; c < anim.channels.size(); ++c) {
        const auto& ch = anim.channels[c];
        auto it = m_boneInfoMap.find(ch.nodeName);

        if (it != m_boneInfoMap.end()) {
            OutputDebugStringA(("  ✓ CH[" + std::to_string(c) + "] '" + ch.nodeName +
                "' -> BONE[" + std::to_string(it->second.boneIndex) + "]\n").c_str());
            bound++;
        }
        else {
            OutputDebugStringA(("  ✗ CH[" + std::to_string(c) + "] '" + ch.nodeName +
                "' -> NOT FOUND in bone map!\n").c_str());
            unbound++;
        }
    }

    OutputDebugStringA(("Summary: bound=" + std::to_string(bound) +
        ", unbound=" + std::to_string(unbound) + "\n").c_str());

    // 3. Проверка первой матрицы
    if (!m_finalBoneMatrices.empty()) {
        XMFLOAT4X4& m = m_finalBoneMatrices[0];
        bool isZero = (m._11 == 1 && m._22 == 1 && m._33 == 1 && m._44 == 1 &&
            m._12 == 0 && m._13 == 0 && m._14 == 0 &&
            m._21 == 0 && m._23 == 0 && m._24 == 0 &&
            m._31 == 0 && m._32 == 0 && m._34 == 0 &&
            m._41 == 0 && m._42 == 0 && m._43 == 0);

        OutputDebugStringA(("First bone matrix: " + std::string(isZero ? "IDENTITY (not animated?)" : "MODIFIED") + "\n").c_str());
    }

    OutputDebugStringA("========== END BINDING DEBUG ==========\n");
}