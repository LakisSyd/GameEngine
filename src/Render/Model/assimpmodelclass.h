#pragma once
#include <d3d11.h>
#include <DirectXMath.h>
#include <vector>
#include <string>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "TextureClass.h"

class TextureShaderClass;

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

        // PBR текстурные слоты (все вшитые из GLB/GLTF)
        ID3D11ShaderResourceView* texture = nullptr; // slot 0 — albedo / diffuse
        ID3D11ShaderResourceView* normalMap = nullptr; // slot 1 — normal map
        ID3D11ShaderResourceView* metalness = nullptr; // slot 2 — metalness
        ID3D11ShaderResourceView* roughness = nullptr; // slot 3 — roughness
    };

    // ── Вершина — добавили tangent и binormal для PBR/NormalMap ──────────────
    struct VertexType
    {
        XMFLOAT3 position;
        XMFLOAT2 texture;
        XMFLOAT3 normal;
        XMFLOAT3 tangent;  // ← нужен для TBN матрицы в pbr.vs
        XMFLOAT3 binormal; // ← нужен для TBN матрицы в pbr.vs
    };

public:
    AssimpModelClass();
    ~AssimpModelClass();

    bool Initialize(ID3D11Device* device, ID3D11DeviceContext* deviceContext,
        const char* modelPath, const char* textureFilename);
    void Shutdown();

    void Render(ID3D11DeviceContext* deviceContext);
    void RenderAll(ID3D11DeviceContext* deviceContext,
        TextureShaderClass* shader,
        XMMATRIX world, XMMATRIX view, XMMATRIX proj);

    static aiColor4D GetPBRColorFromMaterial(const aiMaterial* mat, aiTextureType type);
    static ID3D11ShaderResourceView* Create1x1SRVFromColor(ID3D11Device* device, const aiColor4D& color);

    int                       GetIndexCount();
    ID3D11ShaderResourceView* GetTexture();

    const std::vector<MeshPart>& GetMeshParts() const { return m_meshParts; }

    std::vector<TextureClass*> GetOwnedTextures() const { return m_ownedTextures; }

    std::vector<TextureClass*> m_ownedTextures;

private:
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

    // Владелец для любых TextureClass, созданных из встроенных изображений.
    // Они живут пока не вызовем Shutdown() и тем самым держат SRV валидными.
   
};