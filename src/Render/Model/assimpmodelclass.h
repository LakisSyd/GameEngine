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
#include "AnimationComponent.h"



class TextureShaderClass;
class PBRShaderClass;

using namespace DirectX;
using namespace std;

class AssimpModelClass
{
public:
    // -- Один submesh — хранит геометрию + все PBR текстуры из GLB ------------
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
        bool  isGlass = false;   // < НОВОЕ: стекло из GLB
        float opacity = 1.0f;
    };

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


    // -- Вершина — добавили tangent и binormal для PBR/NormalMap --------------
    

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
    const std::vector<VertexType>& GetCpuMeshVerticesForTest() const { return m_cpuMeshVertices; }
    const std::vector<unsigned long>& GetCpuMeshIndicesForTest() const { return m_cpuMeshIndices; }
    bool ApplyCpuMeshForTest(
        ID3D11Device* device,
        std::vector<VertexType>&& vertices,
        std::vector<unsigned long>&& indices);
    bool InitializeFromGeometry(
        ID3D11Device* device,
        std::vector<VertexType>&& vertices,
        std::vector<unsigned long>&& indices,
        const XMFLOAT4& baseColor = { 0.82f, 0.82f, 0.74f, 1.0f },
        float metallic = 0.0f,
        float roughness = 0.65f);
    bool InitializeFromGeometryWithMaterial(
        ID3D11Device* device,
        ID3D11DeviceContext* context,
        const aiScene* scene,
        const aiMaterial* material,
        std::vector<VertexType>&& vertices,
        std::vector<unsigned long>&& indices,
        bool loadImageTextures = true);
    void LoadBones(const aiScene* scene);
    void LoadAnimations(const aiScene* scene);
    void GetBoundingBox(XMFLOAT3* min, XMFLOAT3* max) const;
    bool IsEmbeddedFormat() const { return m_isEmbeddedFormat; }
    int FindAnimationIndexByName(const std::string& name) const;
    // Exact names are preferred. If an asset author prefixes a standard clip
    // (for example Kevin_Walk), this also recognizes the final _Walk token.
    int FindAnimationIndexBySemanticName(const std::string& name) const;

    // Prototype mesh cutting. Removes whole triangles inside a local-space sphere
    // and rebuilds the first mesh part buffers. This is intentionally rough.
    bool CutSphereHoleForTest(
        ID3D11Device* device,
        const XMFLOAT3& localCenter,
        float radius,
        int* removedTriangles = nullptr);

    // Prototype ray-hit cutter. It subtracts a circular disk on a triangle surface
    // by clipping triangles in local tangent space and rebuilding the first mesh part.
    bool CutCircularHoleForTest(
        ID3D11Device* device,
        const XMFLOAT3& localCenter,
        const XMFLOAT3& localNormal,
        float radius,
        float depth,
        int* removedTriangles = nullptr,
        int* createdTriangles = nullptr,
        bool rebuildGpuBuffers = true);


    bool InitializeFromMesh(
        ID3D11Device* device,
        ID3D11DeviceContext* context,
        const aiScene* scene,
        aiMesh* mesh,
        bool loadImageTextures = true);

    bool InitializeFromMeshes(
        ID3D11Device* device,
        ID3D11DeviceContext* context,
        const aiScene* scene,
        const unsigned int* meshIndices,
        unsigned int meshCount,
        bool loadImageTextures = true);


    //ANIMATION
     // -- Ключевые кадры ------------------------------------------------
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
    bool TryGetBoneModelTransform(
        const AnimationComponent& animComp,
        const std::string& boneName,
        DirectX::XMMATRIX& outTransform) const;
    // Returns the animated node in model space and the same node before its own
    // additive pose correction. Parent corrections are preserved, which lets
    // Animation Studio convert a viewport gizmo back into a stable local key.
    // Unlike TryGetBoneModelTransform this also works for animated helper nodes
    // that do not carry skin weights.
    bool TryGetAnimationNodeEditTransforms(
        const AnimationComponent& animComp,
        const std::string& nodeName,
        DirectX::XMMATRIX& outCurrentTransform,
        DirectX::XMMATRIX& outUncorrectedTransform) const;
    bool ApplyTwoBoneIK(AnimationComponent& animation,
        const std::string& upperArm, const std::string& forearm, const std::string& hand,
        const DirectX::XMMATRIX& target,
        const DirectX::XMVECTOR* poleTarget = nullptr) const;
    bool TryGetArmIKChain(bool rightHand, std::string& upperArm,
        std::string& forearm, std::string& hand) const;
    bool TryGetHeadIKBone(std::string& head) const;
    bool ApplyHeadLookIK(AnimationComponent& animation,
        const std::string& head, const DirectX::XMVECTOR& modelTarget,
        float maxAngleDegrees = 75.0f) const;

  
    

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
        const AnimationClip& anim,              // < const ref
        float animTime,
        std::vector<DirectX::XMFLOAT4X4>& outMatrices,
        const AnimationClip* upperClip = nullptr, float upperTime = 0.0f,
        const std::string& upperRoot = "", bool inUpperBody = false,
        const std::vector<AnimationPoseKey>* poseKeys = nullptr,
        const std::vector<AnimationFistKey>* fistKeys = nullptr,
        int poseVariant = -1) const;

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

    // CPU-копия геометрии нужна для моделей, созданных из одного aiMesh.
    // aiScene живет внутри Assimp::Importer и может исчезнуть после импорта корабля,
    // а picking/culling все равно должны знать bounds и треугольники модели.
    std::vector<XMFLOAT3> m_cpuVertices;
    std::vector<XMFLOAT3> m_cpuTriangles;
    std::vector<VertexType> m_cpuMeshVertices;
    std::vector<unsigned long> m_cpuMeshIndices;
    const aiScene* m_scene = nullptr;

    // ?? ДОБАВЬТЕ КЭШИРОВАНИЕ:
    mutable XMFLOAT3 m_cachedBBMin = XMFLOAT3(FLT_MAX, FLT_MAX, FLT_MAX);
    mutable XMFLOAT3 m_cachedBBMax = XMFLOAT3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    mutable XMFLOAT3 m_cachedSphereCenter = XMFLOAT3(0, 0, 0);
    mutable float m_cachedSphereRadius = 0.0f;
    mutable bool m_boundsCached = false;

    // Владелец для любых TextureClass, созданных из встроенных изображений.
    // Они живут пока не вызовем Shutdown() и тем самым держат SRV валидными.
    bool m_isEmbeddedFormat = false;  // true только для .glb / .gltf
};
