#pragma once
#include <d3d11.h>
#include <DirectXMath.h>
#include <vector>
#include <string>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "TextureClass.h"
#include <unordered_map>
#include <algorithm>

class TextureShaderClass;
class PBRShaderClass;

using namespace DirectX;
using namespace std;

class AssimpModelClass
{
public:
    // ── Один submesh — хранит геометрию + все PBR текстуры из GLB ────────────
    struct MeshPart
    {
        ID3D11Buffer* vertexBuffer = nullptr;
        ID3D11Buffer* indexBuffer  = nullptr;
        int           indexCount   = 0;

        ID3D11ShaderResourceView* texture   = nullptr;
        ID3D11ShaderResourceView* normalMap = nullptr;
        ID3D11ShaderResourceView* metalness = nullptr;
        ID3D11ShaderResourceView* roughness = nullptr;

        bool  isTransparent = false;
        bool  isGlass       = false;   // ← НОВОЕ: стекло из GLB
        float opacity       = 1.0f;
    };

    // ── Вершина — добавили tangent и binormal для PBR/NormalMap ──────────────
    struct VertexType {
        XMFLOAT3 position;
        XMFLOAT2 texture;
        XMFLOAT3 normal;
        XMFLOAT3 tangent;
        XMFLOAT3 binormal;
        int    boneIDs[4];      // Индексы костей (по умолчанию -1)
        float  boneWeights[4];  // Веса (по умолчанию 0.0f)
    };

public:
    AssimpModelClass();
    ~AssimpModelClass();

    bool Initialize(ID3D11Device* device, ID3D11DeviceContext* deviceContext,
        const char* modelPath, const char* textureFilename);
    void Shutdown();


    void Render(ID3D11DeviceContext* deviceContext);
    void RenderAll(ID3D11DeviceContext* deviceContext,
        PBRShaderClass* shader,
        XMMATRIX world, XMMATRIX view, XMMATRIX proj);

    static aiColor4D GetPBRColorFromMaterial(const aiMaterial* mat, aiTextureType type);
    static ID3D11ShaderResourceView* Create1x1SRVFromColor(ID3D11Device* device, const aiColor4D& color);

    int                       GetIndexCount();
    ID3D11ShaderResourceView* GetTexture();

    const std::vector<MeshPart>& GetMeshParts() const { return m_meshParts; }

    std::vector<TextureClass*> GetOwnedTextures() const { return m_ownedTextures; }

    std::vector<TextureClass*> m_ownedTextures;


    void GetBoundingSphere(XMFLOAT3* center, float* radius) const;
    void GetVertices(std::vector<XMFLOAT3>& outVertices) const;
    void GetRawVertices(std::vector<XMFLOAT3>& outVertices) const;
    void GetRawTriangles(std::vector<XMFLOAT3>& outTriangles) const;
    void LoadBones(const aiScene* scene);
    void LoadAnimations(const aiScene* scene);
    void GetBoundingBox(XMFLOAT3* min, XMFLOAT3* max) const;
    bool IsEmbeddedFormat() const { return m_isEmbeddedFormat; }


    //ANIMATION
     // ── Ключевые кадры ────────────────────────────────────────────────
    struct KeyPosition { XMFLOAT3 position; float timeStamp; };
    struct KeyRotation { XMFLOAT4 orientation; float timeStamp; }; // Кватернион
    struct KeyScale { XMFLOAT3 scale; float timeStamp; };

    struct BoneAnimation {
        std::string name;
        std::vector<KeyPosition> positions;
        std::vector<KeyRotation> rotations;
        std::vector<KeyScale> scales;
    };

    struct Animation {
        std::string name;
        float duration;
        float ticksPerSecond;
        std::vector<BoneAnimation> channels; // Индексы совпадают с m_boneMatrices
    };

    bool HasAnimations() const { return !m_animations.empty(); }
    const std::vector<Animation>& GetAnimations() const { return m_animations; }
    const std::vector<XMFLOAT4X4>& GetBoneMatrices() const { return m_boneMatrices; }
    void UpdateAnimation(float currentTime, int animIndex = 0);
    void SetBoneMatrices(ID3D11DeviceContext* deviceContext);

private:

    //ANIMATION
    std::vector<XMFLOAT4X4> m_boneMatrices;          // Финальные матрицы для шейдера
    std::unordered_map<std::string, int> m_boneMap;  // Имя кости → индекс в массиве
    std::vector<Animation> m_animations;
    const aiScene* m_scene;                          // Уже есть у тебя
    int m_maxBones = 0;
    std::vector<XMFLOAT4X4> m_boneOffsets;  // Индекс = boneIndex
    ID3D11Buffer* m_boneMatrixBuffer = nullptr;     // Константный буфер для матриц костей
    // Вспомогательные
    
    XMFLOAT4X4 ConvertMatrix(const aiMatrix4x4& from);
    XMFLOAT4X4 InterpolatePosition(const std::vector<KeyPosition>& keys, float animTime);
    XMFLOAT4X4 InterpolateRotation(const std::vector<AssimpModelClass::KeyRotation>& keys, float animTime);
    XMFLOAT4X4 InterpolateScale(const std::vector<KeyScale>& keys, float animTime);
    XMFLOAT4X4 InterpolateTransform(const BoneAnimation& channel, float animTime);
    void ReadNodeHierarchy(const aiNode* node, const XMFLOAT4X4& parentTransform, const Animation& anim, float animTime);
    XMFLOAT4X4 CalculateFinalTransform(int boneIndex, float animTime);

    // Загружает вшитую текстуру из GLB по типу (BASE_COLOR, NORMALS, etc.)
    ID3D11ShaderResourceView* LoadEmbeddedTexture(ID3D11Device* device,
        const aiTexture* tex);

    // Вспомогательный метод — ищет и грузит текстуру нужного типа для меша
    ID3D11ShaderResourceView* ExtractTexture(ID3D11Device* device,
        const aiMaterial* mat,
        aiTextureType type1,
        aiTextureType type2 = aiTextureType_NONE);

    ID3D11ShaderResourceView* ExtractTexture_Internal(ID3D11Device* device,
        ID3D11DeviceContext* deviceContext,
        const aiScene* scene,
        const aiMaterial* mat,
        aiTextureType type1,
        aiTextureType type2 = aiTextureType_NONE);


   

private:
    Assimp::Importer      m_importer;
    const aiScene* m_scene;
    TextureClass* m_Texture;
    int                   m_totalIndexCount;
    std::vector<MeshPart> m_meshParts;


    // 🔥 ДОБАВЬТЕ КЭШИРОВАНИЕ:
    mutable XMFLOAT3 m_cachedBBMin = XMFLOAT3(FLT_MAX, FLT_MAX, FLT_MAX);
    mutable XMFLOAT3 m_cachedBBMax = XMFLOAT3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    mutable XMFLOAT3 m_cachedSphereCenter = XMFLOAT3(0, 0, 0);
    mutable float m_cachedSphereRadius = 0.0f;
    mutable bool m_boundsCached = false;

    // Владелец для любых TextureClass, созданных из встроенных изображений.
    // Они живут пока не вызовем Shutdown() и тем самым держат SRV валидными.
    bool m_isEmbeddedFormat = false;  // true только для .glb / .gltf
};