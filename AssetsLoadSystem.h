#pragma once
#include <string>
#include <assimpmodelclass.h>
#include "../../JobSystem.h"
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <utility>
#include <DirectXMath.h>
#include "MaterialClass.h"
#include "LightShaderClass.h"
#include "Textureshaderclass.h"
enum class AssetState
{
    Unloaded,
    Loading,
    CpuLoaded,
    Ready,
    Error
};

struct MeshCpuVertex
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

struct MeshCpuData
{
	std::vector<MeshCpuVertex> vertices;
	std::vector<uint32_t> indices;
};

struct MeshLoadResult
	// Добавьте определение структуры Vertex перед использованием в MeshCpuData
{
	bool success = false;
	std::string error;
	MeshCpuData mesh;
    // Добавьте перечисление AssetState перед использованием
  
};

struct MeshGpuData
{
    ID3D11Buffer* vertexBuffer = nullptr;
    ID3D11Buffer* indexBuffer = nullptr;
    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;
};


struct MeshAsset
{
    std::string path;
    AssetState state;
    MeshCpuData cpuData;
    AssimpModelClass* gpuModel;
	std::string error; // Для хранения сообщения об ошибке, если загрузка не удалась
    MeshGpuData gpuData;
};

class AssetLoadSystem
{
public:
    AssetLoadSystem(JobSystem& jobs)
        : m_jobs(jobs)
    {
    }


    void RequestMeshLoad(const std::string& path)
    {
        if (m_meshes.contains(path))
            return;

        MeshAsset asset;
        asset.path = path;
        asset.state = AssetState::Loading;
        asset.gpuModel = nullptr;

        m_meshes[path] = std::move(asset);

        m_jobs.SubmitJob([this, path]()
            {
                MeshLoadResult result = LoadMeshFromFile(path);


                m_jobs.SubmitMainThread([this, path, result = std::move(result)]() mutable
                    {
                        auto& asset = m_meshes[path];

                        if (!result.success)
                        {
                            asset.state = AssetState::Error;
                            asset.error = result.error;
                            return;
                        }


                        asset.cpuData = std::move(result.mesh);
                        asset.state = AssetState::CpuLoaded;

                    });
            });

    }
    void Update(ID3D11Device* device)
    {
        for (auto& [path, asset] : m_meshes)
        {
            if (asset.state == AssetState::CpuLoaded)
            {
                if (CreateGpuMesh(device, asset))
                    asset.state = AssetState::Ready;
                else
                    asset.state = AssetState::Error;
            }
        }
    }

    

    AssetState GetMeshState(const std::string& path) const
    {
        auto it = m_meshes.find(path);
        if (it == m_meshes.end())
            return AssetState::Unloaded;
		return it->second.state;
    }

    MeshAsset* GetMesh(const std::string& path)
    {
        auto it = m_meshes.find(path);
        if (it == m_meshes.end())
            return nullptr;
		return &it->second;
    }

    void Shutdown()
    {
        for (auto& [path, asset] : m_meshes)
        {
            ReleaseGpuMesh(asset);
        }

        m_meshes.clear();
    }

    void ReleaseGpuMesh(MeshAsset& asset)
    {
        if (asset.gpuData.vertexBuffer)
        {
            asset.gpuData.vertexBuffer->Release();
            asset.gpuData.vertexBuffer = nullptr;
        }

        if (asset.gpuData.indexBuffer)
        {
            asset.gpuData.indexBuffer->Release();
            asset.gpuData.indexBuffer = nullptr;
        }

        asset.gpuData.vertexCount = 0;
        asset.gpuData.indexCount = 0;
    }
    bool BindMeshAsset(
        ID3D11DeviceContext* context,
        const std::string& path,
        uint32_t& outIndexCount)
    {
        MeshAsset* asset = GetMesh(path);

        if (!asset || asset->state != AssetState::Ready)
            return false;

        if (!asset->gpuData.vertexBuffer || !asset->gpuData.indexBuffer)
            return false;

        unsigned int stride = sizeof(MeshCpuVertex);
        unsigned int offset = 0;

        context->IASetVertexBuffers(
            0,
            1,
            &asset->gpuData.vertexBuffer,
            &stride,
            &offset
        );

        context->IASetIndexBuffer(
            asset->gpuData.indexBuffer,
            DXGI_FORMAT_R32_UINT,
            0
        );

        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        outIndexCount = asset->gpuData.indexCount;
        return true;
    }

    bool RenderMeshAsset(
        ID3D11DeviceContext* context,
        const std::string& path,
        const DirectX::XMMATRIX& world,
        const DirectX::XMMATRIX& view,
        const DirectX::XMMATRIX& projection,
        MaterialClass* material,
        DirectX::XMFLOAT4 lightColors[],
        DirectX::XMFLOAT4 lightPositions[],
        bool selected = false)
    {
        if (!material || !material->GetShader())
            return false;

        uint32_t indexCount = 0;
        if (!BindMeshAsset(context, path, indexCount))
            return false;

        ID3D11ShaderResourceView* textureSRV = nullptr;
        if (auto* texture = material->GetTexture(0))
            textureSRV = texture->GetTexture();
        if (!textureSRV && material->GetMainTexture())
            textureSRV = material->GetMainTexture()->GetTexture();

        auto* shader = material->GetShader();
        switch (shader->GetType())
        {
        case ShaderType::Light:
        {
            auto* lightShader = static_cast<LightShaderClass*>(shader);
            lightShader->SetSelectedFactor(context, selected ? 2.0f : 1.0f);
            lightShader->Render(
                context,
                static_cast<int>(indexCount),
                world,
                view,
                projection,
                textureSRV,
                lightColors,
                lightPositions);
            return true;
        }
        case ShaderType::Texture:
        {
            auto* textureShader = static_cast<TextureShaderClass*>(shader);
            textureShader->Render(
                context,
                static_cast<int>(indexCount),
                world,
                view,
                projection,
                textureSRV);
            return true;
        }
        default:
            return false;
        }
    }
private:
    JobSystem& m_jobs;
    std::unordered_map<std::string, MeshAsset> m_meshes;


    bool CreateGpuMesh(ID3D11Device* device, MeshAsset& asset)
    {
        if (!device)
            return false;

        if (asset.cpuData.vertices.empty() || asset.cpuData.indices.empty())
            return false;

        asset.gpuData.vertexCount = static_cast<uint32_t>(asset.cpuData.vertices.size());
        asset.gpuData.indexCount = static_cast<uint32_t>(asset.cpuData.indices.size());

        D3D11_BUFFER_DESC vertexDesc{};
        vertexDesc.Usage = D3D11_USAGE_DEFAULT;
        vertexDesc.ByteWidth = sizeof(MeshCpuVertex) * asset.gpuData.vertexCount;
        vertexDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA vertexData{};
        vertexData.pSysMem = asset.cpuData.vertices.data();

        HRESULT hr = device->CreateBuffer(
            &vertexDesc,
            &vertexData,
            &asset.gpuData.vertexBuffer
        );

        if (FAILED(hr))
            return false;

        D3D11_BUFFER_DESC indexDesc{};
        indexDesc.Usage = D3D11_USAGE_DEFAULT;
        indexDesc.ByteWidth = sizeof(uint32_t) * asset.gpuData.indexCount;
        indexDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

        D3D11_SUBRESOURCE_DATA indexData{};
        indexData.pSysMem = asset.cpuData.indices.data();

        hr = device->CreateBuffer(
            &indexDesc,
            &indexData,
            &asset.gpuData.indexBuffer
        );

        if (FAILED(hr))
        {
            ReleaseGpuMesh(asset);
            return false;
        }

        
        asset.cpuData.vertices.clear();
        asset.cpuData.vertices.shrink_to_fit();
        asset.cpuData.indices.clear();
        asset.cpuData.indices.shrink_to_fit();

        return true;
    }

    MeshLoadResult LoadMeshFromFile(const std::string& path)
    {
        MeshLoadResult result;

        Assimp::Importer importer;

        const aiScene* scene = importer.ReadFile(
            path,
            aiProcess_Triangulate |
            aiProcess_GenNormals |
            aiProcess_CalcTangentSpace |
            aiProcess_JoinIdenticalVertices
        );

        if (!scene || !scene->mRootNode || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE)
        {
            result.success = false;
            result.error = importer.GetErrorString();
            return result;
        }
        for (unsigned int meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex)
        {
            aiMesh* mesh = scene->mMeshes[meshIndex];

            uint32_t baseVertex = static_cast<uint32_t>(result.mesh.vertices.size());

            for (unsigned int i = 0; i < mesh->mNumVertices; ++i)
            {
                MeshCpuVertex vertex{};

                vertex.position = {
                    mesh->mVertices[i].x,
                    mesh->mVertices[i].y,
                    mesh->mVertices[i].z
                };

                if (mesh->HasTextureCoords(0))
                {
                    vertex.texture = {
                        mesh->mTextureCoords[0][i].x,
                        mesh->mTextureCoords[0][i].y
                    };
                }

                if (mesh->HasNormals())
                {
                    vertex.normal = {
                        mesh->mNormals[i].x,
                        mesh->mNormals[i].y,
                        mesh->mNormals[i].z
                    };
                }

                if (mesh->HasTangentsAndBitangents())
                {
                    vertex.tangent = {
                        mesh->mTangents[i].x,
                        mesh->mTangents[i].y,
                        mesh->mTangents[i].z
                    };

                    vertex.binormal = {
                        mesh->mBitangents[i].x,
                        mesh->mBitangents[i].y,
                        mesh->mBitangents[i].z
                    };
                }

                for (int b = 0; b < 4; ++b)
                {
                    vertex.boneIDs[b] = 0;
                    vertex.boneWeights[b] = 0.0f;
                }

                result.mesh.vertices.push_back(vertex);
            }

            for (unsigned int f = 0; f < mesh->mNumFaces; ++f)
            {
                const aiFace& face = mesh->mFaces[f];

                if (face.mNumIndices != 3)
                    continue;

                result.mesh.indices.push_back(baseVertex + face.mIndices[0]);
                result.mesh.indices.push_back(baseVertex + face.mIndices[1]);
                result.mesh.indices.push_back(baseVertex + face.mIndices[2]);
            }
        }

        result.success = true;
        return result;
    }
};