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
#include "../../../AnimationComponent.h"

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
        ID3D11Buffer* indexBuffer = nullptr;
        int           indexCount = 0;

        ID3D11ShaderResourceView* texture = nullptr;
        ID3D11ShaderResourceView* normalMap = nullptr;
        ID3D11ShaderResourceView* metalness = nullptr;
        ID3D11ShaderResourceView* roughness = nullptr;

        bool  isTransparent = false;
        bool  isGlass = false;   // ← НОВОЕ: стекло из GLB
        float opacity = 1.0f;
    };

    // ── Вершина — добавили tangent и binormal для PBR/NormalMap ──────────────
    struct VertexType
    {
        XMFLOAT3 position;
        XMFLOAT2 texture;
        XMFLOAT3 normal;
        XMFLOAT3 tangent;
        XMFLOAT3 binormal;

        // НОВОЕ: Для скелетной анимации
        int boneIDs[4];        // или XMFLOAT4/int4
        float boneWeights[4];  // или XMFLOAT4
    };

public:
    AssimpModelClass();
    ~AssimpModelClass();

    bool Initialize(ID3D11Device* device, ID3D11DeviceContext* deviceContext,
        const char* modelPath, const char* textureFilename);
    void DebugCompareNames();
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

    struct BoneAnimationChannel {
        std::string nodeName;  // Имя узла (НЕ кости!)
        std::vector<KeyPosition> positions;
        std::vector<KeyRotation> rotations;
        std::vector<KeyScale> scales;
    };

    void UpdateAnimation(AnimationComponent& animComp, float deltaTime);
    void SetBoneMatricesFromComponent(ID3D11DeviceContext* context, const AnimationComponent& animComp);

  
    

    struct AnimationClip {
        std::string name;
        float duration;
        float ticksPerSecond;
        std::vector<BoneAnimationChannel> channels;
    };
    struct BoneInfo {
        std::string name;
        XMFLOAT4X4 offsetMatrix;
        int boneIndex;
    };

    void ReadNodeHierarchyToComponent(
        const aiNode* node,
        const DirectX::XMMATRIX& parentTransform,
        const AnimationClip& anim,              // ← const ref
        float animTime,
        std::vector<DirectX::XMFLOAT4X4>& outMatrices) const;  // ← БЕЗ const!

    void ComputeBoneMatrices(int animIndex, float animTime, std::vector<XMFLOAT4X4>& outMatrices) const;

    void PrintModelDebugInfo();

    void DebugAnimationBinding(const AnimationClip& anim);

    const AnimationClip* GetAnimation(int index) const {
        if (index < 0 || index >= (int)m_animations.size()) return nullptr;
        return &m_animations[index];
    }

    [[nodiscard]] size_t GetAnimationCount() const { return m_animations.size(); }

    // Опционально: проверить, есть ли анимации вообще
    [[nodiscard]] bool HasAnimations() const { return !m_animations.empty(); }

    std::unordered_map<std::string, BoneInfo> m_boneInfoMap;  // ключ: имя узла
    std::vector<AnimationClip> m_animations;
    std::vector<XMFLOAT4X4> m_finalBoneMatrices;  // [boneIndex] -> матрица для шейдера
    ID3D11Buffer* m_boneMatrixBuffer;
    float m_animationTime;

   



    

private:
   
    //ANIMATIONx
   
    // Вспомогательные

    XMFLOAT4X4 ConvertMatrix(const aiMatrix4x4& from) const;
    XMMATRIX InterpolatePosition(const std::vector<KeyPosition>&, float) const;
    XMMATRIX InterpolateRotation(const std::vector<KeyRotation>&, float) const;
    XMMATRIX InterpolateScale(const std::vector<KeyScale>&, float) const;

   
   
    void ReadNodeHierarchy(const aiNode* node, const XMMATRIX& parentTransform,
        const AnimationClip& anim, float animTime);

   
  
    
  
    
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

    TextureClass* m_Texture;
    int                   m_totalIndexCount;
    std::vector<MeshPart> m_meshParts;
    const aiScene* m_scene = nullptr;

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