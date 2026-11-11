// ============================================================================
// assimpmodelclass.cpp пїЅ пїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ
// ============================================================================

#include "assimpmodelclass.h"
#include <iostream>
#include <Windows.h>
#include <fstream>
#include <GltfMaterial.h>
#include <algorithm>
#include <cctype>
#include "PBRShaderClass.h"
#include <DirectXMath.h>
#include <functional>
#include <iomanip>
using namespace DirectX;

// ============================================================================
// пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅ
// ============================================================================
#define DEBUG_VERBOSE 0          // пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅ
#define DEBUG_BONE_MATCHING 0    // пїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅ
#define DEBUG_TRANSFORMS 0       // пїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ
#define DEBUG_ANIMATION_KEYS 0   // пїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ

static std::string SafeReadAiString(const aiString& aiStr) {
    if (aiStr.length == 0) return "";
    // пїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅ aiStr.length пїЅпїЅпїЅпїЅ, пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ \0
    return std::string(aiStr.C_Str(), aiStr.length);
}


static std::string NormalizeBoneName(std::string name) {
    // 1. пїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ
    size_t pos = name.find("_scaleCompensation");
    if (pos != std::string::npos) name = name.substr(0, pos);

    // 2. пїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅ _01, _02, _14 пїЅ пїЅ.пїЅ.
    pos = name.find_last_of('_');
    if (pos != std::string::npos) {
        std::string suffix = name.substr(pos + 1);
        bool isNumber = true;
        for (char c : suffix) if (!std::isdigit(static_cast<unsigned char>(c))) { isNumber = false; break; }
        if (isNumber && !suffix.empty()) name = name.substr(0, pos);
    }
    return name;
}

struct FistBoneRotation
{
    const char* bone;
    bool rightHand;
    XMFLOAT4 rotation; // DirectX order: x, y, z, w.
};

// Captured from Kevin_Main_Switchable.blend by comparing the evaluated deform
// skeleton with CTRL_Fist.L/R at identity and at the authored closed value.
static constexpr FistBoneRotation kFistBoneRotations[] = {
    { "mixamorig:LeftHandThumb2", false, { 0.5774817f, -0.0702033f, -0.3133351f, 0.7506047f } },
    { "mixamorig:LeftHandThumb3", false, { 0.0698593f,  0.1749494f, -0.4934925f, 0.8491040f } },
    { "mixamorig:LeftHandIndex1", false, { 0.7876519f, 0.0f, 0.0f, 0.6161205f } },
    { "mixamorig:LeftHandIndex2", false, { 0.6933677f, 0.0f, 0.0f, 0.7205840f } },
    { "mixamorig:LeftHandIndex3", false, { 0.6300752f, 0.0f, 0.0f, 0.7765342f } },
    { "mixamorig:LeftHandMiddle1", false, { 0.7876519f, 0.0f, 0.0f, 0.6161205f } },
    { "mixamorig:LeftHandMiddle2", false, { 0.7699874f, 0.0f, 0.0f, 0.6380591f } },
    { "mixamorig:LeftHandMiddle3", false, { 0.6300752f, 0.0f, 0.0f, 0.7765342f } },
    { "mixamorig:LeftHandRing1", false, { 0.7876519f, 0.0f, 0.0f, 0.6161205f } },
    { "mixamorig:LeftHandRing2", false, { 0.6300752f, 0.0f, 0.0f, 0.7765342f } },
    { "mixamorig:LeftHandRing3", false, { 0.6300752f, 0.0f, 0.0f, 0.7765342f } },
    { "mixamorig:LeftHandPinky1", false, { 0.7876519f, 0.0f, 0.0f, 0.6161205f } },
    { "mixamorig:LeftHandPinky2", false, { 0.6300752f, 0.0f, 0.0f, 0.7765342f } },
    { "mixamorig:LeftHandPinky3", false, { 0.6300752f, 0.0f, 0.0f, 0.7765342f } },
    { "mixamorig:RightHandThumb2", true, { 0.4423940f, -0.0589448f, 0.4505921f, 0.7731622f } },
    { "mixamorig:RightHandThumb3", true, { 0.2145435f, -0.2989273f, 0.5006011f, 0.7835893f } },
    { "mixamorig:RightHandIndex1", true, { 0.7750560f, 0.0f, 0.0f, 0.6318925f } },
    { "mixamorig:RightHandIndex2", true, { 0.6182232f, 0.0f, 0.0f, 0.7860025f } },
    { "mixamorig:RightHandIndex3", true, { 0.3862690f, 0.0f, 0.0f, 0.9223862f } },
    { "mixamorig:RightHandMiddle1", true, { 0.8307433f, 0.0f, 0.0f, 0.5566558f } },
    { "mixamorig:RightHandMiddle2", true, { 0.6095273f, 0.0f, 0.0f, 0.7927651f } },
    { "mixamorig:RightHandMiddle3", true, { 0.3862690f, 0.0f, 0.0f, 0.9223862f } },
    { "mixamorig:RightHandRing1", true, { 0.7750560f, 0.0f, 0.0f, 0.6318926f } },
    { "mixamorig:RightHandRing2", true, { 0.6182232f, 0.0f, 0.0f, 0.7860025f } },
    { "mixamorig:RightHandRing3", true, { 0.3862690f, 0.0f, 0.0f, 0.9223862f } },
    { "mixamorig:RightHandPinky1", true, { 0.6182232f, 0.0f, 0.0f, 0.7860025f } },
    { "mixamorig:RightHandPinky2", true, { 0.6182232f, 0.0f, 0.0f, 0.7860025f } },
    { "mixamorig:RightHandPinky3", true, { 0.3862690f, 0.0f, 0.0f, 0.9223862f } },
};

// The thumb controls use Blender Transform constraints that map one input axis
// to three Euler axes with ADD mixing and a per-control influence. Slerping to
// the final quaternion cuts across that mapped path, so keep the evaluated
// Blender curve at 1/8 controller intervals. Values are x, y, z, w.
static constexpr XMFLOAT4 kLeftThumb2Curve[] = {
    { 0.0f, 0.0f, 0.0f, 1.0f },
    { 0.09250893f, 0.02363555f, -0.04027343f, 0.99461627f },
    { 0.18618682f, 0.03632610f, -0.08620033f, 0.97805130f },
    { 0.27904594f, 0.03707414f, -0.13576256f, 0.94990915f },
    { 0.36900008f, 0.02517897f, -0.18683858f, 0.91010785f },
    { 0.45389247f, 0.00022307f, -0.23724328f, 0.85889298f },
    { 0.53151488f, -0.03794977f, -0.28475875f, 0.79684639f },
    { 0.57748175f, -0.07020326f, -0.31333506f, 0.75060469f },
    { 0.57748175f, -0.07020326f, -0.31333506f, 0.75060469f },
};
static constexpr XMFLOAT4 kLeftThumb3Curve[] = {
    { 0.0f, 0.0f, 0.0f, 1.0f },
    { -0.00301321f, 0.02814999f, -0.08001261f, 0.99639177f },
    { -0.00118190f, 0.05660750f, -0.15898590f, 0.98565602f },
    { 0.00551534f, 0.08474633f, -0.23622051f, 0.96798116f },
    { 0.01702757f, 0.11193839f, -0.31104562f, 0.94362628f },
    { 0.03323150f, 0.13756144f, -0.38282439f, 0.91291732f },
    { 0.05393222f, 0.16100876f, -0.45096278f, 0.87624204f },
    { 0.06985933f, 0.17494938f, -0.49349248f, 0.84910399f },
    { 0.06985933f, 0.17494938f, -0.49349248f, 0.84910399f },
};
static constexpr XMFLOAT4 kRightThumb2Curve[] = {
    { 0.0f, 0.0f, 0.0f, 1.0f },
    { 0.05730864f, -0.04442636f, 0.05590607f, 0.99579942f },
    { 0.12066549f, -0.07806060f, 0.11867457f, 0.98247784f },
    { 0.18795626f, -0.09959960f, 0.18628632f, 0.95919228f },
    { 0.25699300f, -0.10815008f, 0.25660044f, 0.92542660f },
    { 0.32557040f, -0.10319341f, 0.32741198f, 0.88099742f },
    { 0.39151216f, -0.08454646f, 0.39649811f, 0.82605046f },
    { 0.44239405f, -0.05894479f, 0.45059210f, 0.77316219f },
    { 0.44239405f, -0.05894479f, 0.45059210f, 0.77316219f },
};
static constexpr XMFLOAT4 kRightThumb3Curve[] = {
    { 0.0f, 0.0f, 0.0f, 1.0f },
    { 0.00822268f, -0.05472275f, 0.08201542f, 0.99509364f },
    { 0.02529619f, -0.10756713f, 0.16313009f, 0.98039681f },
    { 0.05085097f, -0.15730642f, 0.24204037f, 0.95607811f },
    { 0.08435325f, -0.20278037f, 0.31747234f, 0.92248362f },
    { 0.12511627f, -0.24291688f, 0.38820729f, 0.88013208f },
    { 0.17231423f, -0.27675205f, 0.45310465f, 0.82970613f },
    { 0.21454346f, -0.29892731f, 0.50060105f, 0.78358930f },
    { 0.21454346f, -0.29892731f, 0.50060105f, 0.78358930f },
};

static bool SampleMappedThumbRotation(const std::string& nodeName,
    float amount, XMVECTOR& outRotation)
{
    const XMFLOAT4* curve = nullptr;
    if (nodeName == "mixamorig:LeftHandThumb2") curve = kLeftThumb2Curve;
    else if (nodeName == "mixamorig:LeftHandThumb3") curve = kLeftThumb3Curve;
    else if (nodeName == "mixamorig:RightHandThumb2") curve = kRightThumb2Curve;
    else if (nodeName == "mixamorig:RightHandThumb3") curve = kRightThumb3Curve;
    if (!curve) return false;

    const float position = std::clamp(std::abs(amount), 0.0f, 1.0f) * 8.0f;
    const int first = (std::min)(static_cast<int>(position), 7);
    const int second = first + 1;
    const float alpha = position - static_cast<float>(first);
    outRotation = XMQuaternionSlerp(XMLoadFloat4(&curve[first]),
        XMLoadFloat4(&curve[second]), alpha);
    if (amount < 0.0f) outRotation = XMQuaternionInverse(outRotation);
    return true;
}

static float EvaluateFistAmount(const std::vector<AnimationFistKey>& keys,
    const std::string& clip, bool rightHand, int variant, float seconds)
{
    bool hasVariantTrack = false;
    if (variant >= 0)
        for (const auto& key : keys)
            if (key.variant == variant && key.clip == clip &&
                key.rightHand == rightHand)
            {
                hasVariantTrack = true;
                break;
            }
    const int activeVariant = hasVariantTrack ? variant : -1;
    const AnimationFistKey* before = nullptr;
    const AnimationFistKey* after = nullptr;
    for (const auto& key : keys)
    {
        if (key.variant != activeVariant || key.clip != clip ||
            key.rightHand != rightHand) continue;
        if (key.time <= seconds && (!before || key.time > before->time)) before = &key;
        if (key.time >= seconds && (!after || key.time < after->time)) after = &key;
    }
    if (!before) before = after;
    if (!after) after = before;
    if (!before || !after) return 0.0f;
    const float span = after->time - before->time;
    const float alpha = span > 0.00001f
        ? std::clamp((seconds - before->time) / span, 0.0f, 1.0f) : 0.0f;
    return std::clamp(before->amount + (after->amount - before->amount) * alpha,
        -1.0f, 1.0f);
}

static XMMATRIX FistCorrectionForBone(const std::string& nodeName,
    const std::vector<AnimationFistKey>* keys, const std::string& clip,
    int variant, float seconds)
{
    if (!keys || keys->empty()) return XMMatrixIdentity();
    for (const auto& captured : kFistBoneRotations)
    {
        if (NormalizeBoneName(captured.bone) != nodeName) continue;
        const float amount = EvaluateFistAmount(*keys, clip,
            captured.rightHand, variant, seconds);
        if (std::abs(amount) <= 0.00001f) return XMMatrixIdentity();
        XMVECTOR rotation;
        if (SampleMappedThumbRotation(nodeName, amount, rotation))
            return XMMatrixRotationQuaternion(rotation);

        XMVECTOR target = XMLoadFloat4(&captured.rotation);
        if (amount < 0.0f) target = XMQuaternionInverse(target);
        // The source fist controls rotate farther than the 130-degree Map From
        // range, so the constraint reaches its authored maximum before the UI
        // slider reaches 1. Match that saturation for the single-axis fingers.
        const float saturation = captured.rightHand ? 0.8530f : 0.8321f;
        const float mappedAmount = std::clamp(
            std::abs(amount) / saturation, 0.0f, 1.0f);
        rotation = XMQuaternionSlerp(
            XMQuaternionIdentity(), target, mappedAmount);
        return XMMatrixRotationQuaternion(rotation);
    }
    return XMMatrixIdentity();
}

// ============================================================================
// пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅ
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

}

// ============================================================================
// пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ / пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ
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
// пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅ
// ============================================================================
static bool WriteTempFile(const std::string& filename, const char* data, size_t size)
{
    std::ofstream of(filename, std::ios::binary);
    if (!of.is_open()) return false;
    of.write(data, static_cast<std::streamsize>(size));
    return of.good();
}

static std::unordered_map<std::string, ID3D11ShaderResourceView*> g_assimpTextureCache;

static std::string MakeTextureCacheKey(
    const aiScene* scene,
    const aiMaterial* mat,
    aiTextureType type,
    const aiString& path)
{
    return std::to_string(reinterpret_cast<uintptr_t>(scene)) + "|" +
        std::to_string(reinterpret_cast<uintptr_t>(mat)) + "|" +
        std::to_string(static_cast<int>(type)) + "|" + path.C_Str();
}

static ID3D11ShaderResourceView* TryGetCachedTexture(const std::string& key)
{
    auto it = g_assimpTextureCache.find(key);
    if (it == g_assimpTextureCache.end() || !it->second)
        return nullptr;

    it->second->AddRef();
    return it->second;
}

static void StoreCachedTexture(const std::string& key, ID3D11ShaderResourceView* srv)
{
    if (!srv || g_assimpTextureCache.find(key) != g_assimpTextureCache.end())
        return;

    srv->AddRef();
    g_assimpTextureCache[key] = srv;
}

XMFLOAT4X4 AssimpModelClass::ConvertMatrix(const aiMatrix4x4& from) const
{
    XMFLOAT4X4 to;
    // пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ:
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
// пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅ (пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅ)
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

    // пїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ
    if (mat->GetTextureCount(type1) > 0 && mat->GetTexture(type1, 0, &path) == AI_SUCCESS)
    {
        std::string texPath = path.C_Str();
        if (!texPath.empty())
        {
            const std::string cacheKey = MakeTextureCacheKey(scene, mat, type1, path);
            if (auto* cached = TryGetCachedTexture(cacheKey))
                return cached;

            // пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ (пїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅ "*0")
            if (texPath[0] == '*')
            {
                int index = atoi(texPath.c_str() + 1);
                if (scene && index >= 0 && index < static_cast<int>(scene->mNumTextures))
                {
                    const aiTexture* tex = scene->mTextures[index];
                    if (!tex) return nullptr;

                    if (tex->mHeight == 0) // пїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅ (PNG/JPG)
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
                        const bool forceSRGB =
                            type1 == aiTextureType_BASE_COLOR ||
                            type1 == aiTextureType_DIFFUSE;
                        if (!t || !t->Initialize(device, deviceContext, filename.c_str(), forceSRGB))
                        {
                            if (t) { t->Shutdown(); delete t; }
                            DeleteFileA(filename.c_str());
                            return nullptr;
                        }
                        m_ownedTextures.push_back(t);
                        DeleteFileA(filename.c_str());
                        ID3D11ShaderResourceView* srv = t->GetTexture();
                        if (srv)
                        {
                            StoreCachedTexture(cacheKey, srv);
                            srv->AddRef();
                        }
                        return srv;
                    }
                    else // Raw RGBA пїЅпїЅпїЅпїЅпїЅпїЅ
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
                        if (SUCCEEDED(hr) && srv)
                            StoreCachedTexture(cacheKey, srv);
                        return SUCCEEDED(hr) ? srv : nullptr;
                    }
                }
            }
            else // пїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅ
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
                ID3D11ShaderResourceView* srv = t->GetTexture();
                if (srv)
                {
                    StoreCachedTexture(cacheKey, srv);
                    srv->AddRef();
                }
                return srv;
            }
        }
    }

    // пїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅ, пїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅ
    if (type2 != aiTextureType_NONE && mat->GetTextureCount(type2) > 0)
    {
        if (mat->GetTexture(type2, 0, &path) == AI_SUCCESS)
            return ExtractTexture_Internal(device, deviceContext, scene, mat, type2, aiTextureType_NONE);
    }

    return nullptr;
}

// ============================================================================
// пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅ пїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅ пїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ
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


        for (unsigned int b = 0; b < mesh->mNumBones; ++b)
        {
            aiBone* bone = mesh->mBones[b];
            // Controller and end bones may be exported as joints without skin weights.
            // They still participate in the hierarchy, but do not consume GPU palette slots.
            if (bone->mNumWeights == 0) continue;
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

    // пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅ пїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ
    m_finalBoneMatrices.resize(m_boneInfoMap.size());
    XMFLOAT4X4 identity; XMStoreFloat4x4(&identity, XMMatrixIdentity());
    for (auto& mat : m_finalBoneMatrices) mat = identity;


}

// ============================================================================
// пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅ пїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ
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

            // пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ: пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ mNodeName пїЅ пїЅпїЅпїЅ пїЅпїЅпїЅпїЅ пїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ
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

            // пїЅпїЅпїЅпїЅпїЅпїЅпїЅ
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

            // пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ
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

            // пїЅпїЅпїЅпїЅпїЅпїЅпїЅ
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
// пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ
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

#if DEBUG_TRANSFORMS && false // пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ
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
// ReadNodeHierarchy пїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ
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

    // 1. пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅ пїЅпїЅпїЅпїЅпїЅ
    XMFLOAT4X4 nodeLocalFloat = ConvertMatrix(node->mTransformation);
    XMMATRIX nodeTransform = XMLoadFloat4x4(&nodeLocalFloat);

#if DEBUG_TRANSFORMS && false
    PrintMatrix(nodeLocalFloat, indent + "  NodeLocal");
#endif

    // 2. пїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅ
    const BoneAnimationChannel* channel = nullptr;
    for (const auto& ch : anim.channels)
    {
        if (ch.nodeName == nodeName) {
            channel = &ch;
#if DEBUG_TRANSFORMS
            OutputDebugStringA((indent + "  ? Found animation channel\n").c_str());
#endif
            break;
        }
    }

    // 3. пїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ
    if (channel)
    {
        XMMATRIX posMat = InterpolatePosition(channel->positions, animTimeInSeconds);
        XMMATRIX rotMat = InterpolateRotation(channel->rotations, animTimeInSeconds);
        XMMATRIX scaleMat = InterpolateScale(channel->scales, animTimeInSeconds);

        // пїЅпїЅпїЅпїЅпїЅпїЅпїЅ: Scale > Rotate > Translate (пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ)
        nodeTransform = scaleMat * rotMat * posMat;

#if DEBUG_TRANSFORMS && false
        PrintMatrix(nodeTransform, indent + "  AnimatedLocal");
#endif
    }

    // 4. пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ = пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ * пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ
    XMMATRIX globalTransform = nodeTransform * parentTransform;

#if DEBUG_TRANSFORMS && false
    PrintMatrix(globalTransform, indent + "  Global");
#endif

    // 5. пїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅ пїЅ пїЅпїЅпїЅпїЅпїЅ, пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅ
    auto boneIt = m_boneInfoMap.find(nodeName);
    if (boneIt != m_boneInfoMap.end())
    {
        int boneIndex = boneIt->second.boneIndex;
        XMMATRIX offsetMat = XMLoadFloat4x4(&boneIt->second.offsetMatrix);

        // ? пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅ DirectX: offset * global
        XMMATRIX finalMat = globalTransform * offsetMat;
        XMStoreFloat4x4(&m_finalBoneMatrices[boneIndex], finalMat);

#if DEBUG_TRANSFORMS
        sprintf_s(dbg, "%s  [Bone] '%s' (idx=%d) -> final matrix set\n",
            indent.c_str(), nodeName.c_str(), boneIndex);
        OutputDebugStringA(dbg);

        // пїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅ
        static bool firstBonePrinted = false;
        if (!firstBonePrinted && boneIndex == 0) {
            XMFLOAT4X4 temp; XMStoreFloat4x4(&temp, nodeTransform);
            PrintMatrix(temp, indent + "  NodeLocal");
            firstBonePrinted = true;
        }
#endif
    }

    // 6. пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅ пїЅпїЅпїЅпїЅпїЅ
    for (unsigned int i = 0; i < node->mNumChildren; ++i)
    {
        ReadNodeHierarchy(node->mChildren[i], globalTransform, anim, animTimeInSeconds);
    }

#if DEBUG_TRANSFORMS
    depth--;
#endif
}

// ============================================================================
// Initialize пїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ
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
        OutputDebugStringA((std::string("Assimp failed to load '") + modelPath + "': "
            + m_importer.GetErrorString() + "\n").c_str());
        return false;
    }

#if DEBUG_VERBOSE
    OutputDebugStringA(("Assimp: Model loaded. Meshes: " + std::to_string(m_scene->mNumMeshes) +
        ", Materials: " + std::to_string(m_scene->mNumMaterials) +
        ", Animations: " + std::to_string(m_scene->mNumAnimations) + "\n").c_str());
#endif

    m_totalIndexCount = 0;
    m_meshParts.clear();
    m_cpuVertices.clear();
    m_cpuTriangles.clear();
    m_cpuMeshVertices.clear();
    m_cpuMeshIndices.clear();
    m_boneInfoMap.clear();
    m_finalBoneMatrices.clear();
    m_animations.clear();
    m_animationTime = 0.0f;

    // пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅ пїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ
    LoadBones(m_scene);
    LoadAnimations(m_scene);



    // пїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅ
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

    // пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅ
    for (unsigned int i = 0; i < m_scene->mNumMeshes; ++i)
    {
        aiMesh* mesh = m_scene->mMeshes[i];
        // REF_ objects are Blender authoring references (e.g. a rifle preview).
        // Runtime weapons are independent entities attached to the animated wrist.
        auto isReference = [&](auto&& self, const aiNode* node, bool reference) -> bool {
            reference = reference || std::string(node->mName.C_Str()).rfind("REF_", 0) == 0;
            if (reference)
                for (unsigned int n = 0; n < node->mNumMeshes; ++n)
                    if (node->mMeshes[n] == i) return true;
            for (unsigned int n = 0; n < node->mNumChildren; ++n)
                if (self(self, node->mChildren[n], reference)) return true;
            return false;
        };
        if (isReference(isReference, m_scene->mRootNode, false)) continue;
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

            // пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅ
            for (int b = 0; b < 4; ++b) {
                v.boneIDs[b] = -1;
                v.boneWeights[b] = 0.0f;
            }

            vertices.push_back(v);
        }

        // Assign skin weights in one pass over Assimp's sparse bone weights.
        // The old vertex -> bone -> weight search made large skinned GLBs appear frozen.
        for (unsigned int b = 0; b < mesh->mNumBones; ++b)
        {
            const aiBone* bone = mesh->mBones[b];
            auto boneIt = m_boneInfoMap.find(NormalizeBoneName(bone->mName.C_Str()));
            if (boneIt == m_boneInfoMap.end())
                continue;

            const int boneIdx = boneIt->second.boneIndex;
            for (unsigned int w = 0; w < bone->mNumWeights; ++w)
            {
                const aiVertexWeight& influence = bone->mWeights[w];
                if (influence.mVertexId >= vertices.size() || influence.mWeight <= 0.0f)
                    continue;

                VertexType& vertex = vertices[influence.mVertexId];
                int targetSlot = -1;
                for (int slot = 0; slot < 4; ++slot) {
                    if (vertex.boneIDs[slot] == -1) {
                        targetSlot = slot;
                        break;
                    }
                }

                if (targetSlot == -1) {
                    targetSlot = 0;
                    for (int slot = 1; slot < 4; ++slot) {
                        if (vertex.boneWeights[slot] < vertex.boneWeights[targetSlot])
                            targetSlot = slot;
                    }
                    if (influence.mWeight <= vertex.boneWeights[targetSlot])
                        continue;
                }

                vertex.boneIDs[targetSlot] = boneIdx;
                vertex.boneWeights[targetSlot] = influence.mWeight;
            }
        }

        for (VertexType& vertex : vertices)
        {
            const float sum = vertex.boneWeights[0] + vertex.boneWeights[1]
                + vertex.boneWeights[2] + vertex.boneWeights[3];
            if (sum > 0.0001f) {
                for (int slot = 0; slot < 4; ++slot)
                    vertex.boneWeights[slot] /= sum;
            }
            else {
                // Static vertices use their original position in the PBR vertex shader.
                for (int slot = 0; slot < 4; ++slot) {
                    vertex.boneIDs[slot] = -1;
                    vertex.boneWeights[slot] = 0.0f;
                }
            }
        }

        // пїЅпїЅпїЅпїЅпїЅпїЅпїЅ
        std::vector<unsigned long> indices;
        indices.reserve(part.indexCount);
        for (unsigned int f = 0; f < mesh->mNumFaces; f++)
            for (unsigned int k = 0; k < mesh->mFaces[f].mNumIndices; k++)
                indices.push_back(mesh->mFaces[f].mIndices[k]);

        // пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅ
        D3D11_BUFFER_DESC vbDesc = { sizeof(VertexType) * (UINT)vertices.size(), D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER };
        D3D11_SUBRESOURCE_DATA vbData = { vertices.data() };
        if (FAILED(device->CreateBuffer(&vbDesc, &vbData, &part.vertexBuffer))) return false;

        D3D11_BUFFER_DESC ibDesc = { sizeof(unsigned long) * (UINT)indices.size(), D3D11_USAGE_DEFAULT, D3D11_BIND_INDEX_BUFFER };
        D3D11_SUBRESOURCE_DATA ibData = { indices.data() };
        if (FAILED(device->CreateBuffer(&ibDesc, &ibData, &part.indexBuffer))) return false;

        // пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ
        if (mesh->mMaterialIndex >= 0 && mesh->mMaterialIndex < (int)m_scene->mNumMaterials)
        {
            const aiMaterial* mat = m_scene->mMaterials[mesh->mMaterialIndex];
            part.texture = ExtractTexture_Internal(device, deviceContext, m_scene, mat, aiTextureType_BASE_COLOR, aiTextureType_DIFFUSE);
            part.normalMap = ExtractTexture_Internal(device, deviceContext, m_scene, mat, aiTextureType_NORMALS, aiTextureType_HEIGHT);
            part.metalness = ExtractTexture_Internal(device, deviceContext, m_scene, mat, aiTextureType_METALNESS, aiTextureType_UNKNOWN);
            part.roughness = ExtractTexture_Internal(device, deviceContext, m_scene, mat, aiTextureType_DIFFUSE_ROUGHNESS, aiTextureType_UNKNOWN);

            // пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅ пїЅпїЅпїЅпїЅпїЅ, пїЅпїЅпїЅпїЅ пїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅ
            if (!part.texture && !m_Texture)
                part.texture = Create1x1SRVFromColor(device, GetPBRColorFromMaterial(mat, aiTextureType_BASE_COLOR));
            if (!part.metalness) {
                float metallic = 0.0f;
                aiGetMaterialFloat(mat, AI_MATKEY_METALLIC_FACTOR, &metallic);
                part.metalness = Create1x1SRVFromColor(device, aiColor4D(metallic, metallic, metallic, 1.0f));
            }
            if (!part.roughness) {
                float roughness = 220.0f / 255.0f;
                if (aiGetMaterialFloat(mat, AI_MATKEY_ROUGHNESS_FACTOR, &roughness) != AI_SUCCESS)
                    roughness = 220.0f / 255.0f;
                part.roughness = Create1x1SRVFromColor(device, aiColor4D(roughness, roughness, roughness, 1.0f));
            }

            // пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅ пїЅпїЅпїЅпїЅпїЅпїЅ
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
        OutputDebugStringA((std::string("Assimp model contains no valid meshes: ")
            + modelPath + "\n").c_str());
        return false;
    }

    // Fallback пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ
    if (textureFilename && textureFilename[0]) {
        m_Texture = new TextureClass;
        if (!m_Texture || !m_Texture->Initialize(device, deviceContext, textureFilename)) {
            if (m_Texture) { delete m_Texture; m_Texture = nullptr; }
            return false;
        }
    }

    // пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅ
    GetBoundingBox(&m_cachedBBMin, &m_cachedBBMax);
    GetBoundingSphere(&m_cachedSphereCenter, &m_cachedSphereRadius);
    m_boundsCached = true;



#if DEBUG_VERBOSE
    OutputDebugStringA("========== MODEL INITIALIZED SUCCESSFULLY ==========\n\n");
#endif
    return true;
}

void AssimpModelClass::DebugCompareNames()
{
    OutputDebugStringA("=== NAME COMPARISON DEBUG ===\n");

    // 1. пїЅпїЅпїЅпїЅпїЅ (пїЅпїЅ пїЅпїЅпїЅпїЅпїЅ)
    OutputDebugStringA("--- Bones (aiBone::mName) ---\n");
    for (const auto& [name, info] : m_boneInfoMap) {
        OutputDebugStringA(("  BONE: [" + name + "] (index: " + std::to_string(info.boneIndex) + ")\n").c_str());
    }

    // 2. пїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ (aiNode::mName)
    std::function<void(const aiNode*, int)> dumpNodes = [&](const aiNode* node, int depth) {
        std::string indent(depth * 2, ' ');
        std::string name = node->mName.C_Str();
        bool isBone = m_boneInfoMap.count(NormalizeBoneName(name)) > 0;
        OutputDebugStringA((indent + "NODE: [" + name + "]" + (isBone ? " ?bone" : "") + "\n").c_str());
        for (unsigned i = 0; i < node->mNumChildren; ++i)
            dumpNodes(node->mChildren[i], depth + 1);
        };
    if (m_scene && m_scene->mRootNode) {
        OutputDebugStringA("--- Nodes (aiNode::mName) ---\n");
        dumpNodes(m_scene->mRootNode, 0);
    }

    // 3. пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅ (aiNodeAnim::mNodeName)
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
    m_cpuVertices.clear();
    m_cpuTriangles.clear();
    m_cpuMeshVertices.clear();
    m_cpuMeshIndices.clear();

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


bool AssimpModelClass::InitializeFromMeshes(
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    const aiScene* scene,
    const unsigned int* meshIndices,
    unsigned int meshCount,
    bool loadImageTextures)
{
    if (!device || !context || !scene || !meshIndices || meshCount == 0)
        return false;

    Shutdown();

    m_scene = nullptr;
    m_isEmbeddedFormat = true;
    m_totalIndexCount = 0;
    m_boundsCached = false;

    XMFLOAT3 bbMin(FLT_MAX, FLT_MAX, FLT_MAX);
    XMFLOAT3 bbMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    unsigned long globalVertexBase = 0;

    for (unsigned int meshSlot = 0; meshSlot < meshCount; ++meshSlot)
    {
        const unsigned int meshIndex = meshIndices[meshSlot];
        if (meshIndex >= scene->mNumMeshes)
            continue;

        aiMesh* mesh = scene->mMeshes[meshIndex];
        if (!mesh || mesh->mNumVertices == 0)
            continue;

        MeshPart part;
        part.indexCount = static_cast<int>(mesh->mNumFaces) * 3;

        std::vector<VertexType> vertices;
        vertices.reserve(mesh->mNumVertices);

        for (unsigned int i = 0; i < mesh->mNumVertices; ++i)
        {
            VertexType v = {};

            v.position = XMFLOAT3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
            v.texture = mesh->HasTextureCoords(0)
                ? XMFLOAT2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y)
                : XMFLOAT2(0.0f, 0.0f);
            v.normal = mesh->HasNormals()
                ? XMFLOAT3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z)
                : XMFLOAT3(0.0f, 1.0f, 0.0f);

            if (mesh->HasTangentsAndBitangents())
            {
                v.tangent = XMFLOAT3(mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z);
                v.binormal = XMFLOAT3(mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z);
            }

            for (int b = 0; b < 4; ++b)
            {
                v.boneIDs[b] = -1;
                v.boneWeights[b] = 0.0f;
            }

            bbMin.x = (std::min)(bbMin.x, v.position.x);
            bbMin.y = (std::min)(bbMin.y, v.position.y);
            bbMin.z = (std::min)(bbMin.z, v.position.z);
            bbMax.x = (std::max)(bbMax.x, v.position.x);
            bbMax.y = (std::max)(bbMax.y, v.position.y);
            bbMax.z = (std::max)(bbMax.z, v.position.z);

            m_cpuVertices.push_back(v.position);
            m_cpuMeshVertices.push_back(v);
            vertices.push_back(v);
        }

        std::vector<unsigned long> indices;
        indices.reserve(part.indexCount);

        for (unsigned int f = 0; f < mesh->mNumFaces; ++f)
        {
            const aiFace& face = mesh->mFaces[f];
            if (face.mNumIndices != 3)
                continue;

            for (unsigned int j = 0; j < 3; ++j)
            {
                const unsigned int idx = face.mIndices[j];
                indices.push_back(idx);
                m_cpuTriangles.push_back(vertices[idx].position);
                m_cpuMeshIndices.push_back(globalVertexBase + idx);
            }
        }

        if (vertices.empty() || indices.empty())
            continue;

        part.indexCount = static_cast<int>(indices.size());

        D3D11_BUFFER_DESC vbDesc = {};
        vbDesc.ByteWidth = sizeof(VertexType) * static_cast<UINT>(vertices.size());
        vbDesc.Usage = D3D11_USAGE_DEFAULT;
        vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA vbData = {};
        vbData.pSysMem = vertices.data();

        HRESULT hr = device->CreateBuffer(&vbDesc, &vbData, &part.vertexBuffer);
        if (FAILED(hr))
        {
            Shutdown();
            return false;
        }

        D3D11_BUFFER_DESC ibDesc = {};
        ibDesc.ByteWidth = sizeof(unsigned long) * static_cast<UINT>(indices.size());
        ibDesc.Usage = D3D11_USAGE_DEFAULT;
        ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

        D3D11_SUBRESOURCE_DATA ibData = {};
        ibData.pSysMem = indices.data();

        hr = device->CreateBuffer(&ibDesc, &ibData, &part.indexBuffer);
        if (FAILED(hr))
        {
            if (part.vertexBuffer) part.vertexBuffer->Release();
            part.vertexBuffer = nullptr;
            Shutdown();
            return false;
        }

        if (mesh->mMaterialIndex < scene->mNumMaterials)
        {
            const aiMaterial* mat = scene->mMaterials[mesh->mMaterialIndex];
            if (loadImageTextures)
            {
                part.texture = ExtractTexture_Internal(device, context, scene, mat, aiTextureType_BASE_COLOR, aiTextureType_DIFFUSE);
                part.normalMap = ExtractTexture_Internal(device, context, scene, mat, aiTextureType_NORMALS, aiTextureType_HEIGHT);
                part.metalness = ExtractTexture_Internal(device, context, scene, mat, aiTextureType_METALNESS, aiTextureType_UNKNOWN);
                part.roughness = ExtractTexture_Internal(device, context, scene, mat, aiTextureType_DIFFUSE_ROUGHNESS, aiTextureType_UNKNOWN);
            }

            if (!part.texture)
                part.texture = Create1x1SRVFromColor(device, GetPBRColorFromMaterial(mat, aiTextureType_BASE_COLOR));

            if (!part.metalness)
            {
                float metallic = 0.0f;
                aiGetMaterialFloat(mat, AI_MATKEY_METALLIC_FACTOR, &metallic);
                part.metalness = Create1x1SRVFromColor(device, aiColor4D(metallic, metallic, metallic, 1.0f));
            }

            if (!part.roughness)
            {
                float roughness = 220.0f / 255.0f;
                if (aiGetMaterialFloat(mat, AI_MATKEY_ROUGHNESS_FACTOR, &roughness) != AI_SUCCESS)
                    roughness = 220.0f / 255.0f;
                part.roughness = Create1x1SRVFromColor(device, aiColor4D(roughness, roughness, roughness, 1.0f));
            }

            aiString alphaMode;
            mat->Get(AI_MATKEY_GLTF_ALPHAMODE, alphaMode);
            float opacity = 1.0f;
            float transmission = 0.0f;
            aiGetMaterialFloat(mat, AI_MATKEY_OPACITY, &opacity);
            aiGetMaterialFloat(mat, AI_MATKEY_TRANSMISSION_FACTOR, &transmission);
            part.isTransparent = (std::string(alphaMode.C_Str()) == "BLEND") || (opacity < 0.999f) || (transmission > 0.01f);
            part.opacity = opacity;

            float metallicFactor = 0.0f;
            float ior = 1.0f;
            aiGetMaterialFloat(mat, AI_MATKEY_METALLIC_FACTOR, &metallicFactor);
            aiGetMaterialFloat(mat, AI_MATKEY_REFRACTI, &ior);
            aiString matName;
            mat->Get(AI_MATKEY_NAME, matName);
            std::string matNameStr = matName.C_Str();
            std::transform(matNameStr.begin(), matNameStr.end(), matNameStr.begin(), ::tolower);

            part.isGlass = (part.isTransparent && (metallicFactor > 0.5f || transmission > 0.01f)) ||
                matNameStr.find("glass") != std::string::npos ||
                (ior > 1.01f && metallicFactor > 0.5f);
            if (part.isGlass && !part.isTransparent) part.isTransparent = true;
            if (part.isGlass && part.opacity > 0.5f) part.opacity = 0.12f;
        }

        m_totalIndexCount += part.indexCount;
        m_meshParts.push_back(part);
        globalVertexBase += static_cast<unsigned long>(vertices.size());
    }

    if (m_meshParts.empty())
        return false;

    m_cachedBBMin = bbMin;
    m_cachedBBMax = bbMax;
    m_cachedSphereCenter = XMFLOAT3(
        (bbMin.x + bbMax.x) * 0.5f,
        (bbMin.y + bbMax.y) * 0.5f,
        (bbMin.z + bbMax.z) * 0.5f);

    float maxDistSq = 0.0f;
    for (const XMFLOAT3& v : m_cpuVertices)
    {
        const float dx = v.x - m_cachedSphereCenter.x;
        const float dy = v.y - m_cachedSphereCenter.y;
        const float dz = v.z - m_cachedSphereCenter.z;
        maxDistSq = (std::max)(maxDistSq, dx * dx + dy * dy + dz * dz);
    }

    m_cachedSphereRadius = sqrtf(maxDistSq);
    m_boundsCached = true;

    return true;
}
bool AssimpModelClass::InitializeFromGeometry(
    ID3D11Device* device,
    std::vector<VertexType>&& vertices,
    std::vector<unsigned long>&& indices,
    const XMFLOAT4& baseColor,
    float metallic,
    float roughness)
{
    if (!device || vertices.empty() || indices.size() < 3)
        return false;

    Shutdown();

    m_scene = nullptr;
    m_isEmbeddedFormat = true;
    m_totalIndexCount = static_cast<int>(indices.size());
    m_boundsCached = false;

    MeshPart part;
    part.indexCount = static_cast<int>(indices.size());
    const float safeMetallic = std::clamp(metallic, 0.0f, 1.0f);
    const float safeRoughness = std::clamp(roughness, 0.04f, 1.0f);
    part.texture = Create1x1SRVFromColor(device, aiColor4D(baseColor.x, baseColor.y, baseColor.z, baseColor.w));
    part.metalness = Create1x1SRVFromColor(device, aiColor4D(safeMetallic, safeMetallic, safeMetallic, 1.0f));
    part.roughness = Create1x1SRVFromColor(device, aiColor4D(safeRoughness, safeRoughness, safeRoughness, 1.0f));

    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.ByteWidth = sizeof(VertexType) * static_cast<UINT>(vertices.size());
    vbDesc.Usage = D3D11_USAGE_DEFAULT;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vbData = {};
    vbData.pSysMem = vertices.data();

    HRESULT hr = device->CreateBuffer(&vbDesc, &vbData, &part.vertexBuffer);
    if (FAILED(hr))
    {
        Shutdown();
        return false;
    }

    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.ByteWidth = sizeof(unsigned long) * static_cast<UINT>(indices.size());
    ibDesc.Usage = D3D11_USAGE_DEFAULT;
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA ibData = {};
    ibData.pSysMem = indices.data();

    hr = device->CreateBuffer(&ibDesc, &ibData, &part.indexBuffer);
    if (FAILED(hr))
    {
        if (part.vertexBuffer) part.vertexBuffer->Release();
        if (part.texture) part.texture->Release();
        if (part.metalness) part.metalness->Release();
        if (part.roughness) part.roughness->Release();
        part.vertexBuffer = nullptr;
        part.texture = nullptr;
        part.metalness = nullptr;
        part.roughness = nullptr;
        Shutdown();
        return false;
    }

    XMFLOAT3 bbMin(FLT_MAX, FLT_MAX, FLT_MAX);
    XMFLOAT3 bbMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    for (const auto& vertex : vertices)
    {
        const auto& pos = vertex.position;
        bbMin.x = (std::min)(bbMin.x, pos.x);
        bbMin.y = (std::min)(bbMin.y, pos.y);
        bbMin.z = (std::min)(bbMin.z, pos.z);
        bbMax.x = (std::max)(bbMax.x, pos.x);
        bbMax.y = (std::max)(bbMax.y, pos.y);
        bbMax.z = (std::max)(bbMax.z, pos.z);

        m_cpuVertices.push_back(pos);
    }

    for (size_t i = 0; i + 2 < indices.size(); i += 3)
    {
        if (indices[i] >= vertices.size() ||
            indices[i + 1] >= vertices.size() ||
            indices[i + 2] >= vertices.size())
        {
            continue;
        }

        m_cpuTriangles.push_back(vertices[indices[i]].position);
        m_cpuTriangles.push_back(vertices[indices[i + 1]].position);
        m_cpuTriangles.push_back(vertices[indices[i + 2]].position);
    }

    m_cpuMeshVertices = std::move(vertices);
    m_cpuMeshIndices = std::move(indices);
    m_meshParts.push_back(part);

    m_cachedBBMin = bbMin;
    m_cachedBBMax = bbMax;
    m_cachedSphereCenter = XMFLOAT3(
        (bbMin.x + bbMax.x) * 0.5f,
        (bbMin.y + bbMax.y) * 0.5f,
        (bbMin.z + bbMax.z) * 0.5f);

    float maxDistSq = 0.0f;
    for (const XMFLOAT3& vertex : m_cpuVertices)
    {
        const float dx = vertex.x - m_cachedSphereCenter.x;
        const float dy = vertex.y - m_cachedSphereCenter.y;
        const float dz = vertex.z - m_cachedSphereCenter.z;
        maxDistSq = (std::max)(maxDistSq, dx * dx + dy * dy + dz * dz);
    }

    m_cachedSphereRadius = sqrtf(maxDistSq);
    m_boundsCached = true;

    return true;
}

bool AssimpModelClass::InitializeFromGeometryWithMaterial(
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    const aiScene* scene,
    const aiMaterial* material,
    std::vector<VertexType>&& vertices,
    std::vector<unsigned long>&& indices,
    bool loadImageTextures)
{
    aiColor4D baseColor(0.8f, 0.8f, 0.8f, 1.0f);
    float metallic = 0.0f;
    float roughness = 0.5f;

    if (material)
    {
        baseColor = GetPBRColorFromMaterial(material, aiTextureType_BASE_COLOR);
        aiGetMaterialFloat(material, AI_MATKEY_METALLIC_FACTOR, &metallic);
        if (aiGetMaterialFloat(material, AI_MATKEY_ROUGHNESS_FACTOR, &roughness) != AI_SUCCESS)
            roughness = 0.5f;
    }

    if (!InitializeFromGeometry(
        device,
        std::move(vertices),
        std::move(indices),
        XMFLOAT4(baseColor.r, baseColor.g, baseColor.b, baseColor.a),
        metallic,
        roughness))
    {
        return false;
    }

    if (!material || m_meshParts.empty())
        return true;

    MeshPart& part = m_meshParts.front();
    auto replaceResource = [](ID3D11ShaderResourceView*& destination, ID3D11ShaderResourceView* replacement)
    {
        if (destination)
            destination->Release();
        destination = replacement;
    };

    ID3D11ShaderResourceView* baseColorTexture = nullptr;
    ID3D11ShaderResourceView* normalTexture = nullptr;
    ID3D11ShaderResourceView* metallicTexture = nullptr;
    ID3D11ShaderResourceView* roughnessTexture = nullptr;
    if (loadImageTextures)
    {
        baseColorTexture = ExtractTexture_Internal(
            device, context, scene, material, aiTextureType_BASE_COLOR, aiTextureType_DIFFUSE);
        normalTexture = ExtractTexture_Internal(
            device, context, scene, material, aiTextureType_NORMALS, aiTextureType_HEIGHT);
        metallicTexture = ExtractTexture_Internal(
            device, context, scene, material, aiTextureType_METALNESS, aiTextureType_UNKNOWN);
        roughnessTexture = ExtractTexture_Internal(
            device, context, scene, material, aiTextureType_DIFFUSE_ROUGHNESS, aiTextureType_UNKNOWN);
    }

    if (!baseColorTexture)
        baseColorTexture = Create1x1SRVFromColor(device, baseColor);
    if (!metallicTexture)
        metallicTexture = Create1x1SRVFromColor(device, aiColor4D(metallic, metallic, metallic, 1.0f));
    if (!roughnessTexture)
        roughnessTexture = Create1x1SRVFromColor(device, aiColor4D(roughness, roughness, roughness, 1.0f));

    replaceResource(part.texture, baseColorTexture);
    replaceResource(part.normalMap, normalTexture);
    replaceResource(part.metalness, metallicTexture);
    replaceResource(part.roughness, roughnessTexture);

    aiString alphaMode;
    material->Get(AI_MATKEY_GLTF_ALPHAMODE, alphaMode);
    float opacity = 1.0f;
    float transmission = 0.0f;
    aiGetMaterialFloat(material, AI_MATKEY_OPACITY, &opacity);
    aiGetMaterialFloat(material, AI_MATKEY_TRANSMISSION_FACTOR, &transmission);
    part.isTransparent = std::string(alphaMode.C_Str()) == "BLEND" ||
        opacity < 0.999f || transmission > 0.01f;
    part.opacity = opacity;

    aiString materialName;
    material->Get(AI_MATKEY_NAME, materialName);
    std::string lowerName = materialName.C_Str();
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    float ior = 1.0f;
    aiGetMaterialFloat(material, AI_MATKEY_REFRACTI, &ior);
    part.isGlass = lowerName.find("glass") != std::string::npos ||
        (part.isTransparent && (transmission > 0.01f || (ior > 1.01f && metallic > 0.5f)));

    return true;
}
bool AssimpModelClass::InitializeFromMesh(
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    const aiScene* scene,
    aiMesh* mesh,
    bool loadImageTextures)
{
    if (!device || !context || !scene || !mesh || mesh->mNumVertices == 0)
        return false;

    Shutdown();

    // пїЅпїЅпїЅпїЅпїЅ: пїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ scene пїЅ m_scene. пїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ Assimp::Importer,
    // пїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅ ImportShipFromGlb(). пїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅ.
    m_scene = nullptr;
    m_isEmbeddedFormat = true;
    m_totalIndexCount = 0;
    m_boundsCached = false;

    MeshPart part;
    part.indexCount = static_cast<int>(mesh->mNumFaces) * 3;
    m_totalIndexCount = part.indexCount;

    std::vector<VertexType> vertices;
    vertices.reserve(mesh->mNumVertices);
    m_cpuVertices.reserve(mesh->mNumVertices);
    m_cpuMeshVertices.reserve(mesh->mNumVertices);

    XMFLOAT3 bbMin(FLT_MAX, FLT_MAX, FLT_MAX);
    XMFLOAT3 bbMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    for (unsigned int i = 0; i < mesh->mNumVertices; ++i)
    {
        VertexType v = {};

        v.position = XMFLOAT3(
            mesh->mVertices[i].x,
            mesh->mVertices[i].y,
            mesh->mVertices[i].z);

        v.texture = mesh->HasTextureCoords(0)
            ? XMFLOAT2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y)
            : XMFLOAT2(0.0f, 0.0f);

        v.normal = mesh->HasNormals()
            ? XMFLOAT3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z)
            : XMFLOAT3(0.0f, 1.0f, 0.0f);

        if (mesh->HasTangentsAndBitangents())
        {
            v.tangent = XMFLOAT3(mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z);
            v.binormal = XMFLOAT3(mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z);
        }

        // Ship parts пїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ. пїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅ, пїЅпїЅпїЅпїЅпїЅ shader
        // пїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅ bone matrix пїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ.
        for (int b = 0; b < 4; ++b)
        {
            v.boneIDs[b] = -1;
            v.boneWeights[b] = 0.0f;
        }

        bbMin.x = (std::min)(bbMin.x, v.position.x);
        bbMin.y = (std::min)(bbMin.y, v.position.y);
        bbMin.z = (std::min)(bbMin.z, v.position.z);
        bbMax.x = (std::max)(bbMax.x, v.position.x);
        bbMax.y = (std::max)(bbMax.y, v.position.y);
        bbMax.z = (std::max)(bbMax.z, v.position.z);

        m_cpuVertices.push_back(v.position);
        vertices.push_back(v);
    }

    std::vector<unsigned long> indices;
    indices.reserve(part.indexCount);

    for (unsigned int f = 0; f < mesh->mNumFaces; ++f)
    {
        const aiFace& face = mesh->mFaces[f];
        if (face.mNumIndices != 3)
            continue;

        for (unsigned int j = 0; j < 3; ++j)
        {
            const unsigned int idx = face.mIndices[j];
            indices.push_back(idx);
            m_cpuTriangles.push_back(m_cpuVertices[idx]);
        }
    }

    if (vertices.empty() || indices.empty())
        return false;

    m_cpuMeshVertices = vertices;
    m_cpuMeshIndices = indices;
    part.indexCount = static_cast<int>(indices.size());
    m_totalIndexCount = part.indexCount;

    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.ByteWidth = sizeof(VertexType) * static_cast<UINT>(vertices.size());
    vbDesc.Usage = D3D11_USAGE_DEFAULT;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vbData = {};
    vbData.pSysMem = vertices.data();

    HRESULT hr = device->CreateBuffer(&vbDesc, &vbData, &part.vertexBuffer);
    if (FAILED(hr))
        return false;

    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.ByteWidth = sizeof(unsigned long) * static_cast<UINT>(indices.size());
    ibDesc.Usage = D3D11_USAGE_DEFAULT;
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA ibData = {};
    ibData.pSysMem = indices.data();

    hr = device->CreateBuffer(&ibDesc, &ibData, &part.indexBuffer);
    if (FAILED(hr))
    {
        part.vertexBuffer->Release();
        part.vertexBuffer = nullptr;
        return false;
    }

    // пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅ, пїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅ aiMesh. пїЅпїЅпїЅ пїЅпїЅпїЅ пїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ,
    // пїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅ Blender/GLB.
    if (mesh->mMaterialIndex < scene->mNumMaterials)
    {
        const aiMaterial* mat = scene->mMaterials[mesh->mMaterialIndex];
        if (loadImageTextures)
        {
            part.texture = ExtractTexture_Internal(device, context, scene, mat, aiTextureType_BASE_COLOR, aiTextureType_DIFFUSE);
            part.normalMap = ExtractTexture_Internal(device, context, scene, mat, aiTextureType_NORMALS, aiTextureType_HEIGHT);
            part.metalness = ExtractTexture_Internal(device, context, scene, mat, aiTextureType_METALNESS, aiTextureType_UNKNOWN);
            part.roughness = ExtractTexture_Internal(device, context, scene, mat, aiTextureType_DIFFUSE_ROUGHNESS, aiTextureType_UNKNOWN);
        }

        if (!part.texture)
            part.texture = Create1x1SRVFromColor(device, GetPBRColorFromMaterial(mat, aiTextureType_BASE_COLOR));

        if (!part.metalness)
        {
            float metallic = 0.0f;
            aiGetMaterialFloat(mat, AI_MATKEY_METALLIC_FACTOR, &metallic);
            part.metalness = Create1x1SRVFromColor(device, aiColor4D(metallic, metallic, metallic, 1.0f));
        }

        if (!part.roughness)
        {
            float roughness = 220.0f / 255.0f;
            if (aiGetMaterialFloat(mat, AI_MATKEY_ROUGHNESS_FACTOR, &roughness) != AI_SUCCESS)
                roughness = 220.0f / 255.0f;
            part.roughness = Create1x1SRVFromColor(device, aiColor4D(roughness, roughness, roughness, 1.0f));
        }

        aiString alphaMode;
        mat->Get(AI_MATKEY_GLTF_ALPHAMODE, alphaMode);
        float opacity = 1.0f;
        float transmission = 0.0f;
        aiGetMaterialFloat(mat, AI_MATKEY_OPACITY, &opacity);
        aiGetMaterialFloat(mat, AI_MATKEY_TRANSMISSION_FACTOR, &transmission);
        part.isTransparent = (std::string(alphaMode.C_Str()) == "BLEND") || (opacity < 0.999f) || (transmission > 0.01f);
        part.opacity = opacity;

        float metallicFactor = 0.0f;
        float ior = 1.0f;
        aiGetMaterialFloat(mat, AI_MATKEY_METALLIC_FACTOR, &metallicFactor);
        aiGetMaterialFloat(mat, AI_MATKEY_REFRACTI, &ior);
        aiString matName;
        mat->Get(AI_MATKEY_NAME, matName);
        std::string matNameStr = matName.C_Str();
        std::transform(matNameStr.begin(), matNameStr.end(), matNameStr.begin(), ::tolower);

        part.isGlass = (part.isTransparent && (metallicFactor > 0.5f || transmission > 0.01f)) ||
            matNameStr.find("glass") != std::string::npos ||
            (ior > 1.01f && metallicFactor > 0.5f);
        if (part.isGlass && !part.isTransparent) part.isTransparent = true;
        if (part.isGlass && part.opacity > 0.5f) part.opacity = 0.12f;
    }

        m_meshParts.push_back(part);

    m_cachedBBMin = bbMin;
    m_cachedBBMax = bbMax;
    m_cachedSphereCenter = XMFLOAT3(
        (bbMin.x + bbMax.x) * 0.5f,
        (bbMin.y + bbMax.y) * 0.5f,
        (bbMin.z + bbMax.z) * 0.5f);

    float maxDistSq = 0.0f;
    for (const XMFLOAT3& v : m_cpuVertices)
    {
        const float dx = v.x - m_cachedSphereCenter.x;
        const float dy = v.y - m_cachedSphereCenter.y;
        const float dz = v.z - m_cachedSphereCenter.z;
        maxDistSq = (std::max)(maxDistSq, dx * dx + dy * dy + dz * dz);
    }

    m_cachedSphereRadius = sqrtf(maxDistSq);
    m_boundsCached = true;

    return true;
}

// ============================================================================
// Render пїЅпїЅпїЅпїЅпїЅпїЅ
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

            XMFLOAT4 noLights[NUM_LIGHTS] = {};
            XMFLOAT4 noSpotData[NUM_LIGHTS] = {};
            shader->Render(deviceContext, part.indexCount, world, view, proj,
                part.texture, part.normalMap, part.metalness, part.roughness,
                noLights, noLights, noSpotData, noSpotData,
                XMFLOAT3(0, 0, 0), 1.0f, 1.0f);
        }
        else {
            deviceContext->DrawIndexed(part.indexCount, 0, 0);
        }
    }
}

// ============================================================================
// пїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅ
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
                bbMin.x = (std::min)(bbMin.x, v.x); bbMin.y = (std::min)(bbMin.y, v.y); bbMin.z = (std::min)(bbMin.z, v.z);
                bbMax.x = max(bbMax.x, v.x); bbMax.y = max(bbMax.y, v.y); bbMax.z = max(bbMax.z, v.z);
            }
        }
    }
    m_cachedBBMin = bbMin; m_cachedBBMax = bbMax; m_boundsCached = true;
    *min = bbMin; *max = bbMax;
}

int AssimpModelClass::FindAnimationIndexByName(const std::string& name) const
{
    for (size_t i = 0; i < m_animations.size(); ++i)
    {
        const std::string& candidate = m_animations[i].name;
        const bool matches = candidate.size() == name.size() &&
            std::equal(
                candidate.begin(), candidate.end(), name.begin(),
                [](unsigned char left, unsigned char right)
                {
                    return std::tolower(left) == std::tolower(right);
                });
        if (matches)
            return static_cast<int>(i);
    }

    return -1;
}

int AssimpModelClass::FindAnimationIndexBySemanticName(const std::string& name) const
{
    const int exactMatch = FindAnimationIndexByName(name);
    if (exactMatch >= 0 || name.empty())
        return exactMatch;

    // GLB files commonly namespace clips with a character name. Match only a
    // complete final token: Kevin_Walk matches Walk, but Walking does not.
    std::string suffix = "_";
    suffix += name;
    std::transform(suffix.begin(), suffix.end(), suffix.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

    int bestMatch = -1;
    size_t bestLength = std::string::npos;
    for (size_t i = 0; i < m_animations.size(); ++i)
    {
        std::string candidate = m_animations[i].name;
        std::transform(candidate.begin(), candidate.end(), candidate.begin(),
            [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

        if (candidate.size() > suffix.size() &&
            candidate.ends_with(suffix) &&
            candidate.size() < bestLength)
        {
            bestMatch = static_cast<int>(i);
            bestLength = candidate.size();
        }
    }

    // "Walk" should resolve to "Kevin_Walk" before a more specific
    // "Kevin_Melee_Guard_Walk", regardless of the clips' import order.
    return bestMatch;
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

    // пїЅпїЅпїЅпїЅпїЅпїЅ, пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅ aiMesh пїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅ, пїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅ m_scene,
    // пїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ CPU-пїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅ.
    if (!m_cpuVertices.empty()) {
        out = m_cpuVertices;
        return;
    }

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

    if (!m_cpuMeshVertices.empty() && !m_cpuMeshIndices.empty()) {
        out.reserve(m_cpuMeshIndices.size());
        for (unsigned long index : m_cpuMeshIndices) {
            if (index < m_cpuMeshVertices.size())
                out.push_back(m_cpuMeshVertices[index].position);
        }
        return;
    }

    if (!m_cpuTriangles.empty()) {
        out = m_cpuTriangles;
        return;
    }

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
// пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅ пїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ
// ============================================================================
#include "Animation/TwoBoneIK.h"

namespace
{
    bool EvaluateIKTargetCorrection(const AnimationComponent& animation,
        const std::string& clip, const char* control, int variant, float time,
        DirectX::XMFLOAT3& position, DirectX::XMFLOAT3& rotation)
    {
        bool hasVariantTrack = false;
        if (variant >= 0)
            for (const auto& key : animation.poseKeys)
                if (key.variant == variant && key.clip == clip && key.bone == control)
                {
                    hasVariantTrack = true;
                    break;
                }

        const int activeVariant = hasVariantTrack ? variant : -1;
        const AnimationPoseKey* before = nullptr;
        const AnimationPoseKey* after = nullptr;
        for (const auto& key : animation.poseKeys)
        {
            if (key.variant != activeVariant || key.clip != clip ||
                key.bone != control) continue;
            if (key.time <= time && (!before || key.time > before->time))
                before = &key;
            if (key.time >= time && (!after || key.time < after->time))
                after = &key;
        }
        if (!before) before = after;
        if (!after) after = before;
        if (!before || !after) return false;

        const float span = after->time - before->time;
        const float alpha = span > 0.00001f
            ? std::clamp((time - before->time) / span, 0.0f, 1.0f) : 0.0f;
        const auto lerp = [alpha](float a, float b)
            { return a + (b - a) * alpha; };
        const auto lerpAngle = [alpha](float a, float b)
        {
            float delta = std::fmod(b - a + 180.0f, 360.0f);
            if (delta < 0.0f) delta += 360.0f;
            return a + (delta - 180.0f) * alpha;
        };
        position = {
            lerp(before->position.x, after->position.x),
            lerp(before->position.y, after->position.y),
            lerp(before->position.z, after->position.z) };
        rotation = {
            lerpAngle(before->rotation.x, after->rotation.x),
            lerpAngle(before->rotation.y, after->rotation.y),
            lerpAngle(before->rotation.z, after->rotation.z) };
        return true;
    }
}

void AssimpModelClass::UpdateAnimation(AnimationComponent& animComp, float deltaTime)
{
    if (!animComp.isPlaying) return;
    if (m_animations.empty() || animComp.animationIndex < 0 || animComp.animationIndex >= (int)m_animations.size()) return;

    const AnimationClip& anim = m_animations[animComp.animationIndex];
    // A single-frame stance is a valid pose and must be sampled on every equip.

    float ticksPerSecond = (anim.ticksPerSecond > 0.0f) ? anim.ticksPerSecond : 24.0f;

    // 1. пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅ
    animComp.currentTime += deltaTime * animComp.GetClipSpeed(anim.name);

    // 2. пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ
    float animDurationSec = anim.duration / ticksPerSecond;
    if (animDurationSec > 0.0f) {
        animComp.currentTime = fmodf(animComp.currentTime, animDurationSec);
        if (animComp.currentTime < 0.0f) animComp.currentTime += animDurationSec;
    }
    else {
        animComp.currentTime = 0.0f;
    }

    // 3. пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅ пїЅпїЅпїЅпїЅ пїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ (пїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ!)
    float animTimeTicks = animComp.currentTime * ticksPerSecond;
    animTimeTicks = anim.duration > 0.001f ? fmodf(animTimeTicks, anim.duration) : 0.0f;
    if (animTimeTicks < 0.0f) animTimeTicks += anim.duration;

    // 4. ?? пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ: пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ, пїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅ
    size_t boneCount = m_boneInfoMap.size();
    if (animComp.finalBoneMatrices.size() != boneCount) {
        animComp.finalBoneMatrices.resize(boneCount);
    }

    // пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ
    XMFLOAT4X4 identity; XMStoreFloat4x4(&identity, XMMatrixIdentity());
    for (size_t i = 0; i < boneCount; ++i) {
        animComp.finalBoneMatrices[i] = identity;
    }

    // 5. пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅ
    if (m_scene && m_scene->mRootNode) {
        const AnimationClip* upper = GetAnimation(animComp.upperBodyAnimationIndex);
        float upperTicks = 0.0f;
        if (upper) {
            animComp.upperBodyTime += deltaTime * animComp.GetClipSpeed(upper->name);
            const float rate = upper->ticksPerSecond > 0.0f ? upper->ticksPerSecond : 24.0f;
            const float duration = upper->duration / rate;
            if (animComp.socketPreviewEnabled && !animComp.socketPreviewPlaying)
                animComp.upperBodyTime = !animComp.socketPreviewPunch &&
                    animComp.socketPreviewBlendEnabled
                    ? animComp.socketPreviewUpperTime
                    : animComp.socketPreviewTime;
            animComp.upperBodyTime = duration > 0.00001f ? fmodf(animComp.upperBodyTime, duration) : 0.0f;
            upperTicks = animComp.upperBodyTime * rate;
        }
        ReadNodeHierarchyToComponent(m_scene->mRootNode, XMMatrixIdentity(), anim,
            animTimeTicks, animComp.finalBoneMatrices, upper, upperTicks,
            animComp.upperBodyRoot, false, &animComp.poseKeys,
            &animComp.fistKeys,
            animComp.poseVariant);

        // Apply authored hand and elbow targets after ordinary bone sampling.
        // The hand target determines reach; the pole target determines which
        // way the elbow points. Neither control translates a deform bone.
        const AnimationClip& armClip = upper ? *upper : anim;
        const float armTime = upper ? animComp.upperBodyTime : animComp.currentTime;
        const auto applyArmIK = [&](bool rightHand)
        {
            const char* handControl = rightHand
                ? kAnimationRightHandIKControl : kAnimationLeftHandIKControl;
            const char* poleControl = rightHand
                ? kAnimationRightElbowPoleControl : kAnimationLeftElbowPoleControl;
            XMFLOAT3 handPosition{};
            XMFLOAT3 handRotation{};
            XMFLOAT3 polePosition{};
            XMFLOAT3 poleRotation{};
            const bool hasHandTarget = EvaluateIKTargetCorrection(animComp,
                armClip.name, handControl, animComp.poseVariant, armTime,
                handPosition, handRotation);
            const bool hasPoleTarget = EvaluateIKTargetCorrection(animComp,
                armClip.name, poleControl, animComp.poseVariant, armTime,
                polePosition, poleRotation);
            if (!hasHandTarget && !hasPoleTarget)
                return;

            std::string upperArm;
            std::string forearm;
            std::string hand;
            XMMATRIX baseHand;
            XMMATRIX baseElbow;
            XMMATRIX ignored;
            if (!TryGetArmIKChain(rightHand, upperArm, forearm, hand) ||
                !TryGetAnimationNodeEditTransforms(animComp, hand, baseHand, ignored) ||
                !TryGetAnimationNodeEditTransforms(animComp, forearm, baseElbow, ignored))
                return;

            XMMATRIX target = baseHand;
            if (hasHandTarget)
            {
                const XMMATRIX correction = XMMatrixRotationRollPitchYaw(
                    XMConvertToRadians(handRotation.x),
                    XMConvertToRadians(handRotation.y),
                    XMConvertToRadians(handRotation.z)) *
                    XMMatrixTranslation(handPosition.x, handPosition.y,
                        handPosition.z);
                target = correction * baseHand;
            }

            XMVECTOR poleTarget{};
            const XMVECTOR* pole = nullptr;
            if (hasPoleTarget)
            {
                // Pole rotation is intentionally ignored: only its position
                // defines the bend plane.
                const XMMATRIX correction = XMMatrixTranslation(
                    polePosition.x, polePosition.y, polePosition.z);
                poleTarget = (correction * baseElbow).r[3];
                pole = &poleTarget;
            }
            ApplyTwoBoneIK(animComp, upperArm, forearm, hand,
                target, pole);
        };
        applyArmIK(false);
        applyArmIK(true);

        // A keyed virtual target turns the head after the composed base/upper
        // pose has been sampled. The target is additive to the animated head,
        // so the same keys remain useful on blended and full-body previews.
        XMFLOAT3 headTargetOffset{};
        XMFLOAT3 ignoredHeadRotation{};
        if (EvaluateIKTargetCorrection(animComp, armClip.name,
                kAnimationHeadLookIKControl, animComp.poseVariant, armTime,
                headTargetOffset, ignoredHeadRotation))
        {
            std::string headBone;
            XMMATRIX animatedHead;
            XMMATRIX ignoredHead;
            if (TryGetHeadIKBone(headBone) &&
                TryGetAnimationNodeEditTransforms(animComp, headBone,
                    animatedHead, ignoredHead))
            {
                const XMMATRIX targetTransform = XMMatrixTranslation(
                    headTargetOffset.x, headTargetOffset.y,
                    headTargetOffset.z) * animatedHead;
                ApplyHeadLookIK(animComp, headBone,
                    targetTransform.r[3]);
            }
        }
    }
    
}

void AssimpModelClass::SetBoneMatricesFromComponent(ID3D11DeviceContext* context, const AnimationComponent& animComp) {
    if (!context || animComp.finalBoneMatrices.empty() || !m_boneMatrixBuffer) return;

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    HRESULT hr = context->Map(m_boneMatrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (FAILED(hr)) return;

    auto* dst = reinterpret_cast<XMFLOAT4X4*>(mapped.pData);
    XMFLOAT4X4 identity;
    DirectX::XMStoreFloat4x4(&identity, DirectX::XMMatrixIdentity());

    // пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅ 128 пїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ, пїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅ пїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅ
    // пїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ/пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅ.
    for (int i = 0; i < 128; ++i) {
        dst[i] = (i < (int)animComp.finalBoneMatrices.size()) ? animComp.finalBoneMatrices[i]
            : identity;
    }

    context->Unmap(m_boneMatrixBuffer, 0);
    context->VSSetConstantBuffers(8, 1, &m_boneMatrixBuffer);

#if DEBUG_TRANSFORMS
    // пїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ
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
    std::vector<DirectX::XMFLOAT4X4>& outMatrices,
    const AnimationClip* upperClip, float upperTime,
    const std::string& upperRoot, bool inUpperBody,
    const std::vector<AnimationPoseKey>* poseKeys,
    const std::vector<AnimationFistKey>* fistKeys, int poseVariant) const
{
    if (outMatrices.empty()) {
        return;
    }

    // пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅ
    std::string rawName = (node->mName.length > 0) ? std::string(node->mName.C_Str(), node->mName.length) : "";
    std::string nodeName = NormalizeBoneName(rawName);

    // пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ
    XMFLOAT4X4 nodeLocalFloat = ConvertMatrix(node->mTransformation);
    DirectX::XMMATRIX nodeTransform = DirectX::XMLoadFloat4x4(&nodeLocalFloat);

    // пїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ
    const BoneAnimationChannel* channel = nullptr;
    inUpperBody = inUpperBody || (upperClip && nodeName == NormalizeBoneName(upperRoot));
    const auto& sampledClip = inUpperBody && upperClip ? *upperClip : anim;
    const float sampleTime = inUpperBody && upperClip ? upperTime : animTime;
    for (const auto& ch : sampledClip.channels) {
        if (ch.nodeName == nodeName) {
            channel = &ch;
            break;
        }
    }

    // пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ
    if (channel) {
        XMMATRIX posMat = (channel->positions.size() >= 2) ? InterpolatePosition(channel->positions, sampleTime) :
            (channel->positions.empty() ? XMMatrixIdentity() : XMMatrixTranslation(channel->positions[0].position.x, channel->positions[0].position.y, channel->positions[0].position.z));
        XMMATRIX rotMat = (channel->rotations.size() >= 2) ? InterpolateRotation(channel->rotations, sampleTime) :
            (channel->rotations.empty() ? XMMatrixIdentity() : XMMatrixRotationQuaternion(XMLoadFloat4(&channel->rotations[0].orientation)));
        XMMATRIX scaleMat = (channel->scales.size() >= 2) ? InterpolateScale(channel->scales, sampleTime) :
            (channel->scales.empty() ? XMMatrixIdentity() : XMMatrixScaling(channel->scales[0].scale.x, channel->scales[0].scale.y, channel->scales[0].scale.z));

        nodeTransform = scaleMat * rotMat * posMat;
    }

    const float fistRate = sampledClip.ticksPerSecond > 0.0f
        ? sampledClip.ticksPerSecond : 24.0f;
    nodeTransform = FistCorrectionForBone(nodeName, fistKeys,
        sampledClip.name, poseVariant, sampleTime / fistRate) * nodeTransform;

    if (poseKeys && !poseKeys->empty())
    {
        // Ключи JSON добавляют поправку к исходному GLB; соседние ключи
        // интерполируются линейно, вне диапазона держится крайнее значение.
        const float rate = sampledClip.ticksPerSecond > 0.0f
            ? sampledClip.ticksPerSecond : 24.0f;
        const float seconds = sampleTime / rate;
        bool hasVariantKeys = false;
        if (poseVariant >= 0)
            for (const auto& key : *poseKeys)
                if (key.variant == poseVariant && key.clip == sampledClip.name &&
                    NormalizeBoneName(key.bone) == nodeName)
                {
                    hasVariantKeys = true;
                    break;
                }
        const AnimationPoseKey* before = nullptr;
        const AnimationPoseKey* after = nullptr;
        for (const auto& key : *poseKeys)
        {
            if (key.variant != (hasVariantKeys ? poseVariant : -1) ||
                key.clip != sampledClip.name ||
                NormalizeBoneName(key.bone) != nodeName) continue;
            if (key.time <= seconds && (!before || key.time > before->time))
                before = &key;
            if (key.time >= seconds && (!after || key.time < after->time))
                after = &key;
        }
        if (!before) before = after;
        if (!after) after = before;
        if (before && after)
        {
            const float span = after->time - before->time;
            const float alpha = span > 0.00001f
                ? std::clamp((seconds - before->time) / span, 0.0f, 1.0f) : 0.0f;
            const auto lerp = [alpha](float a, float b)
                { return a + (b - a) * alpha; };
            const auto lerpAngle = [alpha](float a, float b)
            {
                float delta = std::fmod(b - a + 180.0f, 360.0f);
                if (delta < 0.0f) delta += 360.0f;
                return a + (delta - 180.0f) * alpha;
            };
            const auto& p = before->position;
            const auto& q = after->position;
            const auto& r = before->rotation;
            const auto& s = after->rotation;
            nodeTransform = DirectX::XMMatrixRotationRollPitchYaw(
                DirectX::XMConvertToRadians(lerpAngle(r.x, s.x)),
                DirectX::XMConvertToRadians(lerpAngle(r.y, s.y)),
                DirectX::XMConvertToRadians(lerpAngle(r.z, s.z))) *
                DirectX::XMMatrixTranslation(lerp(p.x, q.x),
                    lerp(p.y, q.y), lerp(p.z, q.z)) * nodeTransform;
        }
    }

    DirectX::XMMATRIX globalTransform = nodeTransform * parentTransform;

    // ?? пїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅ пїЅ пїЅ пїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ
    auto boneIt = m_boneInfoMap.find(nodeName);

    // пїЅпїЅпїЅпїЅпїЅпїЅпїЅ 2: пїЅпїЅпїЅпїЅ пїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅ
    if (boneIt == m_boneInfoMap.end() && !rawName.empty()) {
        boneIt = m_boneInfoMap.find(rawName);
    }

    // пїЅпїЅпїЅпїЅпїЅпїЅпїЅ 3: пїЅпїЅпїЅпїЅ пїЅпїЅ пїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅ
    if (boneIt == m_boneInfoMap.end() && channel) {
        boneIt = m_boneInfoMap.find(channel->nodeName);
    }

    if(boneIt != m_boneInfoMap.end()) {
        // === пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ ===
        int boneIndex = boneIt->second.boneIndex;
        if (boneIndex >= 0 && boneIndex < (int)outMatrices.size()) {
            DirectX::XMMATRIX offsetMat = DirectX::XMLoadFloat4x4(&boneIt->second.offsetMatrix);
            DirectX::XMMATRIX finalMat = offsetMat * globalTransform;
            DirectX::XMStoreFloat4x4(&outMatrices[boneIndex], finalMat);
        }
    }

    // Animated hierarchy nodes without mesh weights (for example Mixamo End
    // bones) still affect their children through globalTransform, but must not
    // overwrite an unrelated skinning slot. Slot 0 belongs to a real weighted
    // bone and corrupting it stretches every vertex influenced by that bone.

    // пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ
    for (unsigned int i = 0; i < node->mNumChildren; ++i) {
        ReadNodeHierarchyToComponent(node->mChildren[i], globalTransform, anim, animTime, outMatrices,
            upperClip, upperTime, upperRoot, inUpperBody, poseKeys, fistKeys,
            poseVariant);
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
// пїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅ пїЅпїЅпїЅпїЅпїЅпїЅ (пїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅ)
// ============================================================================
void AssimpModelClass::PrintModelDebugInfo() {
#if DEBUG_VERBOSE
    OutputDebugStringA("\n========== MODEL DEBUG INFO ==========\n");

    // пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅ пїЅпїЅпїЅпїЅпїЅ
    if (m_scene) {
        OutputDebugStringA(("Scene: meshes=" + std::to_string(m_scene->mNumMeshes) +
            ", materials=" + std::to_string(m_scene->mNumMaterials) +
            ", textures=" + std::to_string(m_scene->mNumTextures) +
            ", animations=" + std::to_string(m_scene->mNumAnimations) + "\n").c_str());

        // пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅ
        if (m_scene->mRootNode) {
            OutputDebugStringA(("Root node: '" + std::string(m_scene->mRootNode->mName.C_Str()) +
                "', children=" + std::to_string(m_scene->mRootNode->mNumChildren) + "\n").c_str());
        }
    }

    // пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅ пїЅпїЅпїЅпїЅпїЅпїЅ
    OutputDebugStringA(("Bones: " + std::to_string(m_boneInfoMap.size()) + "\n").c_str());
    for (const auto& [name, info] : m_boneInfoMap) {
        OutputDebugStringA(("  Bone[" + std::to_string(info.boneIndex) + "]: '" + name + "'\n").c_str());
    }

    // пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ
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

    // пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅ пїЅпїЅпїЅпїЅ
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
// ?? пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅ
// ============================================================================
void AssimpModelClass::DebugAnimationBinding(const AnimationClip& anim) {
    OutputDebugStringA("\n========== ANIMATION BINDING DEBUG ==========\n");

    // 1. пїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅ
    OutputDebugStringA("--- Registered Bones ---\n");
    for (const auto& [name, info] : m_boneInfoMap) {
        OutputDebugStringA(("  BONE[" + std::to_string(info.boneIndex) + "]: '" + name + "'\n").c_str());
    }

    // 2. пїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅ пїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ
    OutputDebugStringA("--- Animation Channels Binding ---\n");
    int bound = 0, unbound = 0;

    for (size_t c = 0; c < anim.channels.size(); ++c) {
        const auto& ch = anim.channels[c];
        auto it = m_boneInfoMap.find(ch.nodeName);

        if (it != m_boneInfoMap.end()) {
            OutputDebugStringA(("  ? CH[" + std::to_string(c) + "] '" + ch.nodeName +
                "' -> BONE[" + std::to_string(it->second.boneIndex) + "]\n").c_str());
            bound++;
        }
        else {
            OutputDebugStringA(("  ? CH[" + std::to_string(c) + "] '" + ch.nodeName +
                "' -> NOT FOUND in bone map!\n").c_str());
            unbound++;
        }
    }

    OutputDebugStringA(("Summary: bound=" + std::to_string(bound) +
        ", unbound=" + std::to_string(unbound) + "\n").c_str());

    // 3. пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅ
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

bool AssimpModelClass::TryGetBoneModelTransform(
    const AnimationComponent& animComp,
    const std::string& boneName,
    DirectX::XMMATRIX& outTransform) const
{
    auto boneIt = m_boneInfoMap.find(NormalizeBoneName(boneName));
    if (boneIt == m_boneInfoMap.end())
        boneIt = m_boneInfoMap.find(boneName);
    if (boneIt == m_boneInfoMap.end())
        return false;

    const BoneInfo& bone = boneIt->second;
    if (bone.boneIndex < 0 ||
        bone.boneIndex >= static_cast<int>(animComp.finalBoneMatrices.size()))
    {
        return false;
    }

    // Skinning stores offset * animatedGlobal for the row-vector convention
    // used by this renderer. Remove the inverse-bind offset to recover the
    // animated bone transform in model space.
    const XMMATRIX offset = XMLoadFloat4x4(&bone.offsetMatrix);
    const XMMATRIX skinning = XMLoadFloat4x4(
        &animComp.finalBoneMatrices[bone.boneIndex]);
    outTransform = XMMatrixInverse(nullptr, offset) * skinning;
    return true;
}

bool AssimpModelClass::TryGetAnimationNodeEditTransforms(
    const AnimationComponent& animComp,
    const std::string& requestedNode,
    DirectX::XMMATRIX& outCurrentTransform,
    DirectX::XMMATRIX& outUncorrectedTransform) const
{
    if (!m_scene || !m_scene->mRootNode || requestedNode.empty() ||
        animComp.animationIndex < 0 ||
        animComp.animationIndex >= static_cast<int>(m_animations.size()))
        return false;

    const AnimationClip& baseClip = m_animations[animComp.animationIndex];
    const AnimationClip* upperClip = GetAnimation(animComp.upperBodyAnimationIndex);
    const std::string wanted = NormalizeBoneName(requestedNode);
    const auto clipTicks = [](const AnimationClip& clip, float seconds)
    {
        const float rate = clip.ticksPerSecond > 0.0f ? clip.ticksPerSecond : 24.0f;
        float ticks = seconds * rate;
        if (clip.duration > 0.001f)
            ticks = std::fmod(ticks, clip.duration);
        return ticks < 0.0f ? ticks + clip.duration : ticks;
    };
    const float baseTime = clipTicks(baseClip, animComp.currentTime);
    const float upperTime = upperClip
        ? clipTicks(*upperClip, animComp.upperBodyTime) : 0.0f;

    bool found = false;
    auto visit = [&](auto&& self, const aiNode* node,
        const XMMATRIX& parentTransform, bool inUpperBody) -> void
    {
        if (found || !node) return;
        const std::string rawName = node->mName.length > 0
            ? std::string(node->mName.C_Str(), node->mName.length) : std::string{};
        const std::string nodeName = NormalizeBoneName(rawName);
        inUpperBody = inUpperBody || (upperClip &&
            nodeName == NormalizeBoneName(animComp.upperBodyRoot));
        const AnimationClip& sampledClip = inUpperBody && upperClip
            ? *upperClip : baseClip;
        const float sampleTime = inUpperBody && upperClip ? upperTime : baseTime;

        XMFLOAT4X4 localFloat = ConvertMatrix(node->mTransformation);
        XMMATRIX baseLocal = XMLoadFloat4x4(&localFloat);
        const BoneAnimationChannel* channel = nullptr;
        for (const auto& candidate : sampledClip.channels)
            if (candidate.nodeName == nodeName)
            {
                channel = &candidate;
                break;
            }
        if (channel)
        {
            const XMMATRIX position = channel->positions.size() >= 2
                ? InterpolatePosition(channel->positions, sampleTime)
                : channel->positions.empty() ? XMMatrixIdentity()
                : XMMatrixTranslation(channel->positions[0].position.x,
                    channel->positions[0].position.y,
                    channel->positions[0].position.z);
            const XMMATRIX rotation = channel->rotations.size() >= 2
                ? InterpolateRotation(channel->rotations, sampleTime)
                : channel->rotations.empty() ? XMMatrixIdentity()
                : XMMatrixRotationQuaternion(
                    XMLoadFloat4(&channel->rotations[0].orientation));
            const XMMATRIX scale = channel->scales.size() >= 2
                ? InterpolateScale(channel->scales, sampleTime)
                : channel->scales.empty() ? XMMatrixIdentity()
                : XMMatrixScaling(channel->scales[0].scale.x,
                    channel->scales[0].scale.y,
                    channel->scales[0].scale.z);
            baseLocal = scale * rotation * position;
        }

        XMMATRIX correction = XMMatrixIdentity();
        const float rate = sampledClip.ticksPerSecond > 0.0f
            ? sampledClip.ticksPerSecond : 24.0f;
        const float seconds = sampleTime / rate;
        bool hasVariantKeys = false;
        if (animComp.poseVariant >= 0)
            for (const auto& key : animComp.poseKeys)
                if (key.variant == animComp.poseVariant &&
                    key.clip == sampledClip.name &&
                    NormalizeBoneName(key.bone) == nodeName)
                {
                    hasVariantKeys = true;
                    break;
                }
        const int activeVariant = hasVariantKeys ? animComp.poseVariant : -1;
        const AnimationPoseKey* before = nullptr;
        const AnimationPoseKey* after = nullptr;
        for (const auto& key : animComp.poseKeys)
        {
            if (key.variant != activeVariant || key.clip != sampledClip.name ||
                NormalizeBoneName(key.bone) != nodeName) continue;
            if (key.time <= seconds && (!before || key.time > before->time))
                before = &key;
            if (key.time >= seconds && (!after || key.time < after->time))
                after = &key;
        }
        if (!before) before = after;
        if (!after) after = before;
        if (before && after)
        {
            const float span = after->time - before->time;
            const float alpha = span > 0.00001f
                ? std::clamp((seconds - before->time) / span, 0.0f, 1.0f) : 0.0f;
            const auto lerp = [alpha](float a, float b)
                { return a + (b - a) * alpha; };
            const auto lerpAngle = [alpha](float a, float b)
            {
                float delta = std::fmod(b - a + 180.0f, 360.0f);
                if (delta < 0.0f) delta += 360.0f;
                return a + (delta - 180.0f) * alpha;
            };
            const auto& p = before->position;
            const auto& q = after->position;
            const auto& r = before->rotation;
            const auto& s = after->rotation;
            correction = XMMatrixRotationRollPitchYaw(
                XMConvertToRadians(lerpAngle(r.x, s.x)),
                XMConvertToRadians(lerpAngle(r.y, s.y)),
                XMConvertToRadians(lerpAngle(r.z, s.z))) *
                XMMatrixTranslation(lerp(p.x, q.x),
                    lerp(p.y, q.y), lerp(p.z, q.z));
        }

        const XMMATRIX fistCorrection = FistCorrectionForBone(nodeName,
            &animComp.fistKeys, sampledClip.name, animComp.poseVariant, seconds);
        const XMMATRIX uncorrected = fistCorrection * baseLocal * parentTransform;
        const XMMATRIX current = correction * uncorrected;
        if (nodeName == wanted || rawName == requestedNode)
        {
            outCurrentTransform = current;
            outUncorrectedTransform = uncorrected;
            found = true;
            return;
        }
        for (unsigned int i = 0; i < node->mNumChildren; ++i)
            self(self, node->mChildren[i], current, inUpperBody);
    };
    visit(visit, m_scene->mRootNode, XMMatrixIdentity(), false);
    return found;
}

bool AssimpModelClass::TryGetArmIKChain(bool rightHand,
    std::string& upperArm, std::string& forearm, std::string& hand) const
{
    const char* side = rightHand ? "Right" : "Left";
    const auto findBone = [&](const std::string& part, std::string& result)
    {
        const std::string plain = side + part;
        const std::string mixamo = "mixamorig:" + plain;
        auto exact = m_boneInfoMap.find(mixamo);
        if (exact != m_boneInfoMap.end())
        {
            result = exact->first;
            return true;
        }
        exact = m_boneInfoMap.find(plain);
        if (exact != m_boneInfoMap.end())
        {
            result = exact->first;
            return true;
        }

        // Importers may preserve a different namespace. Match only the final
        // namespace component so LeftArm cannot be confused with LeftForeArm.
        for (const auto& [name, info] : m_boneInfoMap)
        {
            (void)info;
            const std::string normalized = NormalizeBoneName(name);
            const size_t separator = normalized.find_last_of(':');
            const std::string leaf = separator == std::string::npos
                ? normalized : normalized.substr(separator + 1);
            if (leaf == plain)
            {
                result = name;
                return true;
            }
        }
        return false;
    };

    return findBone("Arm", upperArm) &&
        findBone("ForeArm", forearm) && findBone("Hand", hand);
}

bool AssimpModelClass::TryGetHeadIKBone(std::string& head) const
{
    const auto findLeaf = [&](const std::string& plain, std::string& result)
    {
        const std::string mixamo = "mixamorig:" + plain;
        auto exact = m_boneInfoMap.find(mixamo);
        if (exact != m_boneInfoMap.end())
        {
            result = exact->first;
            return true;
        }
        exact = m_boneInfoMap.find(plain);
        if (exact != m_boneInfoMap.end())
        {
            result = exact->first;
            return true;
        }
        for (const auto& [name, info] : m_boneInfoMap)
        {
            (void)info;
            const std::string normalized = NormalizeBoneName(name);
            const size_t separator = normalized.find_last_of(':');
            const std::string leaf = separator == std::string::npos
                ? normalized : normalized.substr(separator + 1);
            if (leaf == plain)
            {
                result = name;
                return true;
            }
        }
        return false;
    };
    return findLeaf("Head", head);
}

bool AssimpModelClass::ApplyHeadLookIK(AnimationComponent& animation,
    const std::string& head, const XMVECTOR& modelTarget,
    float maxAngleDegrees) const
{
    XMMATRIX headTransform;
    if (!m_scene || !TryGetBoneModelTransform(animation, head, headTransform))
        return false;

    const XMVECTOR pivot = headTransform.r[3];
    const XMVECTOR toTarget = XMVectorSubtract(modelTarget, pivot);
    if (XMVectorGetX(XMVector3LengthSq(toTarget)) < 0.000001f)
        return false;

    // Kevin/Mixamo faces model -Z. Transforming that axis by the animated head
    // preserves authored nods and turns before applying the additional look.
    XMVECTOR currentForward = XMVector3TransformNormal(
        XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f), headTransform);
    if (XMVectorGetX(XMVector3LengthSq(currentForward)) < 0.000001f)
        currentForward = XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f);
    currentForward = XMVector3Normalize(currentForward);
    const XMVECTOR desiredForward = XMVector3Normalize(toTarget);
    const float dot = std::clamp(XMVectorGetX(
        XMVector3Dot(currentForward, desiredForward)), -1.0f, 1.0f);
    float angle = std::acos(dot);
    angle = (std::min)(angle, XMConvertToRadians(
        std::clamp(maxAngleDegrees, 0.0f, 180.0f)));
    if (angle < 0.00001f)
        return true;

    XMVECTOR axis = XMVector3Cross(currentForward, desiredForward);
    if (XMVectorGetX(XMVector3LengthSq(axis)) < 0.000001f)
    {
        axis = XMVector3Cross(currentForward,
            XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
        if (XMVectorGetX(XMVector3LengthSq(axis)) < 0.000001f)
            axis = XMVector3Cross(currentForward,
                XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f));
    }
    const XMMATRIX delta = XMMatrixTranslationFromVector(XMVectorNegate(pivot)) *
        XMMatrixRotationAxis(XMVector3Normalize(axis), angle) *
        XMMatrixTranslationFromVector(pivot);

    const std::string normalizedRoot = NormalizeBoneName(head);
    auto visit = [&](auto&& self, const aiNode* node, bool inside) -> void
    {
        const std::string name = NormalizeBoneName(node->mName.C_Str());
        inside = inside || name == normalizedRoot;
        if (inside)
        {
            auto bone = m_boneInfoMap.find(name);
            if (bone != m_boneInfoMap.end())
            {
                const int index = bone->second.boneIndex;
                if (index >= 0 && index <
                    static_cast<int>(animation.finalBoneMatrices.size()))
                {
                    auto& matrix = animation.finalBoneMatrices[index];
                    XMStoreFloat4x4(&matrix,
                        XMLoadFloat4x4(&matrix) * delta);
                }
            }
        }
        for (unsigned int child = 0; child < node->mNumChildren; ++child)
            self(self, node->mChildren[child], inside);
    };
    visit(visit, m_scene->mRootNode, false);
    return true;
}

bool AssimpModelClass::ApplyTwoBoneIK(AnimationComponent& animation,
    const std::string& upperArm, const std::string& forearm, const std::string& hand,
    const XMMATRIX& target, const XMVECTOR* poleTarget) const
{
    XMMATRIX upper, lower, palm;
    if (!m_scene || !TryGetBoneModelTransform(animation, upperArm, upper) ||
        !TryGetBoneModelTransform(animation, forearm, lower) ||
        !TryGetBoneModelTransform(animation, hand, palm)) return false;
    XMVECTOR elbow, wrist;
    if (!TwoBoneIK::Solve(upper.r[3], lower.r[3], palm.r[3], target.r[3],
            elbow, wrist, poleTarget)) return false;

    // Each delta moves a whole subtree, so fingers stay attached to the hand.
    auto moveSubtree = [&](const std::string& root, const XMMATRIX& delta) {
        const std::string normalized = NormalizeBoneName(root);
        auto visit = [&](auto&& self, const aiNode* node, bool inside) -> void {
            const std::string name = NormalizeBoneName(node->mName.C_Str());
            inside = inside || name == normalized;
            if (inside) {
                auto bone = m_boneInfoMap.find(name);
                if (bone != m_boneInfoMap.end()) {
                    const int index = bone->second.boneIndex;
                    if (index >= 0 && index < static_cast<int>(animation.finalBoneMatrices.size())) {
                        auto& matrix = animation.finalBoneMatrices[index];
                        XMStoreFloat4x4(&matrix, XMLoadFloat4x4(&matrix) * delta);
                    }
                }
            }
            for (unsigned int i = 0; i < node->mNumChildren; ++i) self(self, node->mChildren[i], inside);
        };
        visit(visit, m_scene->mRootNode, false);
    };
    const XMVECTOR shoulder = upper.r[3];
    moveSubtree(upperArm, XMMatrixTranslationFromVector(XMVectorNegate(shoulder)) *
        TwoBoneIK::RotationBetween(XMVectorSubtract(lower.r[3], shoulder), XMVectorSubtract(elbow, shoulder)) *
        XMMatrixTranslationFromVector(shoulder));
    TryGetBoneModelTransform(animation, forearm, lower);
    TryGetBoneModelTransform(animation, hand, palm);
    moveSubtree(forearm, XMMatrixTranslationFromVector(XMVectorNegate(lower.r[3])) *
        TwoBoneIK::RotationBetween(XMVectorSubtract(palm.r[3], lower.r[3]), XMVectorSubtract(wrist, lower.r[3])) *
        XMMatrixTranslationFromVector(lower.r[3]));
    TryGetBoneModelTransform(animation, hand, palm);
    XMVECTOR scale, rotation, translation, targetScale, targetRotation, targetTranslation;
    if (XMMatrixDecompose(&scale, &rotation, &translation, palm) &&
        XMMatrixDecompose(&targetScale, &targetRotation, &targetTranslation, target)) {
        const XMMATRIX desired = XMMatrixScalingFromVector(scale) *
            XMMatrixRotationQuaternion(targetRotation) * XMMatrixTranslationFromVector(wrist);
        moveSubtree(hand, XMMatrixInverse(nullptr, palm) * desired);
    }
    TryGetBoneModelTransform(animation, hand, palm);
    animation.leftHandIKError = XMVectorGetX(XMVector3Length(XMVectorSubtract(palm.r[3], target.r[3])));
    return true;
}
