// ============================================================================
// SceneManager.cpp
//
// РќР•Р”Р•Р›РЇ 4: GetObject(int) Рё m_legacyObjects СѓРґР°Р»РµРЅС‹.
//           EditorUI Рё PickingSystem СЂР°Р±РѕС‚Р°СЋС‚ С‡РµСЂРµР· Entity + ECS.
// ============================================================================

#include "SceneManager.h"
#include "FileSystem.h"
#include "ShaderManager.h"
#include "PhysicsSystem.h"
#include "AnimationSystem.h"
#include "AssetsLoadSystem.h"
#include "../Render/FrustumCulling.h"
#include "../Render/RenderCandidateSystem.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <future>
#include <atomic>
#include <chrono>
#include <unordered_map>
#include "CharacterSpawnSystem.h"
#include "Gameplay/Player/CharacterPhysicsTuningComponent.h"
#include "RoomSystem.h"
#include "RoomEnvironmentSystem.h"
#include "Gameplay/Power/OxygenGeneratorSystem.h"
#include "Gameplay/Power/GeneratorComponent.h"
#include "Gameplay/Power/GeneratorSystem.h"
#include "Gameplay/Power/PowerConsumerComponent.h"
#include "Gameplay/Power/PowerGridComponent.h"
#include "Gameplay/Power/ShipPowerGridSystem.h"
#include "Gameplay/Ship/ShipAlertSystem.h"
#include "PlayerVitalsSystem.h"
#include "PlayerComponents.h"
#include "Gameplay/Repair/RepairComponent.h"
#include "Gameplay/Ship/RoomCullingSystem.h"
#include "Gameplay/Lighting/SpotLightComponent.h"
#include "Gameplay/Combat/WeaponComponent.h"
#include "Gameplay/Player/FirstPersonViewSystem.h"
#include "../../NPCComponents.h"
#include "Gameplay/AI/NpcNavMeshSystem.h"
namespace
{
    constexpr bool kUseShipPanelVisualBatch = true;

    bool CanUseAsyncMeshPath(const std::string& path)
    {
        if (path.empty())
            return false;

        std::string ext = fs::path(path).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        return ext != ".glb" && ext != ".gltf";
    }

    bool IsGltfMeshPath(const std::string& path)
    {
        std::string ext = fs::path(path).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        return ext == ".glb" || ext == ".gltf";
    }

    bool StartsWith(const std::string& value, const char* prefix)
    {
        return value.rfind(prefix, 0) == 0;
    }

    std::string ToUpper(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(),
            [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        return value;
    }

    bool IsShipCollisionNode(const std::string& nodeName)
    {
        const std::string name = ToUpper(nodeName);
        return StartsWith(name, "COL_") || StartsWith(name, "SHIP_COL_") ||
            StartsWith(name, "PHYS_") || StartsWith(name, "COLLISION_");
    }

    bool IsShipDoorNode(const std::string& nodeName)
    {
        return StartsWith(ToUpper(nodeName), "SHIP_DOOR_");
    }

    bool IsDoorLeafNode(const std::string& nodeName)
    {
        return StartsWith(ToUpper(nodeName), "DOOR_LEAF_");
    }

    bool IsShipLightNode(const std::string& nodeName)
    {
        const std::string upper = ToUpper(nodeName);
        return StartsWith(upper, "SHIP_LIGHT") ||
            StartsWith(upper, "SHIP LIGHT");
    }

    bool IsShipPanelNode(const std::string& nodeName)
    {
        return StartsWith(ToUpper(nodeName), "SHIP_PANEL_");
    }

    bool NameLooksLikeGlass(const std::string& name)
    {
        const std::string upper = ToUpper(name);
        return upper.find("GLASS") != std::string::npos ||
            upper.find("WINDOW") != std::string::npos;
    }

    bool IsNameShipStairBridge(const std::string& name)
    {
        const std::string upper = ToUpper(name);
        return upper.find("STAIR") != std::string::npos ||
            upper.find("BRIDGE") != std::string::npos;
	}

    bool ModelLooksLikeGlass(const AssimpModelClass* model, float* opacityOut = nullptr)
    {
        if (opacityOut)
            *opacityOut = 0.15f;
        if (!model)
            return false;

        bool result = false;
        for (const auto& part : model->GetMeshParts())
        {
            if (!part.isGlass && !part.isTransparent)
                continue;

            result = true;
            if (opacityOut)
                *opacityOut = std::clamp(part.opacity, 0.02f, 0.95f);

            if (part.isGlass)
                break;
        }

        return result;
    }

    std::string TryParsePanelRoomName(const std::string& panelName)
    {
        std::string payload = ToUpper(panelName);
        const std::string prefix = "SHIP_PANEL_";
        if (!StartsWith(payload, prefix.c_str()))
            return "";

        payload = payload.substr(prefix.size());

        const char* markers[] = {
            "_WALL_", "_FLOOR", "_CEIL", "_CEILING", "_ROOF", "_HULL", "_PANEL"
        };

        size_t markerPos = std::string::npos;
        for (const char* marker : markers)
        {
            const size_t pos = payload.find(marker);
            if (pos != std::string::npos && (markerPos == std::string::npos || pos < markerPos))
                markerPos = pos;
        }

        if (markerPos != std::string::npos)
            payload = payload.substr(0, markerPos);

        while (!payload.empty() && payload.back() == '_')
            payload.pop_back();

        return payload.empty() ? "" : "SHIP_ROOM_" + payload;
    }

    std::string StripBlenderNumericSuffix(std::string value)
    {
        const size_t dot = value.rfind('.');
        if (dot == std::string::npos || dot + 1 >= value.size())
            return value;

        for (size_t i = dot + 1; i < value.size(); ++i)
        {
            if (!std::isdigit(static_cast<unsigned char>(value[i])))
                return value;
        }

        return value.substr(0, dot);
    }

    std::string NormalizeRoomLookupKey(std::string value)
    {
        value = StripBlenderNumericSuffix(ToUpper(value));
        if (!StartsWith(value, "SHIP_ROOM_"))
            value = "SHIP_ROOM_" + value;

        std::string key;
        key.reserve(value.size());
        for (unsigned char c : value)
        {
            if (std::isalnum(c))
                key.push_back(static_cast<char>(c));
        }
        return key;
    }
    std::string GetShipCollisionOwnerName(const std::string& collisionName)
    {
        const std::string name = StripBlenderNumericSuffix(ToUpper(collisionName));

        const std::string shipColPrefix = "SHIP_COL_";
        if (StartsWith(name, shipColPrefix.c_str()))
            return "SHIP_" + name.substr(shipColPrefix.size());

        const std::string collisionPrefix = "COLLISION_";
        if (StartsWith(name, collisionPrefix.c_str()))
            return name.substr(collisionPrefix.size());

        const std::string physPrefix = "PHYS_";
        if (StartsWith(name, physPrefix.c_str()))
            return name.substr(physPrefix.size());

        const std::string colPrefix = "COL_";
        if (StartsWith(name, colPrefix.c_str()))
            return name.substr(colPrefix.size());

        return "";
    }

    void CollectShipCollisionProxyNodes(
        aiNode* node,
        std::unordered_map<std::string, aiNode*>& outProxyNodes)
    {
        if (!node)
            return;

        const std::string nodeName = node->mName.C_Str();
        if (node->mNumMeshes > 0 && IsShipCollisionNode(nodeName))
        {
            const std::string ownerName = GetShipCollisionOwnerName(nodeName);
            if (IsShipDoorNode(ownerName) || IsDoorLeafNode(ownerName) ||
                IsNameShipStairBridge(ownerName))
                outProxyNodes[ownerName] = node;
        }

        for (unsigned int i = 0; i < node->mNumChildren; ++i)
            CollectShipCollisionProxyNodes(node->mChildren[i], outProxyNodes);
    }

    bool TryParseDoorRoomNames(const std::string& doorName, std::string& roomA, std::string& roomB)
    {
        std::string payload = StripBlenderNumericSuffix(ToUpper(doorName));
        const std::string prefix = "SHIP_DOOR_";
        if (!StartsWith(payload, prefix.c_str()))
            return false;

        payload = payload.substr(prefix.size());
        const size_t separator = payload.find("__");
        if (separator == std::string::npos)
            return false;

        roomA = payload.substr(0, separator);
        roomB = payload.substr(separator + 2);
        return !roomA.empty() && !roomB.empty();
    }

    std::string MakeDoorLeafName(char side, const std::string& roomA, const std::string& roomB)
    {
        std::string name = "DOOR_LEAF_";
        name.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(side))));
        name += "_" + ToUpper(roomA) + "__" + ToUpper(roomB);
        return name;
    }
    bool ShouldImportShipVisualNode(const std::string& nodeName)
    {
        return !IsShipCollisionNode(nodeName);
    }
    XMFLOAT3 QuaternionToEulerRadians(const XMFLOAT4& q)
    {
        const float sinrCosp = 2.0f * (q.w * q.x + q.y * q.z);
        const float cosrCosp = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
        const float x = std::atan2(sinrCosp, cosrCosp);

        const float sinp = 2.0f * (q.w * q.y - q.z * q.x);
        float y = 0.0f;
        if (std::fabs(sinp) >= 1.0f)
            y = std::copysign(XM_PIDIV2, sinp);
        else
            y = std::asin(sinp);

        const float sinyCosp = 2.0f * (q.w * q.z + q.x * q.y);
        const float cosyCosp = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);
        const float z = std::atan2(sinyCosp, cosyCosp);

        return { x, y, z };
    }

    TransformComponent TransformFromMatrix(const XMMATRIX& world)
    {
        XMVECTOR scaleVec;
        XMVECTOR rotationQuat;
        XMVECTOR translationVec;
        TransformComponent transform{};
        if (XMMatrixDecompose(&scaleVec, &rotationQuat, &translationVec, world))
        {
            XMFLOAT4 q{};
            XMStoreFloat3(&transform.scale, scaleVec);
            XMStoreFloat3(&transform.position, translationVec);
            XMStoreFloat4(&q, rotationQuat);
            transform.rotation = QuaternionToEulerRadians(q);
        }
        return transform;
    }
    struct ShipPanelMaterialGeometry
    {
        std::vector<AssimpModelClass::VertexType> vertices;
        std::vector<unsigned long> indices;
    };

    struct ShipImportNodeTask
    {
        aiNode* node = nullptr;
        DirectX::XMMATRIX parentWorld = DirectX::XMMatrixIdentity();
    };

    void CollectShipImportNodeTasks(
        aiNode* node,
        const DirectX::XMMATRIX& parentWorld,
        std::vector<ShipImportNodeTask>& tasks)
    {
        if (!node)
            return;

        tasks.push_back({ node, parentWorld });
        const DirectX::XMMATRIX local = DirectX::XMMATRIX(
            node->mTransformation.a1, node->mTransformation.b1, node->mTransformation.c1, node->mTransformation.d1,
            node->mTransformation.a2, node->mTransformation.b2, node->mTransformation.c2, node->mTransformation.d2,
            node->mTransformation.a3, node->mTransformation.b3, node->mTransformation.c3, node->mTransformation.d3,
            node->mTransformation.a4, node->mTransformation.b4, node->mTransformation.c4, node->mTransformation.d4);
        const DirectX::XMMATRIX world = local * parentWorld;
        for (unsigned int i = 0; i < node->mNumChildren; ++i)
            CollectShipImportNodeTasks(node->mChildren[i], world, tasks);
    }

    void AppendShipPanelGeometry(
        const aiScene* scene,
        aiNode* node,
        const DirectX::XMMATRIX& parentWorld,
        std::vector<ShipPanelMaterialGeometry>& materialGeometry)
    {
        if (!scene || !node)
            return;

        const DirectX::XMMATRIX local = DirectX::XMMATRIX(
            node->mTransformation.a1, node->mTransformation.b1, node->mTransformation.c1, node->mTransformation.d1,
            node->mTransformation.a2, node->mTransformation.b2, node->mTransformation.c2, node->mTransformation.d2,
            node->mTransformation.a3, node->mTransformation.b3, node->mTransformation.c3, node->mTransformation.d3,
            node->mTransformation.a4, node->mTransformation.b4, node->mTransformation.c4, node->mTransformation.d4);
        const DirectX::XMMATRIX world = local * parentWorld;
        const std::string name = node->mName.C_Str();

        if (IsShipPanelNode(name))
        {
            for (unsigned int meshSlot = 0; meshSlot < node->mNumMeshes; ++meshSlot)
            {
                const unsigned int meshIndex = node->mMeshes[meshSlot];
                if (meshIndex >= scene->mNumMeshes)
                    continue;

                const aiMesh* mesh = scene->mMeshes[meshIndex];
                if (!mesh || mesh->mNumVertices == 0)
                    continue;

                const unsigned int materialIndex = mesh->mMaterialIndex < materialGeometry.size()
                    ? mesh->mMaterialIndex
                    : 0;
                auto& batch = materialGeometry[materialIndex];
                const unsigned long vertexBase = static_cast<unsigned long>(batch.vertices.size());
                batch.vertices.reserve(batch.vertices.size() + mesh->mNumVertices);

                for (unsigned int i = 0; i < mesh->mNumVertices; ++i)
                {
                    AssimpModelClass::VertexType vertex = {};
                    const DirectX::XMVECTOR position = DirectX::XMVectorSet(
                        mesh->mVertices[i].x,
                        mesh->mVertices[i].y,
                        mesh->mVertices[i].z,
                        1.0f);
                    DirectX::XMStoreFloat3(
                        &vertex.position,
                        DirectX::XMVector3TransformCoord(position, world));
                    vertex.texture = mesh->HasTextureCoords(0)
                        ? DirectX::XMFLOAT2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y)
                        : DirectX::XMFLOAT2(0.0f, 0.0f);

                    const aiVector3D sourceNormal = mesh->HasNormals()
                        ? mesh->mNormals[i]
                        : aiVector3D(0.0f, 1.0f, 0.0f);
                    const DirectX::XMVECTOR normal = DirectX::XMVectorSet(
                        sourceNormal.x, sourceNormal.y, sourceNormal.z, 0.0f);
                    DirectX::XMStoreFloat3(
                        &vertex.normal,
                        DirectX::XMVector3Normalize(DirectX::XMVector3TransformNormal(normal, world)));

                    if (mesh->HasTangentsAndBitangents())
                    {
                        const DirectX::XMVECTOR tangent = DirectX::XMVectorSet(
                            mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z, 0.0f);
                        const DirectX::XMVECTOR binormal = DirectX::XMVectorSet(
                            mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z, 0.0f);
                        DirectX::XMStoreFloat3(
                            &vertex.tangent,
                            DirectX::XMVector3Normalize(DirectX::XMVector3TransformNormal(tangent, world)));
                        DirectX::XMStoreFloat3(
                            &vertex.binormal,
                            DirectX::XMVector3Normalize(DirectX::XMVector3TransformNormal(binormal, world)));
                    }

                    for (int bone = 0; bone < 4; ++bone)
                    {
                        vertex.boneIDs[bone] = -1;
                        vertex.boneWeights[bone] = 0.0f;
                    }
                    batch.vertices.push_back(vertex);
                }

                batch.indices.reserve(batch.indices.size() + static_cast<size_t>(mesh->mNumFaces) * 3);
                for (unsigned int faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex)
                {
                    const aiFace& face = mesh->mFaces[faceIndex];
                    if (face.mNumIndices != 3)
                        continue;
                    batch.indices.push_back(vertexBase + face.mIndices[0]);
                    batch.indices.push_back(vertexBase + face.mIndices[1]);
                    batch.indices.push_back(vertexBase + face.mIndices[2]);
                }
            }
        }

        for (unsigned int i = 0; i < node->mNumChildren; ++i)
            AppendShipPanelGeometry(scene, node->mChildren[i], world, materialGeometry);
    }
}

// ============================================================================
// РљРѕРЅСЃС‚СЂСѓРєС‚РѕСЂ
// ============================================================================
// ? РЎРўРђР›Рћ:
SceneManager::SceneManager()
    : m_defaultMaterial(nullptr)
    , m_mainModelFallback(nullptr)
{
    
}

SceneManager::~SceneManager()
{
    Clear();
}

DirectX::XMMATRIX SceneManager::ConvertAssimpMatrix(const aiMatrix4x4& m)
{
    return DirectX::XMMATRIX(
        m.a1, m.b1, m.c1, m.d1,
        m.a2, m.b2, m.c2, m.d2,
        m.a3, m.b3, m.c3, m.d3,
        m.a4, m.b4, m.c4, m.d4
    );
}

void SceneManager::ImportShipNode(
    const aiScene* scene,
    aiNode* node,
    const DirectX::XMMATRIX& parentWorld,
    Entity shipRoot,
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    const std::unordered_map<std::string, aiNode*>* collisionProxyNodes)
{
    if (!scene || !node || !device || !context)
        return;

    const DirectX::XMMATRIX local = ConvertAssimpMatrix(node->mTransformation);
    const DirectX::XMMATRIX world = local * parentWorld;
    const std::string name = node->mName.C_Str();

    char buffer[512];
    sprintf_s(buffer, "Ship node: %s | meshes=%u | children=%u\n",
        name.c_str(), node->mNumMeshes, node->mNumChildren);
    OutputDebugStringA(buffer);


    const std::string upperName = ToUpper(name);

    if (StartsWith(upperName, "REPAIR_ZONE_"))
    {
        const DirectX::XMMATRIX local =
            ConvertAssimpMatrix(node->mTransformation);

        const DirectX::XMMATRIX world =
            local * parentWorld;

        const TransformComponent transform =
            TransformFromMatrix(world);

        const Entity zone =
            m_registry.CreateEntity();

        m_registry.AddComponent<TransformComponent>(
            zone,
            transform);

        m_registry.AddComponent<NameComponent>(
            zone,
            NameComponent{ name });

        m_registry.AddComponent<ActiveComponent>(
            zone,
            ActiveComponent{});

        m_registry.AddComponent<WorkZoneComponent>(
            zone,
            WorkZoneComponent{});

        return;
    }


    if (node->mNumMeshes > 0)
    {
        const std::string upperName = ToUpper(name);
        const bool isRoomNode = StartsWith(upperName, "SHIP_ROOM_");
        const bool isGeneratorNode = StartsWith(upperName, "SHIP_GENERATOR");
        const bool isCollisionNode = IsShipCollisionNode(name);
        const bool isDoorNode = IsShipDoorNode(name);
		const bool isOxygenGeneratorNode = StartsWith(upperName, "SHIP_OXYGEN_GENERATOR_");
        const bool isShipLightNode = IsShipLightNode(name);
        const bool isDoorLeafNode = IsDoorLeafNode(name);
        const bool isPanelNode = IsShipPanelNode(name);
		const bool isStairBridgeNode = IsNameShipStairBridge(name);
        const std::string normalizedName = StripBlenderNumericSuffix(upperName);
        const std::string collisionOwnerName = isCollisionNode ? GetShipCollisionOwnerName(name) : "";
        const bool isDoorCollisionProxy =
            isCollisionNode && (IsShipDoorNode(collisionOwnerName) || IsDoorLeafNode(collisionOwnerName));
        const bool isStairCollisionProxy =
            isCollisionNode && IsNameShipStairBridge(collisionOwnerName);

        auto model = std::make_unique<AssimpModelClass>();

        const bool loadImageTextures = true;
        if (!model->InitializeFromMeshes(device, context, scene, node->mMeshes, node->mNumMeshes, loadImageTextures))
        {
            OutputDebugStringA(("ImportShip: mesh load failed for " + name + "\n").c_str());
            return;
        }

        AssimpModelClass* physicsModel = model.get();
        std::unique_ptr<AssimpModelClass> proxyModel;
        bool hasCollisionProxy = false;
        if (!isCollisionNode && collisionProxyNodes)
        {
            auto proxyIt = collisionProxyNodes->find(normalizedName);
            if (proxyIt != collisionProxyNodes->end() && proxyIt->second && proxyIt->second->mNumMeshes > 0)
            {
                proxyModel = std::make_unique<AssimpModelClass>();
                const bool proxyLoadTextures = false;
                if (proxyModel->InitializeFromMeshes(
                    device,
                    context,
                    scene,
                    proxyIt->second->mMeshes,
                    proxyIt->second->mNumMeshes,
                    proxyLoadTextures))
                {
                    physicsModel = proxyModel.get();
                    hasCollisionProxy = true;
                    OutputDebugStringA(("[ImportShip] Collision proxy linked: " +
                        std::string(proxyIt->second->mName.C_Str()) + " -> " + name + "\n").c_str());
                }
                else
                {
                    OutputDebugStringA(("[ImportShip] Collision proxy load failed for " + name + "\n").c_str());
                    proxyModel.reset();
                }
            }
        }

        SpawnObjectDesc desc;
        desc.model = isCollisionNode
            ? nullptr
            : model.get();
        desc.name = name;

        // Blender object transform Р¶РёРІРµС‚ РІ aiNode, Р° РіРµРѕРјРµС‚СЂРёСЏ Р»РµР¶РёС‚ РІ aiMesh.
        // РџРѕСЌС‚РѕРјСѓ РїРѕР·РёС†РёСЋ/РїРѕРІРѕСЂРѕС‚/РјР°СЃС€С‚Р°Р± Р±РµСЂРµРј РёР· node world matrix.
        const TransformComponent transform = TransformFromMatrix(world);
        desc.position = transform.position;
        desc.rotation = transform.rotation;
        desc.scale = transform.scale;

        if (isRoomNode)
        {
            desc.addMeshRenderer = false;
            desc.addRigidBody = false;
            desc.meshCastsShadow = false;
        }
        else if (isPanelNode)
        {
            desc.addMeshRenderer = !kUseShipPanelVisualBatch;
            desc.addRigidBody = true;
            desc.meshCastsShadow = false;
            // РЈ РїР°РЅРµР»РµР№ Blender С‡Р°СЃС‚Рѕ СЃРјРµС‰С‘РЅ Р»РѕРєР°Р»СЊРЅС‹Р№ origin. TriangleMesh
            // С…СЂР°РЅРёС‚ СЌС‚Рѕ СЃРјРµС‰РµРЅРёРµ С‚РѕС‡РЅРѕ, РїРѕСЌС‚РѕРјСѓ РЅРµ РѕСЃС‚Р°РІР»СЏРµС‚ С‰РµР»РµР№ РІ РєРѕСЂРїСѓСЃРµ.
            desc.rigidBody.useSeparateCollisionMesh = true;
            desc.rigidBody.collisionModel = model.get();
            desc.rigidBody.shapeType = RigidBodyComponent::TriangleMesh;
            desc.rigidBody.useManualSize = false;
            desc.rigidBody.mass = 0.0f;
            desc.rigidBody.isDynamic = false;

            if (!kUseShipPanelVisualBatch)
            {
            auto* material = new MaterialClass();
            if (material->Initialize(device, L"ShipPanelMaterial"))
            {
                float glassOpacity = 0.15f;
                const bool isGlassPanel = NameLooksLikeGlass(name) || ModelLooksLikeGlass(model.get(), &glassOpacity);

                if (isGlassPanel)
                {
                    if (auto* pbrShader = ShaderManager::Get().GetShaderType(ShaderType::PBR))
                        material->SetShader(pbrShader);
                    else if (m_defaultMaterial && m_defaultMaterial->GetShader())
                        material->SetShader(m_defaultMaterial->GetShader());

                    material->SetGlass(true, glassOpacity);
                }
                else if (m_defaultMaterial && m_defaultMaterial->GetShader())
                {
                    material->SetShader(m_defaultMaterial->GetShader());
                }

                desc.material = material;
            }
            else
            {
                delete material;
            }
        }
            }


        else if (isGeneratorNode || isOxygenGeneratorNode)
        {
            desc.addInteraction = true;
            desc.interaction.prompt = isGeneratorNode
                ? "Access fusion generator"
                : "Access oxygen generator";
            desc.addRigidBody = true;
            desc.rigidBody.mass = 0.0f;
            desc.rigidBody.isDynamic = false;
            desc.rigidBody.shapeType = RigidBodyComponent::Box;
            if (hasCollisionProxy)
            {
                desc.rigidBody.useSeparateCollisionMesh = true;
                desc.rigidBody.collisionModel = physicsModel;
            }
            else
            {
                desc.rigidBody.useSeparateCollisionMesh = false;
                desc.rigidBody.collisionModel = nullptr;
                OutputDebugStringA(("[ImportShip] Oxygen generator collision proxy not found; using visual bounds box for " +
                    name + "\n").c_str());
            }
            auto* material = new MaterialClass();
            if (material->Initialize(
                device,
                isGeneratorNode ? L"ShipFusionGeneratorMaterial" : L"ShipOxygenGeneratorMaterial"))
            {
                if (m_defaultMaterial && m_defaultMaterial->GetShader())
                    material->SetShader(m_defaultMaterial->GetShader());

                if (isShipLightNode)
                {
                    material->SetEmissiveColor({ 1.0f, 0.72f, 0.24f });
                    material->SetEmissiveStrength(2.2f);
                    material->SetPBREnvironmentStrength(0.0f);
                }
                desc.material = material;
            }
            else
            {
                delete material;
            }
		}

        else if (isDoorNode)
        {
            desc.addInteraction = true;
            desc.interaction.prompt = "Open door";
            desc.addRigidBody = true;
            desc.rigidBody.mass = 0.0f;
            desc.rigidBody.isDynamic = false;
            if (hasCollisionProxy)
            {
                desc.rigidBody.useSeparateCollisionMesh = true;
                desc.rigidBody.collisionModel = physicsModel;
                desc.rigidBody.shapeType = RigidBodyComponent::TriangleMesh;
            }
            else
            {
                desc.addRigidBody = false;
                desc.rigidBody.useSeparateCollisionMesh = false;
                desc.rigidBody.collisionModel = nullptr;
                OutputDebugStringA(("[ImportShip] Door collision proxy not found; door frame physics disabled for " +
                    name + "\n").c_str());
            }

            auto* material = new MaterialClass();
            if (material->Initialize(device, L"ShipDoorMaterial"))
            {
                if (m_defaultMaterial && m_defaultMaterial->GetShader())
                    material->SetShader(m_defaultMaterial->GetShader());
                desc.material = material;
            }
            else
            {
                delete material;
            }
        }
        else if (isDoorLeafNode)
        {
            desc.addInteraction = true;
            desc.interaction.prompt = "Open door";
            desc.addRigidBody = true;
            desc.rigidBody.mass = 0.0f;
            desc.rigidBody.isDynamic = false;
            desc.rigidBody.isKinematic = true;
            if (hasCollisionProxy)
            {
                desc.rigidBody.useSeparateCollisionMesh = true;
                desc.rigidBody.collisionModel = physicsModel;
                desc.rigidBody.shapeType = RigidBodyComponent::ConvexHull;
            }
            else
            {
                desc.rigidBody.useSeparateCollisionMesh = false;
                desc.rigidBody.collisionModel = nullptr;
                desc.rigidBody.shapeType = RigidBodyComponent::Box;
                OutputDebugStringA(("[ImportShip] Door leaf collision proxy not found; using box fallback for " +
                    name + "\n").c_str());
            }
        }
        else if (isCollisionNode)
        {
            desc.addMeshRenderer = false;
            desc.meshCastsShadow = false;
            if (isDoorCollisionProxy || isStairCollisionProxy)
            {
                desc.active = false;
                desc.addRigidBody = false;
            }
            else
            {
                desc.addRigidBody = true;
                desc.rigidBody.useSeparateCollisionMesh = true;
                desc.rigidBody.collisionModel = model.get();
                desc.rigidBody.shapeType = RigidBodyComponent::TriangleMesh;
                desc.rigidBody.mass = 0.0f;
                desc.rigidBody.isDynamic = false;
            }
        }

        if (isStairBridgeNode)
        {
            desc.addRigidBody = true;
            desc.rigidBody.shapeType = RigidBodyComponent::TriangleMesh;
            desc.rigidBody.mass = 0.0f;
            desc.rigidBody.isDynamic = false;
            if (hasCollisionProxy)
            {
                desc.rigidBody.useSeparateCollisionMesh = true;
                desc.rigidBody.collisionModel = physicsModel;
            }
            else
            {
                desc.rigidBody.useSeparateCollisionMesh = false;
                desc.rigidBody.collisionModel = nullptr;
                OutputDebugStringA(("[ImportShip] Stair/bridge collision proxy not found; using visual mesh for " +
                    name + "\n").c_str());
            }
        }

        const bool needsGenericMaterial =
            !isRoomNode && !isCollisionNode && !isDoorNode && !isPanelNode &&
            !isGeneratorNode && !isOxygenGeneratorNode;
        if (needsGenericMaterial)
        {
            auto* material = new MaterialClass();
            if (material->Initialize(device, L"ShipPartMaterial"))
            {
                if (m_defaultMaterial && m_defaultMaterial->GetShader())
                    material->SetShader(m_defaultMaterial->GetShader());
                desc.material = material;
            }
            else
            {
                delete material;
            }
        }

        Entity part = SpawnObject(std::move(desc));

        if (isRoomNode)
        {
            RoomComponent room;
            room.roomName = name;
            model->GetBoundingBox(&room.localMin, &room.localMax);
            m_registry.AddComponent<RoomComponent>(part, std::move(room));
        }

        if (isDoorNode)
        {
            DoorComponent door;
            XMFLOAT3 doorMin{};
            XMFLOAT3 doorMax{};
            model->GetBoundingBox(&doorMin, &doorMax);
            float modelHeight = (doorMax.y - doorMin.y) * desc.scale.y;
            if (modelHeight < 0.0f)
                modelHeight = -modelHeight;
            if (modelHeight > 0.05f)
                door.openHeight = modelHeight;
            m_registry.AddComponent<DoorComponent>(part, std::move(door));
        }

        if (isPanelNode)
        {
            HullPanelComponent panel;
            panel.panelName = name;
            panel.roomName = TryParsePanelRoomName(name);
            panel.maxHealth = 100.0f;
            panel.health = panel.maxHealth;
            panel.maxBreaches = 3;
            panel.isExternal = true;
            m_registry.AddComponent<HullPanelComponent>(part, std::move(panel));
        }

        
        if (isGeneratorNode)
        {
            GeneratorComponent generator;
            generator.deuteriumKg = 500.0f;
            generator.tritiumKg = 500.0f;
            generator.maxDeuteriumKg = 500.0f;
            generator.maxTritiumKg = 500.0f;
            generator.maxOutputMw = 20.0f;
            generator.currentOutputMw = generator.maxOutputMw;
            generator.targetOutputMw = generator.maxOutputMw;
            generator.heat = generator.ambientTemperature;
            generator.isOnline = true;
            m_registry.AddComponent<GeneratorComponent>(part, generator);

            // РџРѕРїР°РґР°РЅРёСЏ РїРѕ СЂРµР°РєС‚РѕСЂСѓ РѕР±СЂР°Р±Р°С‚С‹РІР°РµС‚ СЃСѓС‰РµСЃС‚РІСѓСЋС‰Р°СЏ DamageSystem.
            m_registry.AddComponent<DamageableComponent>(
                part,
                DamageableComponent{ 1000.0f, 1000.0f, false });
            m_registry.AddComponent<RepairableComponent>(
                part,
                RepairableComponent{ 0.0f, 1.0f, 0.18f, 8.0f, false });
        }

        if (isOxygenGeneratorNode)
        {
            OxygenGeneratorComponent generator;
            generator.oxygenPerSecond = 0.05f;
            generator.powerUsage = 1.0f;
            m_registry.AddComponent<OxygenGeneratorComponent>(part, generator);

            // РџРѕРґРґРµСЂР¶Р°РЅРёРµ РІРѕР·РґСѓС…Р° РІР°Р¶РЅРµРµ РѕСЃРІРµС‰РµРЅРёСЏ РїСЂРё РґРµС„РёС†РёС‚Рµ РјРѕС‰РЅРѕСЃС‚Рё.
            PowerConsumerComponent consumer;
            consumer.requiredMw = generator.powerUsage;
            consumer.priority = PowerPriority::Critical;
            m_registry.AddComponent<PowerConsumerComponent>(part, consumer);
            m_registry.AddComponent<DamageableComponent>(
                part,
                DamageableComponent{ 150.0f, 150.0f, false });
            m_registry.AddComponent<RepairableComponent>(
                part,
                RepairableComponent{ 0.0f, 1.0f, 0.30f, 6.0f, false });
        }

        if (isShipLightNode)
        {
            LightComponent light;
            light.color = { 1.0f, 0.82f, 0.55f, 1.0f };
            light.radius = 12.0f;
            light.enabled = true;
            light.isEditorGizmo = false;
            XMFLOAT3 localMin{};
            XMFLOAT3 localMax{};
            model->GetBoundingBox(&localMin, &localMax);
            // пїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅ пїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅ, пїЅ пїЅпїЅ пїЅ пїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅ.
            // SHIP_LIGHT meshes in SpaceShip.glb are ceiling bars: their lens is
            // the lower face and the emitted cone points down along local -Y.
            light.offset = { 0.0f, localMin.y, 0.0f };
            m_registry.AddComponent<LightComponent>(part, light);

            SpotLightComponent spot;
            spot.localDirection = { 0.0f, -1.0f, 0.0f };
            spot.innerAngleDegrees = 22.0f;
            spot.outerAngleDegrees = 38.0f;
            m_registry.AddComponent<SpotLightComponent>(part, spot);

            // РћСЃРІРµС‰РµРЅРёРµ РІС‹РєР»СЋС‡Р°РµС‚СЃСЏ РїРµСЂРІС‹Рј, РєРѕРіРґР° РјРѕС‰РЅРѕСЃС‚Рё РЅРµ С…РІР°С‚Р°РµС‚.
            PowerConsumerComponent consumer;
            consumer.requiredMw = 0.05f;
            consumer.priority = PowerPriority::Low;
            m_registry.AddComponent<PowerConsumerComponent>(part, consumer);
            m_registry.AddComponent<DamageableComponent>(
                part,
                DamageableComponent{ 25.0f, 25.0f, false });
            m_registry.AddComponent<RepairableComponent>(
                part,
                RepairableComponent{ 0.0f, 1.0f, 0.55f, 4.0f, false });

            OutputDebugStringA(("[ImportShip] Light component added to " + name + "\n").c_str());
        }

        m_registry.AddComponent<ShipPartComponent>(part, ShipPartComponent{ shipRoot });
        m_registry.AddComponent<EditorSelectionComponent>(part, EditorSelectionComponent{ false, shipRoot });

        if (proxyModel)
            m_importedShipModels.push_back(std::move(proxyModel));
        m_importedShipModels.push_back(std::move(model));
    }
}

struct SceneManager::AsyncShipImportState
{
    enum class Phase
    {
        Queued,
        ReadingFile,
        PreparingCpuData,
        CpuReady,
        UploadingNodes,
        UploadingPanelBatches,
        Finalizing,
        Ready,
        Error
    };

    std::atomic<Phase> phase{ Phase::Queued };
    std::atomic<float> cpuProgress{ 0.01f };
    std::string path;
    std::string error;
    std::string currentItem;
    std::unique_ptr<Assimp::Importer> importer;
    const aiScene* scene = nullptr;
    std::vector<ShipImportNodeTask> nodes;
    std::unordered_map<std::string, aiNode*> collisionProxyNodes;
    std::vector<ShipPanelMaterialGeometry> panelGeometry;
    size_t nextNode = 0;
    size_t nextPanel = 0;
    size_t totalPanelBatches = 0;
    size_t completedPanelBatches = 0;
    Entity shipRoot = INVALID_ENTITY;
};

void SceneManager::ImportShipNodeRecursive(
    const aiScene* scene,
    aiNode* node,
    const DirectX::XMMATRIX& parentWorld,
    Entity shipRoot,
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    const std::unordered_map<std::string, aiNode*>* collisionProxyNodes)
{
    if (!scene || !node)
        return;

    ImportShipNode(
        scene, node, parentWorld, shipRoot, device, context, collisionProxyNodes);

    const DirectX::XMMATRIX local = ConvertAssimpMatrix(node->mTransformation);
    const DirectX::XMMATRIX world = local * parentWorld;

    // Even service nodes may have real ship parts below them.
    for (unsigned int i = 0; i < node->mNumChildren; ++i)
    {
        ImportShipNodeRecursive(
            scene,
            node->mChildren[i],
            world,
            shipRoot,
            device,
            context,
            collisionProxyNodes);
    }
}

bool SceneManager::HasImportedShip()
{
    return m_registry.GetComponents<ShipRootComponent>().GetCount() > 0;
}

bool SceneManager::RequestShipImportAsync(const std::string& path)
{
    if (!m_assetLoadSystem || path.empty() || HasImportedShip())
        return false;

    if (m_asyncShipImport)
    {
        const auto phase = m_asyncShipImport->phase.load(std::memory_order_acquire);
        if (phase != AsyncShipImportState::Phase::Ready &&
            phase != AsyncShipImportState::Phase::Error)
            return false;
    }

    auto state = std::make_shared<AsyncShipImportState>();
    state->path = path;
    m_asyncShipImport = state;

    const bool submitted = m_assetLoadSystem->GetJobSystem().SubmitJob([state]()
        {
            state->phase.store(
                AsyncShipImportState::Phase::ReadingFile,
                std::memory_order_release);
            state->cpuProgress.store(0.04f, std::memory_order_relaxed);

            auto importer = std::make_unique<Assimp::Importer>();
            const aiScene* scene = importer->ReadFile(
                state->path,
                aiProcess_Triangulate |
                aiProcess_ConvertToLeftHanded |
                aiProcess_CalcTangentSpace |
                aiProcess_GenSmoothNormals);

            if (!scene || !scene->mRootNode ||
                (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE))
            {
                state->error = importer->GetErrorString();
                state->phase.store(
                    AsyncShipImportState::Phase::Error,
                    std::memory_order_release);
                return;
            }

            state->phase.store(
                AsyncShipImportState::Phase::PreparingCpuData,
                std::memory_order_release);
            state->cpuProgress.store(0.18f, std::memory_order_relaxed);

            std::vector<ShipImportNodeTask> nodes;
            nodes.reserve(scene->mNumMeshes + 1);
            CollectShipImportNodeTasks(
                scene->mRootNode,
                DirectX::XMMatrixIdentity(),
                nodes);

            // A single multi-million-vertex object defeats frame staging because
            // InitializeFromMeshes is one indivisible operation. Reject such an
            // authoring error with the exact Blender object name instead of
            // making the editor look permanently frozen.
            constexpr uint64_t kMaxVerticesPerShipNode = 4000000;
            constexpr uint64_t kMaxFacesPerShipNode = 5000000;
            for (const auto& task : nodes)
            {
                uint64_t vertexCount = 0;
                uint64_t faceCount = 0;
                for (unsigned int slot = 0; slot < task.node->mNumMeshes; ++slot)
                {
                    const unsigned int meshIndex = task.node->mMeshes[slot];
                    if (meshIndex >= scene->mNumMeshes || !scene->mMeshes[meshIndex])
                        continue;
                    vertexCount += scene->mMeshes[meshIndex]->mNumVertices;
                    faceCount += scene->mMeshes[meshIndex]->mNumFaces;
                }

                if (vertexCount > kMaxVerticesPerShipNode ||
                    faceCount > kMaxFacesPerShipNode)
                {
                    state->error =
                        "Ship object is too dense for one-frame GPU/ECS installation: " +
                        std::string(task.node->mName.C_Str()) +
                        " (vertices=" + std::to_string(vertexCount) +
                        ", faces=" + std::to_string(faceCount) +
                        "). Optimize this object in Blender or use SpaceShip_optimized.glb.";
                    state->phase.store(
                        AsyncShipImportState::Phase::Error,
                        std::memory_order_release);
                    return;
                }
            }

            std::unordered_map<std::string, aiNode*> collisionProxyNodes;
            CollectShipCollisionProxyNodes(scene->mRootNode, collisionProxyNodes);

            std::vector<ShipPanelMaterialGeometry> panelGeometry;
            if (kUseShipPanelVisualBatch)
            {
                panelGeometry.resize((std::max)(1u, scene->mNumMaterials));
                AppendShipPanelGeometry(
                    scene,
                    scene->mRootNode,
                    DirectX::XMMatrixIdentity(),
                    panelGeometry);
            }

            state->cpuProgress.store(0.32f, std::memory_order_relaxed);
            state->importer = std::move(importer);
            state->scene = scene;
            state->nodes = std::move(nodes);
            state->collisionProxyNodes = std::move(collisionProxyNodes);
            state->panelGeometry = std::move(panelGeometry);
            for (const auto& geometry : state->panelGeometry)
            {
                if (!geometry.vertices.empty() && !geometry.indices.empty())
                    ++state->totalPanelBatches;
            }
            state->phase.store(
                AsyncShipImportState::Phase::CpuReady,
                std::memory_order_release);
        });

    if (!submitted)
    {
        state->error = "JobSystem rejected the ship import task";
        state->phase.store(
            AsyncShipImportState::Phase::Error,
            std::memory_order_release);
        return false;
    }

    return true;
}

void SceneManager::UpdateAsyncShipImport(
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    float mainThreadBudgetMilliseconds)
{
    auto state = m_asyncShipImport;
    if (!state || !device || !context)
        return;

    auto phase = state->phase.load(std::memory_order_acquire);
    if (phase == AsyncShipImportState::Phase::Error ||
        phase == AsyncShipImportState::Phase::Ready ||
        phase == AsyncShipImportState::Phase::Queued ||
        phase == AsyncShipImportState::Phase::ReadingFile ||
        phase == AsyncShipImportState::Phase::PreparingCpuData)
        return;

    if (phase == AsyncShipImportState::Phase::CpuReady)
    {
        if (!state->scene || !state->scene->mRootNode)
        {
            state->error = "Worker returned an empty ship scene";
            state->phase.store(AsyncShipImportState::Phase::Error, std::memory_order_release);
            return;
        }

        state->shipRoot = m_registry.CreateEntity();
        m_registry.AddComponent<TransformComponent>(state->shipRoot, TransformComponent{});
        m_registry.AddComponent<NameComponent>(state->shipRoot, NameComponent{ "ShipRoot" });
        m_registry.AddComponent<ActiveComponent>(state->shipRoot, {});
        m_registry.AddComponent<ShipRootComponent>(
            state->shipRoot,
            ShipRootComponent{ XMFLOAT3{ 0, 0, 0 }, state->path });
        m_registry.AddComponent<PowerGridComponent>(state->shipRoot, PowerGridComponent{});
        m_registry.AddComponent<EditorSelectionComponent>(
            state->shipRoot,
            EditorSelectionComponent{ true, INVALID_ENTITY });

        OutputDebugStringA("=== Async Import Ship GLB ===\n");
        state->phase.store(
            AsyncShipImportState::Phase::UploadingNodes,
            std::memory_order_release);
        if (!state->nodes.empty() && state->nodes.front().node)
            state->currentItem = state->nodes.front().node->mName.C_Str();
        phase = AsyncShipImportState::Phase::UploadingNodes;
    }

    using Clock = std::chrono::steady_clock;
    const auto frameSliceStart = Clock::now();
    const float budgetMs = (std::max)(0.25f, mainThreadBudgetMilliseconds);
    auto budgetExpired = [&]()
        {
            return std::chrono::duration<float, std::milli>(
                Clock::now() - frameSliceStart).count() >= budgetMs;
        };

    if (phase == AsyncShipImportState::Phase::UploadingNodes)
    {
        do
        {
            if (state->nextNode >= state->nodes.size())
                break;

            const auto& task = state->nodes[state->nextNode++];
            state->currentItem = task.node ? task.node->mName.C_Str() : "<unnamed node>";
            ImportShipNode(
                state->scene,
                task.node,
                task.parentWorld,
                state->shipRoot,
                device,
                context,
                &state->collisionProxyNodes);

            if (state->nextNode < state->nodes.size() &&
                state->nodes[state->nextNode].node)
                state->currentItem = state->nodes[state->nextNode].node->mName.C_Str();
        } while (!budgetExpired());

        if (state->nextNode < state->nodes.size())
            return;

        state->phase.store(
            AsyncShipImportState::Phase::UploadingPanelBatches,
            std::memory_order_release);
        phase = AsyncShipImportState::Phase::UploadingPanelBatches;
    }

    if (phase == AsyncShipImportState::Phase::UploadingPanelBatches)
    {
        while (state->nextPanel < state->panelGeometry.size())
        {
            const size_t materialIndex = state->nextPanel++;
            state->currentItem = "Panel material batch " + std::to_string(materialIndex);
            auto& geometry = state->panelGeometry[materialIndex];
            if (geometry.vertices.empty() || geometry.indices.empty())
            {
                if (budgetExpired())
                    return;
                continue;
            }

            const aiMaterial* sourceMaterial = materialIndex < state->scene->mNumMaterials
                ? state->scene->mMaterials[materialIndex]
                : nullptr;
            aiString sourceMaterialName;
            if (sourceMaterial)
                sourceMaterial->Get(AI_MATKEY_NAME, sourceMaterialName);
            const std::string materialName = sourceMaterialName.length > 0
                ? sourceMaterialName.C_Str()
                : "Default";

            auto panelVisualModel = std::make_unique<AssimpModelClass>();
            if (panelVisualModel->InitializeFromGeometryWithMaterial(
                device,
                context,
                state->scene,
                sourceMaterial,
                std::move(geometry.vertices),
                std::move(geometry.indices),
                true))
            {
                SpawnObjectDesc desc;
                desc.name = "ShipPanelVisualBatch_" + materialName;
                desc.model = panelVisualModel.get();
                desc.addMeshRenderer = true;
                desc.addRigidBody = false;
                float glassOpacity = 0.15f;
                const bool isGlass = NameLooksLikeGlass(materialName) ||
                    ModelLooksLikeGlass(panelVisualModel.get(), &glassOpacity);
                desc.meshCastsShadow = !isGlass;

                auto* material = new MaterialClass();
                if (material->Initialize(device, L"ShipPanelBatchMaterial"))
                {
                    if (auto* pbrShader = ShaderManager::Get().GetShaderType(ShaderType::PBR))
                        material->SetShader(pbrShader);
                    else if (m_defaultMaterial && m_defaultMaterial->GetShader())
                        material->SetShader(m_defaultMaterial->GetShader());
                    material->SetGlass(isGlass, isGlass ? glassOpacity : 1.0f);
                    material->SetPBREnvironmentStrength(isGlass ? 0.35f : 0.0f);
                    desc.material = material;
                }
                else
                {
                    delete material;
                }

                const Entity visualBatch = SpawnObject(std::move(desc));
                m_registry.AddComponent<ShipPanelVisualBatchComponent>(
                    visualBatch,
                    ShipPanelVisualBatchComponent{ state->shipRoot });
                m_registry.AddComponent<ShipPartComponent>(
                    visualBatch,
                    ShipPartComponent{ state->shipRoot });
                m_registry.AddComponent<EditorSelectionComponent>(
                    visualBatch,
                    EditorSelectionComponent{ false, state->shipRoot });
                m_importedShipModels.push_back(std::move(panelVisualModel));
            }

            ++state->completedPanelBatches;

            // Never create two large material batches in one frame.
            return;
        }

        state->phase.store(
            AsyncShipImportState::Phase::Finalizing,
            std::memory_order_release);
        phase = AsyncShipImportState::Phase::Finalizing;
    }

    if (phase == AsyncShipImportState::Phase::Finalizing)
    {
        ResolveDoorRoomLinks();
        auto& panels = m_registry.GetComponents<HullPanelComponent>();
        for (auto& [panelEntity, panel] : panels)
        {
            if (!panel.roomName.empty())
                panel.room = FindRoomByName(panel.roomName);
        }
        AssignRoomMembership();
        ResolveRepairZones();
        ConnectDefaultShipRooms();

        state->collisionProxyNodes.clear();
        state->nodes.clear();
        state->panelGeometry.clear();
        state->scene = nullptr;
        state->importer.reset();
        state->phase.store(AsyncShipImportState::Phase::Ready, std::memory_order_release);
        state->currentItem.clear();
        OutputDebugStringA("=== Async Import Ship Finished ===\n");
    }
}

void SceneManager::AssignRoomMembership()
{
    auto& rooms =
        m_registry.GetComponents<RoomComponent>();

    for (auto& [entity, transform] :
        m_registry.GetComponents<TransformComponent>())
    {
        // Комнату самой себе не назначаем.
        if (m_registry.HasComponent<RoomComponent>(entity))
            continue;

        // Уже назначено вручную или ранее.
        if (m_registry.HasComponent<RoomMembershipComponent>(entity))
            continue;

        // Пока назначаем комнату только gameplay-объектам,
        // которым это действительно нужно.
        const bool needsRoom =
            m_registry.HasComponent<GeneratorComponent>(entity) ||
            m_registry.HasComponent<OxygenGeneratorComponent>(entity) ||
            m_registry.HasComponent<LightComponent>(entity) ||
            m_registry.HasComponent<DamageableComponent>(entity);

        if (!needsRoom)
            continue;


        Entity foundRoom = INVALID_ENTITY;

        for (auto& [roomEntity, room] : rooms)
        {
            const auto* roomTransform =
                m_registry.GetComponent<TransformComponent>(
                    roomEntity);

            if (!roomTransform)
                continue;


            // Переводим мировую позицию объекта
            // в локальное пространство текущей комнаты.
            const XMMATRIX inverseRoom =
                XMMatrixInverse(
                    nullptr,
                    roomTransform->GetWorldMatrix());

            const XMVECTOR worldPosition =
                XMLoadFloat3(
                    &transform.position);

            const XMVECTOR localVector =
                XMVector3TransformCoord(
                    worldPosition,
                    inverseRoom);

            XMFLOAT3 localPosition{};

            XMStoreFloat3(
                &localPosition,
                localVector);


            const bool inside =
                localPosition.x >= room.localMin.x &&
                localPosition.x <= room.localMax.x &&

                localPosition.y >= room.localMin.y &&
                localPosition.y <= room.localMax.y &&

                localPosition.z >= room.localMin.z &&
                localPosition.z <= room.localMax.z;


            if (inside)
            {
                foundRoom = roomEntity;
                break;
            }
        }


        if (foundRoom != INVALID_ENTITY)
        {
            m_registry.AddComponent<RoomMembershipComponent>(
                entity,
                RoomMembershipComponent{ foundRoom });

#ifdef _DEBUG

            const auto* objectName =
                m_registry.GetComponent<NameComponent>(entity);

            const auto* room =
                m_registry.GetComponent<RoomComponent>(foundRoom);

            if (objectName && room)
            {
                char buffer[512]{};

                sprintf_s(
                    buffer,
                    "[RoomMembership] %s -> %s\n",
                    objectName->name.c_str(),
                    room->roomName.c_str());

                OutputDebugStringA(buffer);
            }

#endif
        }
        else
        {
#ifdef _DEBUG

            const auto* objectName =
                m_registry.GetComponent<NameComponent>(entity);

            if (objectName)
            {
                char buffer[512]{};

                sprintf_s(
                    buffer,
                    "[RoomMembership] NO ROOM FOUND: %s entity=%u\n",
                    objectName->name.c_str(),
                    static_cast<unsigned>(entity));

                OutputDebugStringA(buffer);
            }

#endif
        }
    }
}
AsyncShipImportSnapshot SceneManager::GetAsyncShipImportSnapshot() const
{
    AsyncShipImportSnapshot snapshot;
    const auto state = m_asyncShipImport;
    if (!state)
        return snapshot;

    const auto phase = state->phase.load(std::memory_order_acquire);
    snapshot.path = state->path;
    snapshot.failed = phase == AsyncShipImportState::Phase::Error;
    snapshot.finished = phase == AsyncShipImportState::Phase::Ready;
    snapshot.active = !snapshot.failed && !snapshot.finished;
    snapshot.error = snapshot.failed ? state->error : std::string{};
    snapshot.currentItem = state->currentItem;

    if (phase >= AsyncShipImportState::Phase::CpuReady)
    {
        snapshot.totalItems = state->nodes.size() + state->totalPanelBatches + 1;
        snapshot.completedItems =
            (std::min)(state->nextNode, state->nodes.size()) +
            state->completedPanelBatches;
    }

    switch (phase)
    {
    case AsyncShipImportState::Phase::Queued:
        snapshot.stage = "Queued";
        snapshot.progress = 0.01f;
        break;
    case AsyncShipImportState::Phase::ReadingFile:
        snapshot.stage = "Reading GLB with Assimp (worker thread)";
        snapshot.progress = state->cpuProgress.load(std::memory_order_relaxed);
        break;
    case AsyncShipImportState::Phase::PreparingCpuData:
        snapshot.stage = "Preparing nodes, collision proxies and panel batches";
        snapshot.progress = state->cpuProgress.load(std::memory_order_relaxed);
        break;
    case AsyncShipImportState::Phase::CpuReady:
        snapshot.stage = "CPU data ready";
        snapshot.progress = 0.34f;
        break;
    case AsyncShipImportState::Phase::UploadingNodes:
        snapshot.stage = "Creating ship parts, physics and GPU buffers";
        snapshot.progress = snapshot.totalItems > 0
            ? 0.34f + 0.60f * static_cast<float>(snapshot.completedItems) /
                static_cast<float>(snapshot.totalItems)
            : 0.90f;
        break;
    case AsyncShipImportState::Phase::UploadingPanelBatches:
        snapshot.stage = "Creating panel material batches";
        snapshot.progress = snapshot.totalItems > 0
            ? 0.34f + 0.60f * static_cast<float>(snapshot.completedItems) /
                static_cast<float>(snapshot.totalItems)
            : 0.94f;
        break;
    case AsyncShipImportState::Phase::Finalizing:
        snapshot.stage = "Linking rooms, doors and hull panels";
        snapshot.progress = 0.97f;
        break;
    case AsyncShipImportState::Phase::Ready:
        snapshot.stage = "Ship ready";
        snapshot.progress = 1.0f;
        break;
    case AsyncShipImportState::Phase::Error:
        snapshot.stage = "Ship import failed";
        snapshot.progress = 0.0f;
        break;
    }
    return snapshot;
}

Entity SceneManager::ImportShipFromGlb(
    const std::string& path,
    ID3D11Device* device,
    ID3D11DeviceContext* context)
{
    if (!device || !context || path.empty())
        return INVALID_ENTITY;

    Assimp::Importer importer;

    const aiScene* scene = importer.ReadFile(
        path,
        aiProcess_Triangulate |
        aiProcess_ConvertToLeftHanded |
        aiProcess_CalcTangentSpace |
        aiProcess_GenSmoothNormals);

    if (!scene || !scene->mRootNode || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE))
    {
        OutputDebugStringA(("ImportShipFromGlb failed: " +
            std::string(importer.GetErrorString()) + "\n").c_str());
        return INVALID_ENTITY;
    }

    Entity shipRoot = m_registry.CreateEntity();

    m_registry.AddComponent<TransformComponent>(shipRoot, TransformComponent{});
    m_registry.AddComponent<NameComponent>(shipRoot, NameComponent{ "ShipRoot" });
    m_registry.AddComponent<ActiveComponent>(shipRoot, {});
    m_registry.AddComponent<ShipRootComponent>(shipRoot, ShipRootComponent{ XMFLOAT3{ 0, 0, 0 }, path });
    m_registry.AddComponent<PowerGridComponent>(shipRoot, PowerGridComponent{});
    m_registry.AddComponent<EditorSelectionComponent>(shipRoot, EditorSelectionComponent{ true, INVALID_ENTITY });

    OutputDebugStringA("=== Import Ship GLB ===\n");

    std::unordered_map<std::string, aiNode*> collisionProxyNodes;
    CollectShipCollisionProxyNodes(scene->mRootNode, collisionProxyNodes);

    std::future<std::vector<ShipPanelMaterialGeometry>> panelGeometryFuture;
    bool panelGeometryRunsAsync = false;

    if (kUseShipPanelVisualBatch && m_assetLoadSystem)
    {
        // aiScene РїРѕСЃР»Рµ ReadFile Р±РѕР»СЊС€Рµ РЅРµ РјРµРЅСЏРµС‚СЃСЏ. Worker С‡РёС‚Р°РµС‚ С‚РѕР»СЊРєРѕ РµРіРѕ
        // РІРµСЂС€РёРЅС‹ Рё РёРЅРґРµРєСЃС‹, РїРѕРєР° РіР»Р°РІРЅС‹Р№ РїРѕС‚РѕРє СЃРѕР·РґР°С‘С‚ ECS-С‡Р°СЃС‚Рё РєРѕСЂР°Р±Р»СЏ.
        auto resultPromise = std::make_shared<
            std::promise<std::vector<ShipPanelMaterialGeometry>>>();
        panelGeometryFuture = resultPromise->get_future();

        panelGeometryRunsAsync = m_assetLoadSystem->GetJobSystem().SubmitJob(
            [scene, resultPromise]()
            {
                try
                {
                    std::vector<ShipPanelMaterialGeometry> geometry(
                        (std::max)(1u, scene->mNumMaterials));
                    AppendShipPanelGeometry(
                        scene,
                        scene->mRootNode,
                        DirectX::XMMatrixIdentity(),
                        geometry);
                    resultPromise->set_value(std::move(geometry));
                }
                catch (...)
                {
                    resultPromise->set_exception(std::current_exception());
                }
            });
    }

    ImportShipNodeRecursive(
        scene,
        scene->mRootNode,
        DirectX::XMMatrixIdentity(),
        shipRoot,
        device,
        context,
        &collisionProxyNodes);

    if (kUseShipPanelVisualBatch)
    {
        std::vector<ShipPanelMaterialGeometry> panelGeometry;
        if (panelGeometryRunsAsync)
        {
            // Р—РґРµСЃСЊ Р¶РґС‘Рј С‚РѕР»СЊРєРѕ CPU-РіРµРѕРјРµС‚СЂРёСЋ. DirectX-РІС‹Р·РѕРІС‹ РЅРёР¶Рµ РѕСЃС‚Р°СЋС‚СЃСЏ
            // СЃС‚СЂРѕРіРѕ РІ РіР»Р°РІРЅРѕРј РїРѕС‚РѕРєРµ.
            panelGeometry = panelGeometryFuture.get();
        }
        else
        {
            panelGeometry.resize((std::max)(1u, scene->mNumMaterials));
            AppendShipPanelGeometry(
                scene,
                scene->mRootNode,
                DirectX::XMMatrixIdentity(),
                panelGeometry);
        }

        for (unsigned int materialIndex = 0; materialIndex < panelGeometry.size(); ++materialIndex)
        {
            auto& geometry = panelGeometry[materialIndex];
            if (geometry.vertices.empty() || geometry.indices.empty())
                continue;

            const aiMaterial* sourceMaterial = materialIndex < scene->mNumMaterials
                ? scene->mMaterials[materialIndex]
                : nullptr;
            aiString sourceMaterialName;
            if (sourceMaterial)
                sourceMaterial->Get(AI_MATKEY_NAME, sourceMaterialName);
            const std::string materialName = sourceMaterialName.length > 0
                ? sourceMaterialName.C_Str()
                : "Default";

            auto panelVisualModel = std::make_unique<AssimpModelClass>();
            if (!panelVisualModel->InitializeFromGeometryWithMaterial(
                device,
                context,
                scene,
                sourceMaterial,
                std::move(geometry.vertices),
                std::move(geometry.indices),
                true))
            {
                OutputDebugStringA((
                    "[ImportShip] Failed to build panel material batch: " +
                    materialName + "\n").c_str());
                continue;
            }

            SpawnObjectDesc desc;
            desc.name = "ShipPanelVisualBatch_" + materialName;
            desc.model = panelVisualModel.get();
            desc.addMeshRenderer = true;
            desc.addRigidBody = false;
            float glassOpacity = 0.15f;
            const bool isGlassPanelBatch =
                NameLooksLikeGlass(materialName) ||
                NameLooksLikeGlass(desc.name) ||
                ModelLooksLikeGlass(panelVisualModel.get(), &glassOpacity);
            desc.meshCastsShadow = !isGlassPanelBatch;

            auto* material = new MaterialClass();
            if (material->Initialize(device, L"ShipPanelBatchMaterial"))
            {
                if (auto* pbrShader = ShaderManager::Get().GetShaderType(ShaderType::PBR))
                    material->SetShader(pbrShader);
                else if (m_defaultMaterial && m_defaultMaterial->GetShader())
                    material->SetShader(m_defaultMaterial->GetShader());

                material->SetGlass(isGlassPanelBatch, isGlassPanelBatch ? glassOpacity : 1.0f);
                material->SetPBREnvironmentStrength(isGlassPanelBatch ? 0.35f : 0.0f);
                desc.material = material;
            }
            else
            {
                delete material;
            }

            Entity visualBatch = SpawnObject(std::move(desc));
            m_registry.AddComponent<ShipPanelVisualBatchComponent>(
                visualBatch,
                ShipPanelVisualBatchComponent{ shipRoot });
            m_registry.AddComponent<ShipPartComponent>(visualBatch, ShipPartComponent{ shipRoot });
            m_registry.AddComponent<EditorSelectionComponent>(
                visualBatch,
                EditorSelectionComponent{ false, shipRoot });
            m_importedShipModels.push_back(std::move(panelVisualModel));

            char batchDebug[256];
            sprintf_s(
                batchDebug,
                "[ImportShip] Panel batch material=%s index=%u glass=%d opacity=%.2f\n",
                materialName.c_str(),
                materialIndex,
                isGlassPanelBatch ? 1 : 0,
                glassOpacity);
            OutputDebugStringA(batchDebug);
        }
    }
    ResolveDoorRoomLinks();

    auto& panels = m_registry.GetComponents<HullPanelComponent>();
    for (auto& [panelEntity, panel] : panels)
    {
        if (!panel.roomName.empty())
            panel.room = FindRoomByName(panel.roomName);
    }

    AssignRoomMembership();
    ResolveRepairZones();

    OutputDebugStringA("=== Import Ship Finished ===\n");

    return shipRoot;
}
Entity SceneManager::FindRoomByName(const std::string& roomName)
{
    const std::string targetKey = NormalizeRoomLookupKey(roomName);

    auto& rooms = m_registry.GetComponents<RoomComponent>();
    for (auto& [entity, room] : rooms)
    {
        if (NormalizeRoomLookupKey(room.roomName) == targetKey)
            return entity;
    }

    return INVALID_ENTITY;
}

bool SceneManager::ConnectRooms(Entity roomA, Entity roomB)
{
    if (roomA == INVALID_ENTITY || roomB == INVALID_ENTITY || roomA == roomB)
        return false;

    auto* a = m_registry.GetComponent<RoomComponent>(roomA);
    auto* b = m_registry.GetComponent<RoomComponent>(roomB);
    if (!a || !b)
        return false;

    auto addUnique = [](std::vector<Entity>& rooms, Entity room)
        {
            if (std::find(rooms.begin(), rooms.end(), room) == rooms.end())
                rooms.push_back(room);
        };

    addUnique(a->connectedRooms, roomB);
    addUnique(b->connectedRooms, roomA);

    return true;
}

bool SceneManager::ConnectRoomsByName(const std::string& roomAName, const std::string& roomBName)
{
    Entity roomA = FindRoomByName(roomAName);
    Entity roomB = FindRoomByName(roomBName);

    if (!ConnectRooms(roomA, roomB))
        return false;

    return true;
}
void SceneManager::ConnectDefaultShipRooms()
{
    // TEMPORARY AI PORTAL.
    //
    // The current high-level NPC graph treats only DoorComponent as an edge.
    // Until StairLinkComponent is implemented, represent the physical staircase
    // between GUN and CAPTAIN as an invisible, permanently open door. It has no
    // renderer, collision or interaction and can be removed by deleting the
    // TEMP_AI_DOOR_GUN_CAPTAIN entity (and this block) later.
    const Entity gunRoom = FindRoomByName("SHIP_ROOM_GUN");
    const Entity captainRoom = FindRoomByName("SHIP_ROOM_CAPTAIN");
    if (gunRoom == INVALID_ENTITY || captainRoom == INVALID_ENTITY)
    {
        OutputDebugStringA(
            "[Rooms] Temporary GUN <-> CAPTAIN AI door was not created: room missing.\n");
        return;
    }

    // Idempotence: a real door added later automatically wins and prevents a
    // duplicate virtual edge. This also makes repeated import/load calls safe.
    for (auto& [doorEntity, door] : m_registry.GetComponents<DoorComponent>())
    {
        const bool samePair =
            (door.roomA == gunRoom && door.roomB == captainRoom) ||
            (door.roomA == captainRoom && door.roomB == gunRoom);
        if (samePair)
        {
            ConnectRooms(gunRoom, captainRoom);
            return;
        }
    }

    auto roomCenter = [&](Entity roomEntity, DirectX::XMFLOAT3& out) -> bool
    {
        const auto* room = m_registry.GetComponent<RoomComponent>(roomEntity);
        const auto* transform = m_registry.GetComponent<TransformComponent>(roomEntity);
        if (!room || !transform)
            return false;

        const DirectX::XMFLOAT3 localCenter = {
            (room->localMin.x + room->localMax.x) * 0.5f,
            (room->localMin.y + room->localMax.y) * 0.5f,
            (room->localMin.z + room->localMax.z) * 0.5f
        };
        const DirectX::XMVECTOR worldCenter = DirectX::XMVector3TransformCoord(
            DirectX::XMLoadFloat3(&localCenter), transform->GetWorldMatrix());
        DirectX::XMStoreFloat3(&out, worldCenter);
        return true;
    };

    DirectX::XMFLOAT3 gunCenter{};
    DirectX::XMFLOAT3 captainCenter{};
    if (!roomCenter(gunRoom, gunCenter) || !roomCenter(captainRoom, captainCenter))
        return;

    const DirectX::XMFLOAT3 roomsMidpoint = {
        (gunCenter.x + captainCenter.x) * 0.5f,
        (gunCenter.y + captainCenter.y) * 0.5f,
        (gunCenter.z + captainCenter.z) * 0.5f
    };

    // Put the virtual door on the closest imported STAIR/BRIDGE node. This is
    // much more useful than the midpoint of two vertically stacked room AABBs:
    // the NPC first walks to the real staircase and then continues to CAPTAIN.
    DirectX::XMFLOAT3 portalPosition = roomsMidpoint;
    float bestDistanceSq = (std::numeric_limits<float>::max)();
    for (auto& [entity, name] : m_registry.GetComponents<NameComponent>())
    {
        if (!IsNameShipStairBridge(name.name))
            continue;

        const auto* transform = m_registry.GetComponent<TransformComponent>(entity);
        if (!transform)
            continue;

        const float dx = transform->position.x - roomsMidpoint.x;
        const float dy = transform->position.y - roomsMidpoint.y;
        const float dz = transform->position.z - roomsMidpoint.z;
        const float distanceSq = dx * dx + dy * dy + dz * dz;
        if (distanceSq < bestDistanceSq)
        {
            bestDistanceSq = distanceSq;
            portalPosition = transform->position;
        }
    }

    const Entity virtualDoor = m_registry.CreateEntity();
    m_registry.AddComponent<TransformComponent>(
        virtualDoor,
        TransformComponent{ portalPosition, {}, { 1.0f, 1.0f, 1.0f } });
    m_registry.AddComponent<NameComponent>(
        virtualDoor,
        NameComponent{ "TEMP_AI_DOOR_GUN_CAPTAIN" });
    m_registry.AddComponent<ActiveComponent>(virtualDoor, ActiveComponent{});

    DoorComponent door;
    door.roomA = gunRoom;
    door.roomB = captainRoom;
    door.isOpen = true;
    door.isBroken = true;
    door.requiresPower = false;
    door.openProgress = 1.0f;
    door.openSpeed = 0.0f;
    door.openHeight = 0.0f;
    door.flowRate = 0.0f;
    door.closedPosition = portalPosition;
    door.positionInitialized = true;
    m_registry.AddComponent<DoorComponent>(virtualDoor, door);

    ConnectRooms(gunRoom, captainRoom);
    OutputDebugStringA(
        "[Rooms] Created temporary always-open AI door: GUN <-> CAPTAIN.\n");
}
void SceneManager::ResolveDoorRoomLinks()
{
    auto& doors = m_registry.GetComponents<DoorComponent>();

    for (auto& [doorEntity, door] : doors)
    {
        auto* name = m_registry.GetComponent<NameComponent>(doorEntity);
        if (!name)
            continue;

        std::string roomAName;
        std::string roomBName;
        if (!TryParseDoorRoomNames(name->name, roomAName, roomBName))
            continue;

        auto findEntityByName = [&](const std::string& targetName) -> Entity
            {
                const std::string target = ToUpper(targetName);
                auto& names = m_registry.GetComponents<NameComponent>();
                for (auto& [candidateEntity, candidateName] : names)
                {
                    if (ToUpper(StripBlenderNumericSuffix(candidateName.name)) == target)
                        return candidateEntity;
                }
                return INVALID_ENTITY;
            };

        auto findDoorLeaf = [&](char side) -> Entity
            {
                Entity leaf = findEntityByName(MakeDoorLeafName(side, roomAName, roomBName));
                if (leaf != INVALID_ENTITY)
                    return leaf;
                return findEntityByName(MakeDoorLeafName(side, roomBName, roomAName));
            };

        Entity leftLeaf = findDoorLeaf('L');
        Entity rightLeaf = findDoorLeaf('R');

        if (leftLeaf != INVALID_ENTITY || rightLeaf != INVALID_ENTITY)
        {
            door.animationMode = DoorAnimationMode::Sliding;
            door.leftLeaf = leftLeaf;
            door.rightLeaf = rightLeaf;
            door.leafPositionsInitialized = false;
            door.positionInitialized = false;
            door.slideAxis = { 1.0f, 0.0f, 0.0f };

            OutputDebugStringA(("[Doors] Linked sliding leaf/leaves for " + name->name + "\n").c_str());
        }

        Entity roomA = FindRoomByName(roomAName);
        Entity roomB = FindRoomByName(roomBName);
        if (roomA == INVALID_ENTITY || roomB == INVALID_ENTITY)
        {
            std::string message = "[Doors] Failed to resolve rooms for " + name->name + ": ";
            if (roomA == INVALID_ENTITY)
                message += roomAName + " missing ";
            if (roomB == INVALID_ENTITY)
                message += roomBName + " missing";
            message += "\n";
            OutputDebugStringA(message.c_str());
            continue;
        }

        door.roomA = roomA;
        door.roomB = roomB;
        ConnectRooms(roomA, roomB);
        OutputDebugStringA(("[Doors] Linked " + name->name + "\n").c_str());
    }
}
Entity SceneManager::SpawnObject(AssimpModelClass* model, float x, float y, float z)
{
    SpawnObjectDesc desc;
    desc.model = model;
    desc.position = { x, y, z };

    return SpawnObject(std::move(desc));
}

Entity SceneManager::SpawnObject(SpawnObjectDesc&& desc)
{
    Entity entity = m_registry.CreateEntity();

    m_registry.AddComponent<TransformComponent>(
        entity,
        TransformComponent{ desc.position, desc.rotation, desc.scale }
    );

    if (desc.addMeshRenderer)
    {
        MeshRendererComponent mesh;
        mesh.model = desc.model;
        mesh.material = desc.material;
        mesh.castsShadow = desc.meshCastsShadow;

        m_registry.AddComponent<MeshRendererComponent>(
            entity,
            std::move(mesh)
        );
    }
    m_registry.AddComponent<NameComponent>(
        entity,
        NameComponent{ desc.name }
    );
    if (desc.active)
        m_registry.AddComponent<ActiveComponent>(entity, {});

    if (desc.addInteraction)
    {
        m_registry.AddComponent<InteractionComponent>(
            entity,
            std::move(desc.interaction)
        );
    }

    if (desc.addRigidBody)
    {
        m_registry.AddComponent<RigidBodyComponent>(
            entity,
            std::move(desc.rigidBody)
        );

        PhysicsSystem::CreateRigidBody(m_registry, entity);
    }

    if (desc.model && desc.model->HasAnimations())
        m_registry.AddComponent<AnimationComponent>(entity, {});

    return entity;
}

Entity SceneManager::CreateWorldArea(WorldAreaType type)
{
    Entity entity = m_registry.CreateEntity();

    WorldAreaComponent area;
    area.type = type;

    m_registry.AddComponent<WorldAreaComponent>(entity, area);
    m_registry.AddComponent<NameComponent>(entity, NameComponent{ "World Area" });
    m_registry.AddComponent<ActiveComponent>(entity, {});

    return entity;
}

// ============================================================================
// RemoveObject вЂ” РґРµР°РєС‚РёРІРёСЂРѕРІР°С‚СЊ Рё РїРѕР»РЅРѕСЃС‚СЊСЋ СѓРґР°Р»РёС‚СЊ СЃСѓС‰РЅРѕСЃС‚СЊ
// РќР•Р”Р•Р›РЇ 4: С‚РµРїРµСЂСЊ РІС‹Р·С‹РІР°РµРј DestroyEntity вЂ” EditorUI С…СЂР°РЅРёС‚ Entity, РЅРµ int index.
// ============================================================================
bool SceneManager::RemoveObject(Entity entity)
{
    const bool isSceneObject =
        m_registry.HasComponent<TransformComponent>(entity) ||
        m_registry.HasComponent<WorldAreaComponent>(entity);

    if (!isSceneObject)
        return false; // СЃСѓС‰РЅРѕСЃС‚СЊ РЅРµ СЃСѓС‰РµСЃС‚РІСѓРµС‚

    
    // РЈР±РёСЂР°РµРј С‚РµРі Р°РєС‚РёРІРЅРѕСЃС‚Рё вЂ” RenderSystem РїСЂРѕРїСѓСЃС‚РёС‚ СЌС‚Сѓ СЃСѓС‰РЅРѕСЃС‚СЊ
    m_registry.RemoveComponent<ActiveComponent>(entity);
    if (auto* mesh = m_registry.GetComponent<MeshRendererComponent>(entity))
    {
        if (mesh->material && mesh->material != m_defaultMaterial) {
            delete mesh->material;
        }
        mesh->material = nullptr;
    }
    if (m_registry.HasComponent<CharacterController>(entity))
    {
        CharacterSpawnSystem::Destroy(m_registry, entity);
    }
    m_registry.DestroyEntity(entity);
   

    return true;
}

// ============================================================================
// Clear вЂ” РѕС‡РёСЃС‚РёС‚СЊ РІСЃСЋ СЃС†РµРЅСѓ
// ============================================================================
void SceneManager::Clear()
{
    // A worker owns its shared import state until it finishes. Dropping the
    // manager reference safely prevents a late CPU result from entering a new scene.
    m_asyncShipImport.reset();

    // NavMesh хранит кэш по Entity комнат. До уничтожения/повторного использования
    // Entity обязательно забываем старые Detour meshes и пути NPC.
    NpcNavMeshSystem::InvalidateAll();

    auto& meshes = m_registry.GetComponents<MeshRendererComponent>();
    for (auto& [entity, mesh] : meshes)
    {
        if (mesh.material && mesh.material != m_defaultMaterial)
            delete mesh.material;

        mesh.material = nullptr;
    }
    // РќРµРґРµР»СЏ 3: РѕС‡РёС‰Р°РµРј ECS СЂРµРµСЃС‚СЂ С‡РµСЂРµР· РїРµСЂРµСЃРѕР·РґР°РЅРёРµ
    // (РїРѕР»РЅС‹Р№ СЃР±СЂРѕСЃ вЂ” РІСЃРµ Entity Рё РІСЃРµ РєРѕРјРїРѕРЅРµРЅС‚С‹)
    m_registry = ECSRegistry{};
    m_importedShipModels.clear();
    for (auto& [name, model] : m_modelCache)
    {
        delete model;  // РР»Рё model->Shutdown() РµСЃР»Рё РµСЃС‚СЊ
    }
    m_modelCache.clear();
    m_importedShipModels.clear();

    // Р’ MaterialClass::~MaterialClass()
  
}

// ============================================================================
// UpdateAll вЂ” РѕР±РЅРѕРІРёС‚СЊ СЃС†РµРЅСѓ Р·Р° РєР°РґСЂ
//
// РќР•Р”Р•Р›РЇ 3: TransformSystem РЈР”РђР›РЃРќ.
// Р”Р°РЅРЅС‹Рµ С‚СЂР°РЅСЃС„РѕСЂРјР°С†РёРё Р¶РёРІСѓС‚ РўРћР›Р¬РљРћ РІ TransformComponent.
// TransformComponent::GetWorldMatrix() РёСЃРїРѕР»СЊР·СѓРµС‚СЃСЏ РЅР°РїСЂСЏРјСѓСЋ РІ RenderSystem.
//
// Р—РґРµСЃСЊ РјРѕР¶РЅРѕ РґРѕР±Р°РІРёС‚СЊ РґСЂСѓРіРёРµ СЃРёСЃС‚РµРјС‹: С„РёР·РёРєСѓ, Р°РЅРёРјР°С†РёРё, СЃРєСЂРёРїС‚С‹.
// ============================================================================
void SceneManager::UpdateAll(float deltaTime, Entity player)
{
    // РЈ РёРјРїРѕСЂС‚РёСЂРѕРІР°РЅРЅРѕРіРѕ РєРѕСЂР°Р±Р»СЏ РґРµС‚Р°Р»Рё РїРѕРєР° С…СЂР°РЅСЏС‚ world transform.
    // РџРѕСЌС‚РѕРјСѓ Р»СЋР±РѕРµ РїРµСЂРµРјРµС‰РµРЅРёРµ ShipRoot РїРµСЂРµРЅРѕСЃРёРј РЅР° РІСЃРµ ShipPart РїРѕ delta.
    auto& shipRoots = m_registry.GetComponents<ShipRootComponent>();
    for (auto& [rootEntity, root] : shipRoots)
    {
        auto* rootTransform = m_registry.GetComponent<TransformComponent>(rootEntity);
        if (!rootTransform)
            continue;

        const XMFLOAT3 delta = {
            rootTransform->position.x - root.lastSyncedPosition.x,
            rootTransform->position.y - root.lastSyncedPosition.y,
            rootTransform->position.z - root.lastSyncedPosition.z
        };

        if (delta.x != 0.0f || delta.y != 0.0f || delta.z != 0.0f)
        {
            auto& shipParts = m_registry.GetComponents<ShipPartComponent>();
            for (auto& [partEntity, part] : shipParts)
            {
                if (part.root != rootEntity)
                    continue;

                if (auto* partTransform = m_registry.GetComponent<TransformComponent>(partEntity))
                {
                    partTransform->position.x += delta.x;
                    partTransform->position.y += delta.y;
                    partTransform->position.z += delta.z;
                }
            }

            root.lastSyncedPosition = rootTransform->position;
        }
    }

    AnimationSystem::Update(m_registry, deltaTime);
    FirstPersonViewSystem::UpdateBoneAttachments(m_registry);

	RoomSystem::Update(
        m_registry,
        deltaTime,
        m_assetLoadSystem ? &m_assetLoadSystem->GetJobSystem() : nullptr);

    // РџРѕРІСЂРµР¶РґС‘РЅРЅС‹Р№ СЂРµР°РєС‚РѕСЂ РЅРµ РґРѕР»Р¶РµРЅ РїРёС‚Р°С‚СЊ РєРѕСЂР°Р±Р»СЊ.
    m_registry.ForEach<GeneratorComponent>(
        [&](Entity entity, GeneratorComponent& generator)
        {
            const auto* damage = m_registry.GetComponent<DamageableComponent>(entity);
            if (damage && damage->isDestroyed)
                generator.isOnline = false;
        });

    GeneratorSystem::Update(m_registry, deltaTime);
    ShipPowerGridSystem::Update(m_registry, deltaTime);

    // Р—Р°РІРёСЃРёРјС‹Рµ РѕС‚ РїРёС‚Р°РЅРёСЏ СЃРІРµС‚РёР»СЊРЅРёРєРё РјРµРЅСЏРµРј РїРѕСЃР»Рµ СЂР°СЃС‡С‘С‚Р° PowerSystem.
    m_registry.ForEach<LightComponent>(
        [&](Entity entity, LightComponent& light)
        {
            auto* consumer = m_registry.GetComponent<PowerConsumerComponent>(entity);
            if (!consumer)
                return;

            const auto* damage = m_registry.GetComponent<DamageableComponent>(entity);
            light.enabled = (!damage || !damage->isDestroyed) && consumer->hasPower;
        });

    m_registry.ForEach<OxygenGeneratorComponent>(
        [&](Entity entity, OxygenGeneratorComponent& generator)
        {
            const auto* consumer = m_registry.GetComponent<PowerConsumerComponent>(entity);
            const auto* damage = m_registry.GetComponent<DamageableComponent>(entity);

            generator.hasPower = !consumer || consumer->hasPower;
            generator.broken = damage && damage->isDestroyed;
        });

    RoomEnvironmentSystem::Update(m_registry, deltaTime);

    OxygenGeneratorSystem::Update(m_registry, deltaTime);

	PlayerVitalsSystem::Update(m_registry, player, deltaTime);
    ShipAlertSystem::Update(m_registry, player);
   
}
// ============================================================================
// RenderAllECS вЂ” СЂРµРЅРґРµСЂ РІСЃРµС… Р°РєС‚РёРІРЅС‹С… РѕР±СЉРµРєС‚РѕРІ С‡РµСЂРµР· RenderSystem
//
// РќР•Р”Р•Р›РЇ 3: Р·Р°РјРµРЅСЏРµС‚ СЃС‚Р°СЂС‹Р№ RenderAll() С‡РµСЂРµР· m_objects.
//
// RenderSystem::Render РёС‚РµСЂРёСЂСѓРµС‚ ECS:
//   1. Р‘РµСЂС‘С‚ РІСЃРµ MeshRendererComponent
//   2. РџСЂРѕРІРµСЂСЏРµС‚ ActiveComponent (С‚РµРі)
//   3. Р‘РµСЂС‘С‚ TransformComponent > GetWorldMatrix()
//   4. Р’С‹Р·С‹РІР°РµС‚ RenderMesh()
//
// РЎРІРµС‚ РєРѕРїРёСЂСѓРµС‚СЃСЏ РёР· m_lights РІ РїРµСЂРµРґР°РЅРЅС‹Рµ РјР°СЃСЃРёРІС‹ Р—Р”Р•РЎР¬ вЂ” РґРѕ РІС‹Р·РѕРІР° RenderSystem.
// ============================================================================
int SceneManager::RenderAllECS(
    ID3D11DeviceContext* context,
    const XMMATRIX& viewMatrix,
    const XMMATRIX& projectionMatrix,
    MaterialClass* defaultMaterial,
    XMFLOAT4 lightColors[],
    XMFLOAT4 lightPositions[],
    const Settings::FogSettings& fog,
    Entity selectedEntity,
    XMFLOAT3 cameraPos,
    float deltaTime)
{
    XMFLOAT4 lightDirectionsAndOuterCos[NUM_LIGHTS] = {};
    XMFLOAT4 lightInnerCosAndFlags[NUM_LIGHTS] = {};
    GatherLightsForShader(lightColors, lightPositions,
        lightDirectionsAndOuterCos, lightInnerCosAndFlags, NUM_LIGHTS);

    bool hasActiveSpotLight = false;
    for (int i = 0; i < NUM_LIGHTS; ++i)
    {
        hasActiveSpotLight |= lightInnerCosAndFlags[i].y > 0.5f;
    }

    auto& meshPool = m_registry.GetComponents<MeshRendererComponent>();
    const FrustumCulling frustum(viewMatrix, projectionMatrix);
    int renderedCount = 0;
    std::vector<Entity> visibleRooms;
    RoomCullingSystem::BuildVisibleRooms(m_registry, cameraPos, visibleRooms);
    std::unordered_map<Entity, bool> roomVisibilityCache;
    auto isVisibleByRoom = [&](Entity entity)
    {
        auto it = roomVisibilityCache.find(entity);
        if (it != roomVisibilityCache.end())
            return it->second;

        const bool visible =
            RoomCullingSystem::IsEntityVisibleFromRooms(m_registry, entity, visibleRooms);
        roomVisibilityCache.emplace(entity, visible);
        return visible;
    };
    auto updatePbrDamageState = [&](MaterialClass* mat, Entity entity, const XMMATRIX& world)
    {
        auto* shader = mat ? mat->GetShader() : nullptr;
        if (!shader || shader->GetType() != ShaderType::PBR)
            return;

        auto* pbrShader = static_cast<PBRShaderClass*>(shader);
        const DamageDecalComponent* damage = m_registry.GetComponent<DamageDecalComponent>(entity);
        DamageDecalComponent batchDamage;

        if (const auto* batch = m_registry.GetComponent<ShipPanelVisualBatchComponent>(entity))
        {
            // The batch mesh is built in ship/world coordinates. Convert every
            // panel-local decal to that space, then draw all of them in one call.
            const XMMATRIX inverseBatchWorld = XMMatrixInverse(nullptr, world);
            const XMMATRIX batchNormalToLocal = XMMatrixTranspose(world);

            m_registry.ForEach<HullPanelComponent>([&](Entity panelEntity, HullPanelComponent& panel)
            {
                if (batchDamage.cracks.size() >= DamageDecalComponent::MAX_CRACKS)
                    return;

                const auto* part = m_registry.GetComponent<ShipPartComponent>(panelEntity);
                const auto* panelTransform = m_registry.GetComponent<TransformComponent>(panelEntity);
                const auto* panelDamage = m_registry.GetComponent<DamageDecalComponent>(panelEntity);
                if (!part || part->root != batch->shipRoot ||
                    (batch->room != INVALID_ENTITY && panel.room != batch->room) ||
                    !panelTransform || !panelDamage)
                    return;

                const XMMATRIX panelWorld = panelTransform->GetWorldMatrix();
                const XMMATRIX panelNormalToWorld = XMMatrixTranspose(XMMatrixInverse(nullptr, panelWorld));
                for (const auto& crack : panelDamage->cracks)
                {
                    if (batchDamage.cracks.size() >= DamageDecalComponent::MAX_CRACKS)
                        break;

                    DamageDecalComponent::CrackEntry batchCrack = crack;
                    const XMVECTOR worldPosition = XMVector3TransformCoord(XMLoadFloat3(&crack.localPosition), panelWorld);
                    XMStoreFloat3(&batchCrack.localPosition, XMVector3TransformCoord(worldPosition, inverseBatchWorld));

                    const XMVECTOR worldNormal = XMVector3Normalize(XMVector3TransformNormal(
                        XMLoadFloat3(&crack.localNormal), panelNormalToWorld));
                    XMStoreFloat3(&batchCrack.localNormal, XMVector3Normalize(
                        XMVector3TransformNormal(worldNormal, batchNormalToLocal)));
                    batchDamage.cracks.push_back(batchCrack);
                }
            });

            damage = &batchDamage;
        }

        pbrShader->SetDamageDecals(context, damage && !damage->cracks.empty() ? damage : nullptr, world);
    };

    // -- РџСЂРѕС…РѕРґ 1: С‚РѕР»СЊРєРѕ РЅРµРїСЂРѕР·СЂР°С‡РЅС‹Рµ РѕР±СЉРµРєС‚С‹ (Р·Р°РїРёСЃСЊ РІ z-buffer) ------------
    for (auto& [entity, mesh] : meshPool)
    {
        if (!m_registry.HasComponent<ActiveComponent>(entity)) continue;
        if (!ShouldRenderInWorldPass(entity)) continue;
        if (!RenderCandidateSystem::IsCandidate(entity, viewMatrix, projectionMatrix)) continue;
        auto* light = m_registry.GetComponent<LightComponent>(entity);
        if (light && light->isEditorGizmo) continue;
        auto* transform = m_registry.GetComponent<TransformComponent>(entity);
        if (!transform || !mesh.model) continue;
        if (!isVisibleByRoom(entity)) continue;
        auto* mat = mesh.material ? mesh.material : defaultMaterial;
        if (!mat) continue;

        // РџСЂРѕРїСѓСЃРєР°РµРј СЃС‚РµРєР»СЏРЅРЅС‹Рµ РѕР±СЉРµРєС‚С‹ РІ РїРµСЂРІРѕРј РїСЂРѕС…РѕРґРµ
        if (mat->IsGlass()) continue;

        // РџСЂРѕРІРµСЂСЏРµРј С‚Р°РєР¶Рµ embedded transparent meshes
        bool hasTransparentParts = false;
        for (const auto& part : mesh.model->GetMeshParts())
            if (part.isTransparent) { hasTransparentParts = true; break; }
        if (hasTransparentParts && mat->GetShader() &&
            mat->GetShader()->GetType() == ShaderType::PBR) continue;

        XMMATRIX world = transform->GetWorldMatrix();

        XMFLOAT3 localMin;
        XMFLOAT3 localMax;
        mesh.model->GetBoundingBox(&localMin, &localMax);
        if (!frustum.IsVisible(world, localMin, localMax)) continue;

        auto* animComp = m_registry.GetComponent<AnimationComponent>(entity);
        updatePbrDamageState(mat, entity, world);

        bool renderedAsync = false;
        // The async asset path currently uploads only point-light data. Keep
        // spot-lit geometry on the full renderer so the cone is not lost.
        if (! mesh.useAsyncMesh && m_assetLoadSystem && !mesh.asyncModelPath.empty())
        {
            renderedAsync = m_assetLoadSystem->RenderMeshAsset(
                context, mesh.asyncModelPath, world, viewMatrix, projectionMatrix,
                mat, lightColors, lightPositions, entity == selectedEntity);
        }

        if (renderedAsync)
            renderedCount++;

        if (!renderedAsync)
        {
            RenderMesh(context, mesh.model, world, viewMatrix, projectionMatrix,
                mat, lightColors, lightPositions, fog,
                entity == selectedEntity, cameraPos, deltaTime, animComp,
                lightDirectionsAndOuterCos, lightInnerCosAndFlags,
                light && !light->enabled ? 0.0f : 1.0f);
            renderedCount++;
        }
    }

    // -- РџСЂРѕС…РѕРґ 2: РїСЂРѕР·СЂР°С‡РЅС‹Рµ РѕР±СЉРµРєС‚С‹ (СЃС‚РµРєР»Рѕ) вЂ” z-buffer РЅР° С‡С‚РµРЅРёРµ ----------
    // РћС‚РєР»СЋС‡Р°РµРј Р·Р°РїРёСЃСЊ РІ z-buffer С‡С‚РѕР±С‹ СЃС‚РµРєР»Рѕ РЅРµ РїРµСЂРµРєСЂС‹РІР°Р»Рѕ СЃР°РјРѕ СЃРµР±СЏ
    // Рё РЅРµ СЃРєСЂС‹РІР°Р»Рѕ РѕР±СЉРµРєС‚С‹ Р·Р° РЅРёРј
    for (auto& [entity, mesh] : meshPool)
    {
        if (!m_registry.HasComponent<ActiveComponent>(entity)) continue;
        if (!ShouldRenderInWorldPass(entity)) continue;
        if (!RenderCandidateSystem::IsCandidate(entity, viewMatrix, projectionMatrix)) continue;
        auto* light = m_registry.GetComponent<LightComponent>(entity);
        if (light && light->isEditorGizmo) continue;
        auto* transform = m_registry.GetComponent<TransformComponent>(entity);
        if (!transform || !mesh.model) continue;
        if (!isVisibleByRoom(entity)) continue;
        auto* mat = mesh.material ? mesh.material : defaultMaterial;
        if (!mat) continue;

        bool isTransparentObj = mat->IsGlass();
        if (!isTransparentObj)
        {
            for (const auto& part : mesh.model->GetMeshParts())
                if (part.isTransparent) {
                    isTransparentObj = true; break;
                }
        }
        if (!isTransparentObj) continue;

        XMMATRIX world = transform->GetWorldMatrix();

        XMFLOAT3 localMin;
        XMFLOAT3 localMax;
        mesh.model->GetBoundingBox(&localMin, &localMax);
        if (!frustum.IsVisible(world, localMin, localMax)) continue;
        updatePbrDamageState(mat, entity, world);

        bool renderedAsync = false;
        if (!hasActiveSpotLight && mesh.useAsyncMesh && m_assetLoadSystem && !mesh.asyncModelPath.empty())
        {
            renderedAsync = m_assetLoadSystem->RenderMeshAsset(
                context, mesh.asyncModelPath, world, viewMatrix, projectionMatrix,
                mat, lightColors, lightPositions, entity == selectedEntity);
        }

        if (renderedAsync)
            renderedCount++;

        if (!renderedAsync)
        {
            RenderMesh(context, mesh.model, world, viewMatrix, projectionMatrix,
                mat, lightColors, lightPositions, fog,
                entity == selectedEntity, cameraPos, deltaTime, nullptr,
                lightDirectionsAndOuterCos, lightInnerCosAndFlags,
                light && !light->enabled ? 0.0f : 1.0f);
            renderedCount++;
        }
    }

    return renderedCount;
}

bool SceneManager::ShouldRenderInWorldPass(Entity entity)
{
    if (m_registry.HasComponent<FirstPersonViewComponent>(entity))
        return false;

    if (m_firstPersonViewActive)
    {
        if (auto* weaponOwner = m_registry.GetComponent<WeaponOwnerComponent>(entity))
        {
            if (weaponOwner->owner == m_firstPersonLocalPlayer)
                return false;
        }
    }

    if (!m_firstPersonViewActive || entity != m_firstPersonLocalPlayer)
        return true;

    auto& firstPersonPool =
        m_registry.GetComponents<FirstPersonViewComponent>();
    for (const auto& [viewEntity, viewModel] : firstPersonPool)
    {
        (void)viewEntity;
        if (viewModel.owner == m_firstPersonLocalPlayer && viewModel.hideOwner)
            return false;
    }

    return true;
}

int SceneManager::RenderFirstPersonECS(
    ID3D11DeviceContext* context,
    const XMMATRIX& viewMatrix,
    const XMMATRIX& projectionMatrix,
    MaterialClass* defaultMaterial,
    const Settings::FogSettings& fog,
    Entity localPlayer,
    XMFLOAT3 cameraPos,
    float deltaTime)
{
    if (localPlayer == INVALID_ENTITY)
        return 0;

    XMFLOAT4 lightColors[NUM_LIGHTS] = {};
    XMFLOAT4 lightPositions[NUM_LIGHTS] = {};
    XMFLOAT4 lightDirectionsAndOuterCos[NUM_LIGHTS] = {};
    XMFLOAT4 lightInnerCosAndFlags[NUM_LIGHTS] = {};
    GatherLightsForShader(
        lightColors, lightPositions,
        lightDirectionsAndOuterCos, lightInnerCosAndFlags,
        NUM_LIGHTS);

    int renderedCount = 0;
    auto& meshPool = m_registry.GetComponents<MeshRendererComponent>();
    for (auto& [entity, mesh] : meshPool)
    {
        if (!m_registry.HasComponent<ActiveComponent>(entity))
            continue;

        auto* viewModel = m_registry.GetComponent<FirstPersonViewComponent>(entity);
        if (!viewModel || viewModel->owner != localPlayer)
            continue;

        auto* transform = m_registry.GetComponent<TransformComponent>(entity);
        if (!transform || !mesh.model)
            continue;

        MaterialClass* material = mesh.material ? mesh.material : defaultMaterial;
        if (!material)
            continue;

        auto* animation = m_registry.GetComponent<AnimationComponent>(entity);
        RenderMesh(
            context,
            mesh.model,
            transform->GetWorldMatrix(),
            viewMatrix,
            projectionMatrix,
            material,
            lightColors,
            lightPositions,
            fog,
            false,
            cameraPos,
            deltaTime,
            animation,
            lightDirectionsAndOuterCos,
            lightInnerCosAndFlags);
        ++renderedCount;
    }

    return renderedCount;
}

// ============================================================================
// RenderAllECSExcluding вЂ” СЂРµРЅРґРµСЂ РІСЃРµС… РѕР±СЉРµРєС‚РѕРІ РєСЂРѕРјРµ РѕРґРЅРѕРіРѕ
//
// РќР•Р”Р•Р›РЇ 3: Р·Р°РјРµРЅСЏРµС‚ Render3DObjectsExcluding() РІ ApplicationClass.
// РСЃРїРѕР»СЊР·СѓРµС‚СЃСЏ РґР»СЏ reflection pass: РїР»Р°СЃС‚РёРЅР°-Р·РµСЂРєР°Р»Рѕ РёСЃРєР»СЋС‡Р°РµС‚СЃСЏ
// С‡С‚РѕР±С‹ РѕРЅР° РЅРµ РѕС‚СЂР°Р¶Р°Р»Р° СЃР°РјСѓ СЃРµР±СЏ (СЂРµРєСѓСЂСЃРёСЏ).
/// TODO: РїСЂРѕРІРµСЂРёС‚СЊ СЂР°Р±РѕС‚РѕСЃРїРѕСЃРѕР±РЅРѕСЃС‚СЊ СЃ РЅРѕРІРѕР№ Р°СЂС…РёС‚РµРєС‚СѓСЂРѕР№
// ============================================================================
void SceneManager::RenderAllECSExcluding(
    ID3D11DeviceContext* context,
    const XMMATRIX& viewMatrix,
    const XMMATRIX& projectionMatrix,
    MaterialClass* defaultMaterial,
    XMFLOAT4             lightColors[],
    XMFLOAT4             lightPositions[],
    const Settings::FogSettings& fog,
    Entity               excludeEntity,
    float                deltaTime,
    XMFLOAT3             cameraPos)
{

    // ?? РќРћР’РћР•: РЎРѕР±РёСЂР°РµРј СЃРІРµС‚Р° РёР· ECS (РєР°Рє РІ RenderAllECS)
    XMFLOAT4 lightDirectionsAndOuterCos[NUM_LIGHTS] = {};
    XMFLOAT4 lightInnerCosAndFlags[NUM_LIGHTS] = {};
    GatherLightsForShader(lightColors, lightPositions,
        lightDirectionsAndOuterCos, lightInnerCosAndFlags, NUM_LIGHTS);

    bool hasActiveSpotLight = false;
    for (int i = 0; i < NUM_LIGHTS; ++i)
    {
        hasActiveSpotLight |= lightInnerCosAndFlags[i].y > 0.5f;
    }

    // РС‚РµСЂРёСЂСѓРµРј РІСЂСѓС‡РЅСѓСЋ С‡С‚РѕР±С‹ РїСЂРѕРїСѓСЃС‚РёС‚СЊ excludeEntity
    auto& meshPool = m_registry.GetComponents<MeshRendererComponent>();
    const FrustumCulling frustum(viewMatrix, projectionMatrix);
    std::vector<Entity> visibleRooms;
    RoomCullingSystem::BuildVisibleRooms(m_registry, cameraPos, visibleRooms);
    std::unordered_map<Entity, bool> roomVisibilityCache;
    auto isVisibleByRoom = [&](Entity entity)
    {
        auto it = roomVisibilityCache.find(entity);
        if (it != roomVisibilityCache.end())
            return it->second;

        const bool visible =
            RoomCullingSystem::IsEntityVisibleFromRooms(m_registry, entity, visibleRooms);
        roomVisibilityCache.emplace(entity, visible);
        return visible;
    };

    for (auto& [entity, mesh] : meshPool)
    {
        // РџСЂРѕРїСѓСЃРєР°РµРј РёСЃРєР»СЋС‡С‘РЅРЅСѓСЋ СЃСѓС‰РЅРѕСЃС‚СЊ (reflection РѕР±СЉРµРєС‚)
        if (entity == excludeEntity) continue;
        if (m_registry.HasComponent<FirstPersonViewComponent>(entity)) continue;

        // РџСЂРѕРїСѓСЃРєР°РµРј РЅРµР°РєС‚РёРІРЅС‹Рµ
        if (!m_registry.HasComponent<ActiveComponent>(entity)) continue;
        if (!RenderCandidateSystem::IsCandidate(entity, viewMatrix, projectionMatrix)) continue;

        auto* transform = m_registry.GetComponent<TransformComponent>(entity);
        if (!transform || !mesh.model) continue;
        if (!isVisibleByRoom(entity)) continue;

        auto* mat = mesh.material ? mesh.material : defaultMaterial;
        if (!mat) continue;

        XMMATRIX world = transform->GetWorldMatrix();

        XMFLOAT3 localMin;
        XMFLOAT3 localMax;
        mesh.model->GetBoundingBox(&localMin, &localMax);
        if (!frustum.IsVisible(world, localMin, localMax)) continue;
        auto* animComp = m_registry.GetComponent<AnimationComponent>(entity);

        bool renderedAsync = false;
        if (!hasActiveSpotLight && mesh.useAsyncMesh && m_assetLoadSystem && !mesh.asyncModelPath.empty())
        {
            renderedAsync = m_assetLoadSystem->RenderMeshAsset(
                context, mesh.asyncModelPath, world, viewMatrix, projectionMatrix,
                mat, lightColors, lightPositions, false);
        }

        if (!renderedAsync)
        {
            RenderMesh(context, mesh.model, world, viewMatrix, projectionMatrix,
                mat, lightColors, lightPositions, fog, false, XMFLOAT3(), deltaTime, animComp,
                lightDirectionsAndOuterCos, lightInnerCosAndFlags,
                [&]()
                {
                    auto* light = m_registry.GetComponent<LightComponent>(entity);
                    return light && !light->enabled ? 0.0f : 1.0f;
                }());
        }
    }
}

// ============================================================================
// SpawnSprite
// ============================================================================
int SceneManager::SpawnSprite(
    ID3D11Device* device, ID3D11DeviceContext* deviceContext,
    const char* textureName,
    float x, float y, float z, float width, float height)
{
    auto sprite = std::make_unique<WorldSpriteClass>();
    if (!sprite->Initialize(device, deviceContext, textureName, width, height))
    {
        OutputDebugStringA("SpawnSprite: FAILED TO INITIALIZE WORLDSPRITE\n");
        return -1;
    }
    sprite->SetPosition(x, y, z);
    m_sprites.push_back(std::move(sprite));
    return static_cast<int>(m_sprites.size() - 1);
}

bool SceneManager::RemoveSprite(int index)
{
    if (index < 0 || index >= static_cast<int>(m_sprites.size())) return false;
    m_sprites.erase(m_sprites.begin() + index);
    return true;
}

WorldSpriteClass* SceneManager::GetSprite(int index)
{
    if (index < 0 || index >= static_cast<int>(m_sprites.size())) return nullptr;
    return m_sprites[index].get();
}

void SceneManager::RenderSprites(
    ID3D11DeviceContext* context,
    XMMATRIX view, XMMATRIX projection, TextureShaderClass* shader)
{
    for (auto& sprite : m_sprites)
        sprite->Render(context, view, projection, shader);
}

// ============================================================================
// SetupLightSpheres / RenderLightSpheres
// Light-СЃС„РµСЂС‹ вЂ” С‚РѕР»СЊРєРѕ РІРёР·СѓР°Р»СЊРЅС‹Рµ РёРЅРґРёРєР°С‚РѕСЂС‹ СЂРµРґР°РєС‚РѕСЂР°, РЅРµ С‡Р°СЃС‚СЊ ECS.
// ============================================================================
void SceneManager::SetupLightSpheres(std::vector<GameObject>& spheres, AssimpModelClass* model)
{
    spheres.clear();
    spheres.reserve(NUM_LIGHTS);
    for (int i = 0; i < NUM_LIGHTS; ++i)
    {
        GameObject sphere;
        sphere.Initialize(model, 0, 0, 0);
        sphere.SetScale(0.2f, 0.2f, 0.2f);
        spheres.push_back(sphere);
    }
}



WorldSettings& SceneManager::GetWorldSettings()
{
    return m_worldSettings;
}

const WorldSettings& SceneManager::GetWorldSettings() const
{
    return m_worldSettings;
}

void SceneManager::SetWorldSettings(const WorldSettings& settings)
{

	m_worldSettings = settings;
}

bool SceneManager::SaveWeaponProfile(
    const WeaponComponent& weapon,
    const std::string& filename) const
{
    const auto vec3 = [](const DirectX::XMFLOAT3& value)
    {
        return json::array({ value.x, value.y, value.z });
    };

    json profile;
    profile["version"] = 1;
    profile["visual_model"] = weapon.visualModelPath;
    profile["right_hand_bone"] = weapon.rightHandBone;
    profile["rifle_idle_animation"] = weapon.rifleIdleAnimation;
    profile["rifle_gun_animation"] = weapon.rifleGunAnimation;
    profile["world_position"] = vec3(weapon.worldGrip.position);
    profile["world_rotation"] = vec3(weapon.worldGrip.rotation);
    profile["world_scale"] = vec3(weapon.worldGrip.scale);
    profile["first_person_position"] = vec3(weapon.firstPersonGrip.position);
    profile["first_person_rotation"] = vec3(weapon.firstPersonGrip.rotation);
    profile["first_person_scale"] = vec3(weapon.firstPersonGrip.scale);
    profile["first_person_pitch_scale"] = weapon.firstPersonPitchScale;
    profile["first_person_pitch_offset"] = weapon.firstPersonPitchOffset;
    profile["left_hand_grip"] = vec3(weapon.leftHandGrip);
    profile["muzzle_position"] = vec3(weapon.muzzlePosition);
    profile["show_world_visual"] = weapon.showWorldVisual;
    profile["show_first_person_visual"] = weapon.showFirstPersonVisual;
    profile["enable_left_hand_ik"] = weapon.enableLeftHandIK;

    const fs::path path(filename);
    if (path.has_parent_path())
    {
        std::error_code error;
        fs::create_directories(path.parent_path(), error);
        if (error)
            return false;
    }

    std::ofstream file(filename);
    if (!file.is_open())
        return false;
    file << profile.dump(4);
    return file.good();
}

bool SceneManager::LoadWeaponProfile(
    WeaponComponent& weapon,
    const std::string& filename) const
{
    std::ifstream file(filename);
    if (!file.is_open())
        return false;

    json profile;
    try
    {
        file >> profile;
    }
    catch (const json::exception&)
    {
        return false;
    }

    const auto readVec3 = [&](const char* key, DirectX::XMFLOAT3& value)
    {
        if (!profile.contains(key) || !profile[key].is_array() ||
            profile[key].size() < 3)
        {
            return;
        }
        value = {
            profile[key][0].get<float>(),
            profile[key][1].get<float>(),
            profile[key][2].get<float>() };
    };

    weapon.visualModelPath = profile.value("visual_model", weapon.visualModelPath);
    weapon.rightHandBone = profile.value("right_hand_bone", weapon.rightHandBone);
    weapon.rifleIdleAnimation = profile.value(
        "rifle_idle_animation", weapon.rifleIdleAnimation);
    weapon.rifleGunAnimation = profile.value(
        "rifle_gun_animation", weapon.rifleGunAnimation);
    readVec3("world_position", weapon.worldGrip.position);
    readVec3("world_rotation", weapon.worldGrip.rotation);
    readVec3("world_scale", weapon.worldGrip.scale);
    readVec3("first_person_position", weapon.firstPersonGrip.position);
    readVec3("first_person_rotation", weapon.firstPersonGrip.rotation);
    readVec3("first_person_scale", weapon.firstPersonGrip.scale);
    weapon.firstPersonPitchScale = profile.value(
        "first_person_pitch_scale", weapon.firstPersonPitchScale);
    weapon.firstPersonPitchOffset = profile.value(
        "first_person_pitch_offset", weapon.firstPersonPitchOffset);
    readVec3("left_hand_grip", weapon.leftHandGrip);
    readVec3("muzzle_position", weapon.muzzlePosition);
    weapon.showWorldVisual = profile.value(
        "show_world_visual", weapon.showWorldVisual);
    weapon.showFirstPersonVisual = profile.value(
        "show_first_person_visual", weapon.showFirstPersonVisual);
    weapon.enableLeftHandIK = profile.value(
        "enable_left_hand_ik", weapon.enableLeftHandIK);
    return true;
}

// ============================================================================
// SaveScene вЂ” СЃРµСЂРёР°Р»РёР·Р°С†РёСЏ
//
// РќР•Р”Р•Р›РЇ 3: С‡РёС‚Р°РµРј РґР°РЅРЅС‹Рµ РёР· ECS (TransformComponent, MeshRendererComponent)
// ============================================================================
bool SceneManager::SaveScene(const std::string& filename)
{
    json scene;
    scene["scene_name"] = "MyLevel";
    scene["version"] = 1;
    scene["objects"] = json::array();

    int id = 0;

    // Imported ship parts own temporary submeshes, so save the GLB root only.
    auto& shipRootPool = m_registry.GetComponents<ShipRootComponent>();
    for (auto& [entity, shipRoot] : shipRootPool)
    {
        if (!m_registry.HasComponent<ActiveComponent>(entity)) continue;
        if (m_registry.HasComponent<ShipPartComponent>(entity)) continue;
        scene["objects"].push_back(SerializeObject(entity, id++));
    }
    // -- РЎРѕС…СЂР°РЅСЏРµРј РѕР±СЉРµРєС‚С‹ СЃ РјРµС€РµРј (РѕР±С‹С‡РЅС‹Рµ РѕР±СЉРµРєС‚С‹, РєРѕСЂРЅРё РїРѕРґР»РѕРґРѕРє Рё С‚.Рґ.) --
    auto& meshPool = m_registry.GetComponents<MeshRendererComponent>();
    for (auto& [entity, mesh] : meshPool)
    {
        if (!m_registry.HasComponent<ActiveComponent>(entity)) continue;
        if (m_registry.HasComponent<ShipPartComponent>(entity)) continue;
        scene["objects"].push_back(SerializeObject(entity, id++));
    }

    
    auto& compPool = m_registry.GetComponents<CompartmentComponent>();
    for (auto& [entity, comp] : compPool)
    {
      
        if (m_registry.HasComponent<MeshRendererComponent>(entity)) continue;
        if (!m_registry.HasComponent<ActiveComponent>(entity)) continue;
        if (m_registry.HasComponent<ShipPartComponent>(entity)) continue;

        scene["objects"].push_back(SerializeObject(entity, id++));
    }

    // -- РЎРѕС…СЂР°РЅСЏРµРј РєРѕРјРЅР°С‚С‹ Р‘Р•Р— РјРµС€Р° (Р»РѕРіРёС‡РµСЃРєРёРµ RoomComponent) ----------------
    auto& roomPool = m_registry.GetComponents<RoomComponent>();
    for (auto& [entity, room] : roomPool)
    {
        if (m_registry.HasComponent<MeshRendererComponent>(entity)) continue;
        if (m_registry.HasComponent<CompartmentComponent>(entity)) continue;
        if (!m_registry.HasComponent<ActiveComponent>(entity)) continue;
        if (m_registry.HasComponent<ShipPartComponent>(entity)) continue;

        scene["objects"].push_back(SerializeObject(entity, id++));
    }
    // -- РЎРѕС…СЂР°РЅСЏРµРј standalone-РєРѕР»Р»РёР·РёРё Р±РµР· РјРµС€Р° -------------------------------
    auto& rbPool = m_registry.GetComponents<RigidBodyComponent>();
    for (auto& [entity, rb] : rbPool)
    {
        if (m_registry.HasComponent<MeshRendererComponent>(entity)) continue;
        if (m_registry.HasComponent<CompartmentComponent>(entity)) continue;
        if (!m_registry.HasComponent<ActiveComponent>(entity)) continue;
        if (m_registry.HasComponent<ShipPartComponent>(entity)) continue;

        scene["objects"].push_back(SerializeObject(entity, id++));
    }

    // -- РЎРѕС…СЂР°РЅСЏРµРј Р»РѕРіРёС‡РµСЃРєРёРµ РїСЂРѕР±РѕРёРЅС‹ Р±РµР· РјРµС€Р° -------------------------------
    auto& breachPool = m_registry.GetComponents<HullBreachComponent>();
    for (auto& [entity, breach] : breachPool)
    {
        if (m_registry.HasComponent<MeshRendererComponent>(entity)) continue;
        if (m_registry.HasComponent<RigidBodyComponent>(entity)) continue;
        if (m_registry.HasComponent<CompartmentComponent>(entity)) continue;
        if (m_registry.HasComponent<RoomComponent>(entity)) continue;
        if (!m_registry.HasComponent<ActiveComponent>(entity)) continue;
        if (m_registry.HasComponent<ShipPartComponent>(entity)) continue;

        scene["objects"].push_back(SerializeObject(entity, id++));
    }

    // NPCs are normally rendered objects and were already saved through meshPool.
    // Keep headless/debug NPC entities persistent as well.
    auto& npcPool = m_registry.GetComponents<NPCComponent>();
    for (auto& [entity, npc] : npcPool)
    {
        (void)npc;
        if (m_registry.HasComponent<MeshRendererComponent>(entity)) continue;
        if (m_registry.HasComponent<RigidBodyComponent>(entity)) continue;
        if (m_registry.HasComponent<CompartmentComponent>(entity)) continue;
        if (m_registry.HasComponent<RoomComponent>(entity)) continue;
        if (m_registry.HasComponent<HullBreachComponent>(entity)) continue;
        if (!m_registry.HasComponent<ActiveComponent>(entity)) continue;
        if (m_registry.HasComponent<ShipPartComponent>(entity)) continue;

        scene["objects"].push_back(SerializeObject(entity, id++));
    }

    auto& weaponPool = m_registry.GetComponents<WeaponComponent>();
    for (auto& [entity, weapon] : weaponPool)
    {
        (void)weapon;
        if (m_registry.HasComponent<MeshRendererComponent>(entity)) continue;
        if (m_registry.HasComponent<RigidBodyComponent>(entity)) continue;
        if (m_registry.HasComponent<CompartmentComponent>(entity)) continue;
        if (m_registry.HasComponent<RoomComponent>(entity)) continue;
        if (m_registry.HasComponent<HullBreachComponent>(entity)) continue;
        if (m_registry.HasComponent<NPCComponent>(entity)) continue;
        if (!m_registry.HasComponent<ActiveComponent>(entity)) continue;
        if (m_registry.HasComponent<ShipPartComponent>(entity)) continue;

        scene["objects"].push_back(SerializeObject(entity, id++));
    }
    std::ofstream file(filename);
    if (!file.is_open()) return false;
    file << scene.dump(4);
    return true;
}

// ============================================================================
// SerializeObject вЂ” СѓРїР°РєРѕРІР°С‚СЊ ECS-СЃСѓС‰РЅРѕСЃС‚СЊ РІ JSON
//
// РќР•Р”Р•Р›РЇ 3: С‡РёС‚Р°РµС‚ РёР· TransformComponent, РЅРµ РёР· GameObject
// ============================================================================
json SceneManager::SerializeObject(Entity entity, int id)
{
    json objJson;
    objJson["id"] = id;
    objJson["entity_id"] = static_cast<uint32_t>(entity); // < СЃРѕС…СЂР°РЅСЏРµРј РѕСЂРёРіРёРЅР°Р»СЊРЅС‹Р№ Entity ID

    // -- Model / Texture paths -------------------------------------------------
    const bool hasMeshRenderer = m_registry.HasComponent<MeshRendererComponent>(entity);
    std::string modelPath = hasMeshRenderer ? "assets/model/sphere.obj" : "";
    std::string texturePath;

    if (auto* mesh = m_registry.GetComponent<MeshRendererComponent>(entity))
    {
        if (mesh->useAsyncMesh && !mesh->asyncModelPath.empty())
        {
            modelPath = mesh->asyncModelPath;
        }
        else
        {
            for (auto& [path, cachedModel] : m_modelCache)
            {
                if (cachedModel == mesh->model) { modelPath = path; break; }
            }
        }

        if (mesh->material)
            texturePath = mesh->material->GetTexturePath(0);
    }
    objJson["model_path"] = modelPath;
    objJson["texture_path"] = texturePath;

    if (auto* name = m_registry.GetComponent<NameComponent>(entity))
        objJson["object_name"] = name->name;

    if (auto* shipRoot = m_registry.GetComponent<ShipRootComponent>(entity))
    {
        objJson["has_ship_root"] = true;
        objJson["ship_source_path"] = shipRoot->sourcePath;

        if (auto* grid = m_registry.GetComponent<PowerGridComponent>(entity))
        {
            objJson["power_grid_battery_capacity_mwh"] = grid->batteryCapacityMwh;
            objJson["power_grid_battery_charge_mwh"] = grid->batteryChargeMwh;
            objJson["power_grid_max_charge_mw"] = grid->maxBatteryChargeMw;
            objJson["power_grid_max_discharge_mw"] = grid->maxBatteryDischargeMw;
        }

        objJson["ship_generators"] = json::array();
        for (auto& [generatorEntity, generator] : m_registry.GetComponents<GeneratorComponent>())
        {
            const auto* part = m_registry.GetComponent<ShipPartComponent>(generatorEntity);
            if (!part || part->root != entity)
                continue;

            const auto* name = m_registry.GetComponent<NameComponent>(generatorEntity);
            objJson["ship_generators"].push_back({
                { "name", name ? name->name : std::string() },
                { "deuterium_kg", generator.deuteriumKg },
                { "tritium_kg", generator.tritiumKg },
                { "current_output_mw", generator.currentOutputMw },
                { "target_output_mw", generator.targetOutputMw },
                { "temperature", generator.heat },
                { "is_online", generator.isOnline },
                { "is_scrammed", generator.isScrammed }
            });
        }
    }

    // -- TransformComponent ----------------------------------------------------
    if (auto* t = m_registry.GetComponent<TransformComponent>(entity))
    {
        objJson["position"] = { {"x", t->position.x}, {"y", t->position.y}, {"z", t->position.z} };
        objJson["rotation"] = { {"x", t->rotation.x}, {"y", t->rotation.y}, {"z", t->rotation.z} };
        objJson["scale"] = { {"x", t->scale.x},    {"y", t->scale.y},    {"z", t->scale.z} };
    }

    //Components Character

   



    // -- Shader ----------------------------------------------------------------
    if (auto* mesh = m_registry.GetComponent<MeshRendererComponent>(entity))
    {
        if (mesh->material && mesh->material->GetShader())
            objJson["shader_type"] = static_cast<int>(mesh->material->GetShader()->GetType());
    }

    // -- TerrainBlend material ------------------------------------------------
    if (auto* mesh = m_registry.GetComponent<MeshRendererComponent>(entity))
    {
        if (mesh->material && mesh->material->GetShader() &&
            mesh->material->GetShader()->GetType() == ShaderType::TerrainBlend)
        {
            objJson["terrain_grass_texture"] = mesh->material->GetTexturePath(0);
            objJson["terrain_dirt_texture"] = mesh->material->GetTexturePath(1);
            objJson["terrain_mask_texture"] = mesh->material->GetTexturePath(2);
            objJson["terrain_grass_normal"] = mesh->material->GetTexturePath(3);
            objJson["terrain_dirt_normal"] = mesh->material->GetTexturePath(4);
            objJson["terrain_grass_variation_texture"] = mesh->material->GetTexturePath(5);
            objJson["terrain_grass_variation_normal"] = mesh->material->GetTexturePath(6);
            objJson["terrain_tile_scale"] = mesh->material->GetTerrainTileScale();
            objJson["terrain_normal_strength"] = mesh->material->GetTerrainNormalStrength();
            objJson["terrain_variation_scale"] = mesh->material->GetTerrainVariationScale();
            objJson["terrain_variation_strength"] = mesh->material->GetTerrainVariationStrength();
            objJson["terrain_brightness"] = mesh->material->GetTerrainBrightness();
            objJson["terrain_ambient_boost"] = mesh->material->GetTerrainAmbientBoost();
            auto minXZ = mesh->material->GetTerrainMinXZ();
            auto sizeXZ = mesh->material->GetTerrainSizeXZ();
            objJson["terrain_min_xz"] = { minXZ.x, minXZ.y };
            objJson["terrain_size_xz"] = { sizeXZ.x, sizeXZ.y };
        }
    }
    // -- LightComponent --------------------------------------------------------
    if (auto* light = m_registry.GetComponent<LightComponent>(entity))
    {
        objJson["is_light"] = true;
        objJson["light_color"] = { light->color.x, light->color.y, light->color.z, light->color.w };
        objJson["light_enabled"] = light->enabled;
        objJson["light_radius"] = light->radius;
        objJson["light_offset"] = { light->offset.x, light->offset.y, light->offset.z };
        objJson["light_is_gizmo"] = light->isEditorGizmo;
        objJson["light_attached_to"] = static_cast<uint32_t>(light->attachedTo);

        if (const auto* spot = m_registry.GetComponent<SpotLightComponent>(entity))
        {
            objJson["has_spotlight"] = true;
            objJson["spotlight_direction"] = {
                spot->localDirection.x, spot->localDirection.y, spot->localDirection.z };
            objJson["spotlight_inner_angle"] = spot->innerAngleDegrees;
            objJson["spotlight_outer_angle"] = spot->outerAngleDegrees;
        }
    }

    // -- RigidBodyComponent ----------------------------------------------------
    if (auto* rig = m_registry.GetComponent<RigidBodyComponent>(entity))
    {
        objJson["has_rigidbody"] = true;
        objJson["rb_mass"] = rig->mass;
        objJson["rb_is_dynamic"] = rig->isDynamic;
        objJson["rb_is_kinematic"] = rig->isKinematic;
        objJson["rb_enable_ccd"] = rig->enableCCD;
        objJson["rb_linear_damping"] = rig->linearDamping;
        objJson["rb_angular_damping"] = rig->angularDamping;
        objJson["rb_friction"] = rig->friction;
        objJson["rb_shape_type"] = static_cast<int>(rig->shapeType);
        objJson["rb_shape_radius"] = rig->shapeRadius;
        objJson["rb_use_separate_collision_mesh"] = rig->useSeparateCollisionMesh;
        objJson["rb_collision_model_path"] = rig->collisionModelPath;
        objJson["rb_use_manual_size"] = rig->useManualSize;
        objJson["rb_shape_size"] = { rig->shapeSize.x(), rig->shapeSize.y(), rig->shapeSize.z() };
        objJson["rb_is_compound"] = rig->isCompound;

        // CompoundElements
        json elemsJson = json::array();
        for (const auto& elem : rig->compoundElements)
        {
            json e;
            e["type"] = static_cast<int>(elem.type);
            e["size"] = elem.size;
            e["radius"] = elem.radius;
            e["height"] = elem.height;
            e["ramp_size"] = { elem.rampSize.x, elem.rampSize.y, elem.rampSize.z };
            e["box_offset_min"] = { elem.boxOffsetMin.x, elem.boxOffsetMin.y, elem.boxOffsetMin.z };
            e["box_offset_max"] = { elem.boxOffsetMax.x, elem.boxOffsetMax.y, elem.boxOffsetMax.z };
            e["local_position"] = { elem.localPosition.x, elem.localPosition.y, elem.localPosition.z };
            e["local_rotation"] = { elem.localRotation.x, elem.localRotation.y,
                                       elem.localRotation.z, elem.localRotation.w };
            e["radius_offset"] = elem.radiusOffset;
            e["height_offset_top"] = elem.heightOffsetTop;
            e["height_offset_bot"] = elem.heightOffsetBottom;
            e["debug_weight"] = elem.debugWeight;
            // editorEulerDegrees вЂ” С‚РѕР»СЊРєРѕ РґР»СЏ UI, РЅРµ СЃРѕС…СЂР°РЅСЏРµРј
            elemsJson.push_back(e);
        }
        objJson["rb_compound_elements"] = elemsJson;
    }

    // -- SubmarineHullConfig ---------------------------------------------------
    if (auto* hull = m_registry.GetComponent<SubmarineHullConfig>(entity))
    {
        objJson["has_hull_config"] = true;
        objJson["hull_extents"] = { hull->hullExtents.x, hull->hullExtents.y, hull->hullExtents.z };
        objJson["hull_wall_thickness"] = hull->wallThickness;
        objJson["hull_is_kinematic"] = hull->isKinematic;
        objJson["hull_show_debug"] = hull->showDebugHull;
    }

    // -- StaticColliderComponent -----------------------------------------------
    if (auto* collider = m_registry.GetComponent<StaticColliderComponent>(entity))
    {
        objJson["has_static_collider"] = true;
    }

    // -- BuoyancyComponent -----------------------------------------------------
    if (auto* buoyancy = m_registry.GetComponent<BuoyancyComponent>(entity))
    {
        objJson["has_buoyancy"] = true;
        objJson["buoyancy_fluid_density"] = buoyancy->fluidDensity;
        objJson["buoyancy_volume"] = buoyancy->volume;
        objJson["buoyancy_height"] = buoyancy->height;
        objJson["buoyancy_multiplier"] = buoyancy->buoyancyMultiplier;
    }

    if (m_registry.HasComponent<PlayerGameTag>(entity))
        objJson["has_player_tag"] = true;

    if (auto* firstPerson = m_registry.GetComponent<FirstPersonViewComponent>(entity))
    {
        objJson["has_first_person_view"] = true;
        objJson["first_person_owner"] = static_cast<uint32_t>(firstPerson->owner);
        objJson["first_person_camera_offset"] = {
            firstPerson->cameraOffset.x, firstPerson->cameraOffset.y, firstPerson->cameraOffset.z };
        objJson["first_person_rotation_offset"] = {
            firstPerson->rotationOffset.x, firstPerson->rotationOffset.y, firstPerson->rotationOffset.z };
        objJson["first_person_scale"] = {
            firstPerson->scale.x, firstPerson->scale.y, firstPerson->scale.z };
        objJson["first_person_pitch_scale"] = firstPerson->pitchScale;
        objJson["first_person_pitch_offset"] = firstPerson->pitchOffset;
        objJson["first_person_follow_camera"] = firstPerson->followCamera;
        objJson["first_person_sync_animation"] = firstPerson->syncAnimation;
        objJson["first_person_hide_owner"] = firstPerson->hideOwner;
    }

    if (auto* attachment = m_registry.GetComponent<BoneAttachmentComponent>(entity))
    {
        objJson["has_bone_attachment"] = true;
        objJson["bone_attachment_parent"] = static_cast<uint32_t>(attachment->parent);
        objJson["bone_attachment_name"] = attachment->boneName;
        objJson["bone_attachment_position"] = {
            attachment->localPosition.x, attachment->localPosition.y, attachment->localPosition.z };
        objJson["bone_attachment_rotation"] = {
            attachment->localRotation.x, attachment->localRotation.y, attachment->localRotation.z };
        objJson["bone_attachment_scale"] = {
            attachment->localScale.x, attachment->localScale.y, attachment->localScale.z };
        objJson["bone_attachment_enabled"] = attachment->enabled;
    }

    if (auto* equipped = m_registry.GetComponent<EquippedWeaponComponent>(entity))
    {
        objJson["has_equipped_weapon"] = true;
        objJson["equipped_weapon"] = static_cast<uint32_t>(equipped->weapon);
    }

    if (auto* player = m_registry.GetComponent<PlayerComponent>(entity))
    {
        objJson["has_player_component"] = true;
        objJson["player_health"] = player->health;
        objJson["player_stamina"] = player->stamina;
        objJson["player_oxygen"] = player->oxygen;

        objJson["player_max_health"] = player->maxHealth;
        objJson["player_max_stamina"] = player->maxStamina;
        objJson["player_max_oxygen"] = player->maxOxygen;

        
    }

    if (auto* weapon = m_registry.GetComponent<WeaponComponent>(entity))
    {
        objJson["has_weapon"] = true;
        objJson["weapon_type"] = static_cast<int>(weapon->type);
        objJson["weapon_damage"] = weapon->damage;
        objJson["weapon_range"] = weapon->range;
        objJson["weapon_fire_rate"] = weapon->fireRate;
        objJson["weapon_breach_radius"] = weapon->breachRadius;
        objJson["weapon_visual_model"] = weapon->visualModelPath;
        objJson["weapon_right_hand_bone"] = weapon->rightHandBone;
        objJson["weapon_rifle_idle_animation"] = weapon->rifleIdleAnimation;
        objJson["weapon_rifle_gun_animation"] = weapon->rifleGunAnimation;
        objJson["weapon_world_grip_position"] = {
            weapon->worldGrip.position.x, weapon->worldGrip.position.y, weapon->worldGrip.position.z };
        objJson["weapon_world_grip_rotation"] = {
            weapon->worldGrip.rotation.x, weapon->worldGrip.rotation.y, weapon->worldGrip.rotation.z };
        objJson["weapon_world_grip_scale"] = {
            weapon->worldGrip.scale.x, weapon->worldGrip.scale.y, weapon->worldGrip.scale.z };
        objJson["weapon_fp_grip_position"] = {
            weapon->firstPersonGrip.position.x, weapon->firstPersonGrip.position.y, weapon->firstPersonGrip.position.z };
        objJson["weapon_fp_grip_rotation"] = {
            weapon->firstPersonGrip.rotation.x, weapon->firstPersonGrip.rotation.y, weapon->firstPersonGrip.rotation.z };
        objJson["weapon_fp_grip_scale"] = {
            weapon->firstPersonGrip.scale.x, weapon->firstPersonGrip.scale.y, weapon->firstPersonGrip.scale.z };
        objJson["weapon_fp_pitch_scale"] = weapon->firstPersonPitchScale;
        objJson["weapon_fp_pitch_offset"] = weapon->firstPersonPitchOffset;
        objJson["weapon_left_hand_grip"] = {
            weapon->leftHandGrip.x, weapon->leftHandGrip.y, weapon->leftHandGrip.z };
        objJson["weapon_muzzle_position"] = {
            weapon->muzzlePosition.x, weapon->muzzlePosition.y, weapon->muzzlePosition.z };
        objJson["weapon_show_world_visual"] = weapon->showWorldVisual;
        objJson["weapon_show_fp_visual"] = weapon->showFirstPersonVisual;
        objJson["weapon_enable_left_hand_ik"] = weapon->enableLeftHandIK;
    }

    if (auto* owner = m_registry.GetComponent<WeaponOwnerComponent>(entity))
    {
        objJson["has_weapon_owner"] = true;
        objJson["weapon_owner"] = static_cast<uint32_t>(owner->owner);
    }

    if (auto* movement = m_registry.GetComponent<CharacterMovement>(entity))
    {
        objJson["has_character_movement"] = true;
        objJson["movement_walk_direction"] = { movement->walkDirection.x, movement->walkDirection.y, movement->walkDirection.z };
        objJson["movement_move_speed"] = movement->moveSpeed;
        objJson["movement_run_speed"] = movement->runSpeed;
        objJson["movement_rotation_speed"] = movement->rotationSpeed;
        objJson["movement_current_speed"] = movement->currentSpeed;
    }

    if (auto* controller = m_registry.GetComponent<CharacterController>(entity))
    {
        objJson["has_character_controller"] = true;
        objJson["controller_capsule_radius"] = controller->capsuleRadius;
        objJson["controller_capsule_height"] = controller->capsuleHeight;
        objJson["controller_capsule_center_offset_y"] = controller->capsuleCenterOffsetY;
        objJson["controller_step_height"] = controller->stepHeight;
        objJson["controller_jump_speed"] = controller->jumpSpeed;
        objJson["controller_gravity"] = controller->gravity;
        objJson["controller_max_slope"] = controller->maxSlope;
    }

    if (auto* tuning = m_registry.GetComponent<CharacterPhysicsTuningComponent>(entity))
    {
        objJson["has_character_physics_tuning"] = true;
        objJson["character_auto_center_offset"] = tuning->autoCalculateCapsuleCenterOffset;
        objJson["character_manual_center_offset_y"] = tuning->manualCapsuleCenterOffsetY;
        objJson["character_foot_clearance"] = tuning->capsuleFootClearance;
        objJson["character_force_y_axis"] = tuning->forceYAxis;
        objJson["character_use_ghost_sweep"] = tuning->useGhostSweepTest;
        objJson["character_max_penetration_depth"] = tuning->maxPenetrationDepth;
        objJson["character_fall_speed"] = tuning->fallSpeed;
        objJson["character_clamp_step"] = tuning->clampNormalStepHeight;
        objJson["character_minimum_step"] = tuning->minimumStepHeight;
        objJson["character_maximum_step"] = tuning->maximumStepHeight;
        objJson["character_migrate_legacy_step"] = tuning->migrateLegacyStepHeight;
        objJson["character_legacy_step"] = tuning->legacyStepHeight;
        objJson["character_legacy_step_replacement"] = tuning->legacyReplacementStepHeight;
        objJson["character_auto_step_enabled"] = tuning->automaticStepEnabled;
        objJson["character_auto_step_predictive"] = tuning->automaticStepPredictive;
        objJson["character_auto_step_height"] = tuning->automaticStepHeight;
        objJson["character_auto_step_delay"] = tuning->automaticStepDelay;
        objJson["character_auto_step_duration"] = tuning->automaticStepDuration;
        objJson["character_auto_step_cooldown"] = tuning->automaticStepCooldown;
        objJson["character_auto_step_progress_distance"] = tuning->automaticStepProgressDistance;
        objJson["character_auto_step_forward_probe"] = tuning->automaticStepForwardProbe;
        objJson["character_auto_step_down_probe"] = tuning->automaticStepDownProbe;
        objJson["character_auto_step_skin"] = tuning->automaticStepSkin;
    }

    if (auto* npc = m_registry.GetComponent<NPCComponent>(entity))
    {
        objJson["has_npc"] = true;
        objJson["npc_name"] = npc->name;
        objJson["npc_description"] = npc->description;
        objJson["npc_health"] = npc->health;
        objJson["npc_max_health"] = npc->maxHealth;
        objJson["npc_hostile"] = npc->isHostile;

        if (auto* npcMovement = m_registry.GetComponent<NPCMovementComponent>(entity))
        {
            objJson["npc_speed"] = npcMovement->speed;
            objJson["npc_run_speed"] = npcMovement->runSpeed;
            objJson["npc_run_distance"] = npcMovement->runWhenFartherThan;
            objJson["npc_arrival_distance"] = npcMovement->arrivalDistance;
            objJson["npc_door_open_distance"] = npcMovement->doorOpenDistance;
            objJson["npc_door_wait_distance"] = npcMovement->doorWaitDistance;
            objJson["npc_allow_running"] = npcMovement->allowRunning;
            objJson["npc_can_fly"] = npcMovement->canFly;
        }

        if (auto* brain = m_registry.GetComponent<NpcBrainComponent>(entity))
        {
            objJson["npc_detection_range"] = brain->detectionRange;
            objJson["npc_repair_distance"] = brain->repairDistance;
            objJson["npc_repair_duration"] = brain->repairDuration;
            objJson["npc_repath_interval"] = brain->repathInterval;
            objJson["npc_stuck_timeout"] = brain->stuckTimeout;
            objJson["npc_wander_radius"] = brain->wanderRadius;
        }
    }

    if (auto* anim = m_registry.GetComponent<AnimationComponent>(entity))
    {
        objJson["has_animation"] = true;
        objJson["animation_state"] = static_cast<int>(anim->currentState);
        objJson["animation_index"] = anim->animationIndex;
        objJson["animation_time"] = anim->currentTime;
        objJson["animation_is_playing"] = anim->isPlaying;
        objJson["animation_bindings"] = json::array();
        for (const AnimationBinding& binding : anim->animations)
        {
            objJson["animation_bindings"].push_back({
                { "state", static_cast<int>(binding.state) },
                { "name", binding.name }
                });
        }
    }
// -- DoorComponent ---------------------------------------------------------
    if (auto* door = m_registry.GetComponent<DoorComponent>(entity))
    {
        objJson["has_door"] = true;
        objJson["door_compartment_a"] = static_cast<uint32_t>(door->compartmentA);
        objJson["door_compartment_b"] = static_cast<uint32_t>(door->compartmentB);
        objJson["door_room_a"] = static_cast<uint32_t>(door->roomA);
        objJson["door_room_b"] = static_cast<uint32_t>(door->roomB);
        objJson["door_is_open"] = door->isOpen;
        objJson["door_is_broken"] = door->isBroken;
        objJson["door_flow_rate"] = door->flowRate;
        objJson["door_requires_power"] = door->requiresPower;
        objJson["door_open_progress"] = door->openProgress;
        objJson["door_open_speed"] = door->openSpeed;
        objJson["door_open_angle"] = door->openAngle;
        objJson["door_closed_rotation"] = { door->closedRotation.x, door->closedRotation.y, door->closedRotation.z };
        objJson["door_rotation_initialized"] = door->rotationInitialized;
        objJson["door_animation_mode"] = static_cast<int>(door->animationMode);
        objJson["door_open_height"] = door->openHeight;
        objJson["door_closed_position"] = { door->closedPosition.x, door->closedPosition.y, door->closedPosition.z };
        objJson["door_position_initialized"] = door->positionInitialized;
        objJson["door_left_leaf"] = static_cast<uint32_t>(door->leftLeaf);
        objJson["door_right_leaf"] = static_cast<uint32_t>(door->rightLeaf);
        objJson["door_left_closed_position"] = { door->leftClosedPosition.x, door->leftClosedPosition.y, door->leftClosedPosition.z };
        objJson["door_right_closed_position"] = { door->rightClosedPosition.x, door->rightClosedPosition.y, door->rightClosedPosition.z };
        objJson["door_leaf_positions_initialized"] = door->leafPositionsInitialized;
        objJson["door_slide_axis"] = { door->slideAxis.x, door->slideAxis.y, door->slideAxis.z };
        objJson["door_slide_distance"] = door->slideDistance;
    }
    // -- RoomComponent ---------------------------------------------------------
    if (auto* room = m_registry.GetComponent<RoomComponent>(entity))
    {
        objJson["has_room"] = true;
        objJson["room_name"] = room->roomName;
        objJson["room_local_min"] = { room->localMin.x, room->localMin.y, room->localMin.z };
        objJson["room_local_max"] = { room->localMax.x, room->localMax.y, room->localMax.z };
        objJson["room_oxygen"] = room->oxygen;
        objJson["room_pressure"] = room->pressure;
        objJson["room_temperature"] = room->temperature;
        objJson["room_leak_rate"] = room->leakRate;
        objJson["room_is_active"] = room->isActive;

        json connectedRoomsJson = json::array();
        for (Entity connected : room->connectedRooms)
            connectedRoomsJson.push_back(static_cast<uint32_t>(connected));
        objJson["room_connected"] = connectedRoomsJson;
    }
    // -- OxygenGeneratorComponent ---------------------------------------------
    if (auto* generator = m_registry.GetComponent<OxygenGeneratorComponent>(entity))
    {
        objJson["has_oxygen_generator"] = true;
        objJson["oxygen_generator_oxygen_per_second"] = generator->oxygenPerSecond;
        objJson["oxygen_generator_pressure_per_second"] = generator->pressurePerSecond;
        objJson["oxygen_generator_temperature_recovery_per_second"] = generator->temperatureRecoveryPerSecond;
        objJson["oxygen_generator_target_temperature"] = generator->targetTemperature;
        objJson["oxygen_generator_power_usage"] = generator->powerUsage;
        objJson["oxygen_generator_enabled"] = generator->enabled;
        objJson["oxygen_generator_has_power"] = generator->hasPower;
        objJson["oxygen_generator_broken"] = generator->broken;
    }
    // -- HullPanelComponent ----------------------------------------------------
    if (auto* panel = m_registry.GetComponent<HullPanelComponent>(entity))
    {
        objJson["has_hull_panel"] = true;
        objJson["panel_name"] = panel->panelName;
        objJson["panel_room_name"] = panel->roomName;
        objJson["panel_room_entity"] = static_cast<uint32_t>(panel->room);
        objJson["panel_max_health"] = panel->maxHealth;
        objJson["panel_health"] = panel->health;
        objJson["panel_accumulated_damage"] = panel->accumulatedDamage;
        objJson["panel_armor_resistance"] = panel->armorResistance;
        objJson["panel_breach_count"] = panel->breachCount;
        objJson["panel_max_breaches"] = panel->maxBreaches;
        objJson["panel_is_external"] = panel->isExternal;
        objJson["panel_is_destroyed"] = panel->isDestroyed;
        objJson["panel_collision_disabled_when_destroyed"] = panel->collisionDisabledWhenDestroyed;
    }

    // -- DamageDecalComponent --------------------------------------------------
    if (auto* decals = m_registry.GetComponent<DamageDecalComponent>(entity))
    {
        objJson["has_damage_decals"] = true;
        json decalArray = json::array();
        for (const auto& crack : decals->cracks)
        {
            decalArray.push_back({
                { "local_position", { crack.localPosition.x, crack.localPosition.y, crack.localPosition.z } },
                { "local_normal", { crack.localNormal.x, crack.localNormal.y, crack.localNormal.z } },
                { "radius", crack.radius },
                { "intensity", crack.intensity },
                { "rotation", crack.rotation }
            });
        }
        objJson["damage_decals"] = std::move(decalArray);
    }
    // -- HullBreachComponent ---------------------------------------------------
    if (auto* breach = m_registry.GetComponent<HullBreachComponent>(entity))
    {
        objJson["has_hull_breach"] = true;
        objJson["breach_panel"] = static_cast<uint32_t>(breach->panel);
        objJson["breach_room"] = static_cast<uint32_t>(breach->room);
        objJson["breach_local_position"] = { breach->localPosition.x, breach->localPosition.y, breach->localPosition.z };
        objJson["breach_local_normal"] = { breach->localNormal.x, breach->localNormal.y, breach->localNormal.z };
        objJson["breach_world_position"] = { breach->worldPosition.x, breach->worldPosition.y, breach->worldPosition.z };
        objJson["breach_radius"] = breach->radius;
        objJson["breach_leak_rate"] = breach->leakRate;
        objJson["breach_sealed"] = breach->sealed;
    }

    // -- RepairableComponent ---------------------------------------------------
    if (auto* repairable = m_registry.GetComponent<RepairableComponent>(entity))
    {
        objJson["has_repairable"] = true;
        objJson["repair_progress"] = repairable->progress;
        objJson["repair_required_progress"] = repairable->requiredProgress;
        objJson["repair_rate"] = repairable->repairRate;
        objJson["repair_max_ray_distance"] = repairable->maxRayDistance;
        objJson["repair_repaired"] = repairable->repaired;
    }
    // -- CompartmentComponent --------------------------------------------------
    if (auto* comp = m_registry.GetComponent<CompartmentComponent>(entity))
    {
        objJson["has_compartment"] = true;
        objJson["compartment_local_min"] = { comp->localMin.x, comp->localMin.y, comp->localMin.z };
        objJson["compartment_local_max"] = { comp->localMax.x, comp->localMax.y, comp->localMax.z };
        objJson["compartment_volume"] = comp->volume;
        objJson["compartment_water_volume"] = comp->waterVolume;
        objJson["compartment_water_level"] = comp->waterLevel;
        objJson["compartment_is_sealed"] = comp->isSealed;
        objJson["compartment_has_power"] = comp->hasPower;
        objJson["compartment_pump_rate"] = comp->pumpRate;
        objJson["compartment_damage_visual"] = comp->damageVisual;

        json breachesJson = json::array();
        for (const auto& breach : comp->breaches)
        {
            json b;
            b["local_position"] = { breach.localPosition.x, breach.localPosition.y, breach.localPosition.z };
            b["size"] = breach.size;
            b["flow_rate"] = breach.flowRate;
            b["is_sealed"] = breach.isSealed;
            breachesJson.push_back(b);
        }
        objJson["compartment_breaches"] = breachesJson;

        json connectedJson = json::array();
        for (Entity connected : comp->connectedCompartments)
            connectedJson.push_back(static_cast<uint32_t>(connected));
        objJson["compartment_connected"] = connectedJson;
    }

    // -- SubmarineComponent ----------------------------------------------------
    if (auto* sub = m_registry.GetComponent<SubmarineComponent>(entity))
    {
        objJson["has_submarine"] = true;
        objJson["sub_root_entity"] = static_cast<uint32_t>(sub->submarineRoot);
        objJson["sub_compartment_index"] = sub->compartmentIndex;
        objJson["sub_is_detached"] = sub->isDetached;
        json allCompartmentsJson = json::array();
        for (Entity comp : sub->allCompartments)
            allCompartmentsJson.push_back(static_cast<uint32_t>(comp));
        objJson["sub_all_compartments"] = allCompartmentsJson;
    }

    // -- PressureHullComponent -------------------------------------------------
    if (auto* phull = m_registry.GetComponent<PressureHullComponent>(entity))
    {
        objJson["has_pressure_hull"] = true;
        objJson["hull_max_pressure"] = phull->maxPressure;
        objJson["hull_current_health"] = phull->currentHealth;
        objJson["hull_repair_rate"] = phull->repairRate;
        objJson["hull_is_leaking"] = phull->isLeaking;
        objJson["hull_leak_intensity"] = phull->leakIntensity;
    }

    // -- SubmarineJointComponent -----------------------------------------------
    if (auto* joint = m_registry.GetComponent<SubmarineJointComponent>(entity))
    {
        objJson["has_sub_joint"] = true;
        objJson["sub_joint_comp_a"] = static_cast<uint32_t>(joint->compartmentA);
        objJson["sub_joint_comp_b"] = static_cast<uint32_t>(joint->compartmentB);
        objJson["sub_joint_break_force"] = joint->breakForce;
        objJson["sub_joint_current_stress"] = joint->currentStress;
        objJson["sub_joint_is_broken"] = joint->isBroken;
    }

    // -- JointConnectionComponent ----------------------------------------------
    if (auto* conn = m_registry.GetComponent<JointConnectionComponent>(entity))
    {
        objJson["has_joint_connection"] = true;
        objJson["joint_target_entity"] = static_cast<uint32_t>(conn->targetEntity);
        objJson["joint_pivot_a"] = { conn->pivotA.x(), conn->pivotA.y(), conn->pivotA.z() };
        objJson["joint_pivot_b"] = { conn->pivotB.x(), conn->pivotB.y(), conn->pivotB.z() };
        objJson["joint_break_force"] = conn->breakForce;
        objJson["joint_disable_collisions"] = conn->disableCollisions;
        objJson["joint_is_broken"] = conn->isBroken;
        objJson["joint_show_gizmo"] = conn->showGizmo;
        objJson["joint_gizmo_color"] = { conn->gizmoColor.x, conn->gizmoColor.y, conn->gizmoColor.z, conn->gizmoColor.w };
    }

    if (auto* zone = m_registry.GetComponent<WorkZoneComponent>(entity))
    {
        objJson["has_work_zone"] = true;
        objJson["work_zone_owner"] = static_cast<uint32_t>(zone->owner);
    }

    return objJson;
}

bool SceneManager::LoadScene(const std::string& filename,
    ID3D11Device* device, ID3D11DeviceContext* deviceContext)
{
    std::ifstream file(filename);
    if (!file.is_open()) return false;

    json scene;
    file >> scene;

    NpcNavMeshSystem::InvalidateAll();
    m_registry = ECSRegistry{};
    m_importedShipModels.clear();
    std::unordered_map<uint32_t, Entity> remap;

    if (scene.contains("world"))
    {
        const auto& world = scene["world"];
        m_worldSettings.fogEnabled = world.value("fog_enabled", m_worldSettings.fogEnabled);
        if (world.contains("fog_color"))
        {
            const auto& c = world["fog_color"];
            m_worldSettings.fogColor = { c[0], c[1], c[2] };
        }
        m_worldSettings.fogStart = world.value("fog_start", m_worldSettings.fogStart);
        m_worldSettings.fogEnd = world.value("fog_end", m_worldSettings.fogEnd);
        m_worldSettings.skyboxPath = world.value("skybox_path", m_worldSettings.skyboxPath);
    }

    if (scene.contains("objects") && scene["objects"].is_array())
    {
        for (const auto& obj : scene["objects"])
        {
            Entity e = INVALID_ENTITY;
            if (obj.value("has_ship_root", false))
            {
                const std::string shipPath = obj.value("ship_source_path", std::string());
                if (!shipPath.empty())
                {
                    e = ImportShipFromGlb(shipPath, device, deviceContext);
                    if (auto* transform = m_registry.GetComponent<TransformComponent>(e))
                    {
                        if (obj.contains("position") && obj["position"].is_object())
                        {
                            transform->position = {
                                obj["position"].value("x", transform->position.x),
                                obj["position"].value("y", transform->position.y),
                                obj["position"].value("z", transform->position.z)
                            };
                        }
                        if (obj.contains("rotation") && obj["rotation"].is_object())
                        {
                            transform->rotation = {
                                obj["rotation"].value("x", transform->rotation.x),
                                obj["rotation"].value("y", transform->rotation.y),
                                obj["rotation"].value("z", transform->rotation.z)
                            };
                        }
                        if (obj.contains("scale") && obj["scale"].is_object())
                        {
                            transform->scale = {
                                obj["scale"].value("x", transform->scale.x),
                                obj["scale"].value("y", transform->scale.y),
                                obj["scale"].value("z", transform->scale.z)
                            };
                        }
                    }
                    if (auto* shipRoot = m_registry.GetComponent<ShipRootComponent>(e))
                        shipRoot->sourcePath = shipPath;

                    if (auto* grid = m_registry.GetComponent<PowerGridComponent>(e))
                    {
                        grid->batteryCapacityMwh = obj.value(
                            "power_grid_battery_capacity_mwh",
                            grid->batteryCapacityMwh);
                        grid->batteryChargeMwh = obj.value(
                            "power_grid_battery_charge_mwh",
                            grid->batteryChargeMwh);
                        grid->maxBatteryChargeMw = obj.value(
                            "power_grid_max_charge_mw",
                            grid->maxBatteryChargeMw);
                        grid->maxBatteryDischargeMw = obj.value(
                            "power_grid_max_discharge_mw",
                            grid->maxBatteryDischargeMw);
                    }

                    if (obj.contains("ship_generators") && obj["ship_generators"].is_array())
                    {
                        for (const auto& savedGenerator : obj["ship_generators"])
                        {
                            const std::string savedName = savedGenerator.value("name", std::string());

                            for (auto& [generatorEntity, generator] :
                                m_registry.GetComponents<GeneratorComponent>())
                            {
                                const auto* part = m_registry.GetComponent<ShipPartComponent>(generatorEntity);
                                const auto* name = m_registry.GetComponent<NameComponent>(generatorEntity);
                                if (!part || part->root != e || !name || name->name != savedName)
                                    continue;

                                generator.deuteriumKg = savedGenerator.value(
                                    "deuterium_kg", generator.deuteriumKg);
                                generator.tritiumKg = savedGenerator.value(
                                    "tritium_kg", generator.tritiumKg);
                                generator.currentOutputMw = savedGenerator.value(
                                    "current_output_mw", generator.currentOutputMw);
                                generator.targetOutputMw = savedGenerator.value(
                                    "target_output_mw", generator.targetOutputMw);
                                generator.heat = savedGenerator.value(
                                    "temperature", generator.heat);
                                generator.isOnline = savedGenerator.value(
                                    "is_online", generator.isOnline);
                                generator.isScrammed = savedGenerator.value(
                                    "is_scrammed", generator.isScrammed);
                                break;
                            }
                        }
                    }
                }
            }
            else
            {
                e = DeserializeObject(obj, device, deviceContext);
            }

            if (e != INVALID_ENTITY && obj.contains("entity_id"))
                remap[obj["entity_id"].get<uint32_t>()] = e;
        }
    }

    RemapEntityReferences(remap);
    ResolveRepairZones();
    // Temporary GUN <-> CAPTAIN staircase portal is idempotent: if it was
    // already saved (or replaced by a real door), this call creates nothing.
    ConnectDefaultShipRooms();
    return true;
}

Entity SceneManager::DeserializeObject(const json& j, ID3D11Device* device,
    ID3D11DeviceContext* deviceContext)
{
    auto readVec3Array = [](const json& value, const XMFLOAT3& fallback) -> XMFLOAT3
    {
        if (!value.is_array() || value.size() < 3) return fallback;
        return { value[0].get<float>(), value[1].get<float>(), value[2].get<float>() };
    };

    auto readVec3Object = [](const json& value, const XMFLOAT3& fallback) -> XMFLOAT3
    {
        if (!value.is_object()) return fallback;
        return {
            value.value("x", fallback.x),
            value.value("y", fallback.y),
            value.value("z", fallback.z)
        };
    };

    Entity e = m_registry.CreateEntity();

    TransformComponent transform;
    if (j.contains("position")) transform.position = readVec3Object(j["position"], transform.position);
    if (j.contains("rotation")) transform.rotation = readVec3Object(j["rotation"], transform.rotation);
    if (j.contains("scale")) transform.scale = readVec3Object(j["scale"], transform.scale);
    m_registry.AddComponent<TransformComponent>(e, transform);
    m_registry.AddComponent<ActiveComponent>(e, ActiveComponent{});

    if (j.contains("object_name"))
        m_registry.AddComponent<NameComponent>(e, NameComponent{ j["object_name"].get<std::string>() });

    const std::string modelPath = j.value("model_path", std::string());
    const std::string texturePath = j.value("texture_path", std::string());
    if (!modelPath.empty())
    {
        if (AssimpModelClass* model = GetOrCreateModel(modelPath, device, deviceContext, texturePath, true))
        {
            MeshRendererComponent mesh;
            mesh.model = model;
            mesh.asyncModelPath = CanUseAsyncMeshPath(modelPath) ? modelPath : "";
            mesh.useAsyncMesh = !mesh.asyncModelPath.empty();

            auto* material = new MaterialClass();
            if (material->Initialize(device, L"LoadedObjectMaterial"))
            {
                if (model->HasAnimations())
                {
                    if (auto* pbrShader = ShaderManager::Get().GetShaderType(ShaderType::PBR))
                        material->SetShader(pbrShader);
                    else if (m_defaultMaterial && m_defaultMaterial->GetShader())
                        material->SetShader(m_defaultMaterial->GetShader());
                }
                else if (m_defaultMaterial && m_defaultMaterial->GetShader())
                {
                    material->SetShader(m_defaultMaterial->GetShader());
                }

                if (!texturePath.empty())
                {
                    auto texture = std::make_unique<TextureClass>();
                    if (texture->Initialize(device, deviceContext, texturePath.c_str()))
                    {
                        material->AdoptTexture(0, std::move(texture));
                        material->SetTexturePath(0, texturePath);
                    }
                }

                mesh.material = material;
            }
            else
            {
                delete material;
                mesh.material = nullptr;
            }

            m_registry.AddComponent<MeshRendererComponent>(e, mesh);

            if (j.value("has_animation", false) || model->HasAnimations())
            {
                AnimationComponent anim;
                anim.currentState = static_cast<AnimationState>(j.value("animation_state", static_cast<int>(anim.currentState)));
                anim.animationIndex = j.value("animation_index", anim.animationIndex);
                anim.currentTime = j.value("animation_time", anim.currentTime);
                anim.isPlaying = j.value("animation_is_playing", anim.isPlaying);
                if (j.contains("animation_bindings") && j["animation_bindings"].is_array())
                {
                    for (const auto& savedBinding : j["animation_bindings"])
                    {
                        const auto savedState = static_cast<AnimationState>(
                            savedBinding.value("state", static_cast<int>(AnimationState::Idle)));
                        for (AnimationBinding& binding : anim.animations)
                        {
                            if (binding.state == savedState)
                            {
                                binding.name = savedBinding.value("name", binding.name);
                                binding.index = -1;
                                break;
                            }
                        }
                    }
                }
                m_registry.AddComponent<AnimationComponent>(e, anim);
            }
        }
    }

    if (j.value("has_rigidbody", false))
    {
        RigidBodyComponent rig;
        rig.mass = j.value("rb_mass", rig.mass);
        rig.isDynamic = j.value("rb_is_dynamic", rig.isDynamic);
        rig.isKinematic = j.value("rb_is_kinematic", rig.isKinematic);
        rig.enableCCD = j.value("rb_enable_ccd", rig.enableCCD);
        rig.linearDamping = j.value("rb_linear_damping", rig.linearDamping);
        rig.angularDamping = j.value("rb_angular_damping", rig.angularDamping);
        rig.friction = j.value("rb_friction", rig.friction);
        rig.shapeType = static_cast<RigidBodyComponent::ShapeType>(j.value("rb_shape_type", static_cast<int>(rig.shapeType)));
        rig.shapeRadius = j.value("rb_shape_radius", rig.shapeRadius);
        rig.useSeparateCollisionMesh = j.value("rb_use_separate_collision_mesh", rig.useSeparateCollisionMesh);
        rig.collisionModelPath = j.value("rb_collision_model_path", rig.collisionModelPath);
        rig.useManualSize = j.value("rb_use_manual_size", rig.useManualSize);
        if (j.contains("rb_shape_size") && j["rb_shape_size"].is_array() && j["rb_shape_size"].size() >= 3)
            rig.shapeSize = btVector3(j["rb_shape_size"][0].get<float>(), j["rb_shape_size"][1].get<float>(), j["rb_shape_size"][2].get<float>());
        if (rig.useSeparateCollisionMesh && !rig.collisionModelPath.empty())
            rig.collisionModel = GetOrCreateModel(rig.collisionModelPath, device, deviceContext, "", true);
        else if (auto* mesh = m_registry.GetComponent<MeshRendererComponent>(e))
            rig.collisionModel = mesh->model;
        m_registry.AddComponent<RigidBodyComponent>(e, std::move(rig));
    }

    if (j.value("has_static_collider", false))
        m_registry.AddComponent<StaticColliderComponent>(e, StaticColliderComponent{});

    if (j.value("is_light", false))
    {
        LightComponent light;
        if (j.contains("light_color") && j["light_color"].is_array() && j["light_color"].size() >= 4)
        {
            light.color = {
                j["light_color"][0].get<float>(), j["light_color"][1].get<float>(),
                j["light_color"][2].get<float>(), j["light_color"][3].get<float>() };
        }
        light.enabled = j.value("light_enabled", light.enabled);
        light.radius = j.value("light_radius", light.radius);
        if (j.contains("light_offset")) light.offset = readVec3Array(j["light_offset"], light.offset);
        light.isEditorGizmo = j.value("light_is_gizmo", light.isEditorGizmo);
        light.attachedTo = static_cast<Entity>(j.value(
            "light_attached_to", static_cast<uint32_t>(light.attachedTo)));
        m_registry.AddComponent<LightComponent>(e, light);

        if (j.value("has_spotlight", false))
        {
            SpotLightComponent spot;
            if (j.contains("spotlight_direction"))
                spot.localDirection = readVec3Array(j["spotlight_direction"], spot.localDirection);
            spot.innerAngleDegrees = j.value("spotlight_inner_angle", spot.innerAngleDegrees);
            spot.outerAngleDegrees = j.value("spotlight_outer_angle", spot.outerAngleDegrees);
            m_registry.AddComponent<SpotLightComponent>(e, spot);
        }
    }

    if (j.value("has_buoyancy", false))
    {
        BuoyancyComponent buoyancy;
        buoyancy.fluidDensity = j.value("buoyancy_fluid_density", buoyancy.fluidDensity);
        buoyancy.volume = j.value("buoyancy_volume", buoyancy.volume);
        buoyancy.height = j.value("buoyancy_height", buoyancy.height);
        buoyancy.buoyancyMultiplier = j.value("buoyancy_multiplier", buoyancy.buoyancyMultiplier);
        m_registry.AddComponent<BuoyancyComponent>(e, buoyancy);
    }

    if (j.value("has_door", false))
    {
        DoorComponent door;
        door.compartmentA = static_cast<Entity>(j.value("door_compartment_a", static_cast<uint32_t>(INVALID_ENTITY)));
        door.compartmentB = static_cast<Entity>(j.value("door_compartment_b", static_cast<uint32_t>(INVALID_ENTITY)));
        door.roomA = static_cast<Entity>(j.value("door_room_a", static_cast<uint32_t>(INVALID_ENTITY)));
        door.roomB = static_cast<Entity>(j.value("door_room_b", static_cast<uint32_t>(INVALID_ENTITY)));
        door.isOpen = j.value("door_is_open", door.isOpen);
        door.isBroken = j.value("door_is_broken", door.isBroken);
        door.flowRate = j.value("door_flow_rate", door.flowRate);
        door.requiresPower = j.value("door_requires_power", door.requiresPower);
        door.openProgress = j.value("door_open_progress", door.openProgress);
        door.openSpeed = j.value("door_open_speed", door.openSpeed);
        door.openAngle = j.value("door_open_angle", door.openAngle);
        if (j.contains("door_closed_rotation")) door.closedRotation = readVec3Array(j["door_closed_rotation"], door.closedRotation);
        door.rotationInitialized = j.value("door_rotation_initialized", door.rotationInitialized);
        door.animationMode = static_cast<DoorAnimationMode>(j.value("door_animation_mode", static_cast<int>(door.animationMode)));
        door.openHeight = j.value("door_open_height", door.openHeight);
        if (j.contains("door_closed_position")) door.closedPosition = readVec3Array(j["door_closed_position"], door.closedPosition);
        door.positionInitialized = j.value("door_position_initialized", door.positionInitialized);
        door.leftLeaf = static_cast<Entity>(j.value("door_left_leaf", static_cast<uint32_t>(door.leftLeaf)));
        door.rightLeaf = static_cast<Entity>(j.value("door_right_leaf", static_cast<uint32_t>(door.rightLeaf)));
        if (j.contains("door_left_closed_position")) door.leftClosedPosition = readVec3Array(j["door_left_closed_position"], door.leftClosedPosition);
        if (j.contains("door_right_closed_position")) door.rightClosedPosition = readVec3Array(j["door_right_closed_position"], door.rightClosedPosition);
        door.leafPositionsInitialized = j.value("door_leaf_positions_initialized", door.leafPositionsInitialized);
        if (j.contains("door_slide_axis")) door.slideAxis = readVec3Array(j["door_slide_axis"], door.slideAxis);
        door.slideDistance = j.value("door_slide_distance", door.slideDistance);
        m_registry.AddComponent<DoorComponent>(e, door);
    }

    if (j.value("has_room", false))
    {
        RoomComponent room;
        room.roomName = j.value("room_name", room.roomName);
        if (j.contains("room_local_min")) room.localMin = readVec3Array(j["room_local_min"], room.localMin);
        if (j.contains("room_local_max")) room.localMax = readVec3Array(j["room_local_max"], room.localMax);
        room.oxygen = j.value("room_oxygen", room.oxygen);
        room.pressure = j.value("room_pressure", room.pressure);
        room.temperature = j.value("room_temperature", room.temperature);
        room.leakRate = j.value("room_leak_rate", room.leakRate);
        room.isActive = j.value("room_is_active", room.isActive);
        if (j.contains("room_connected") && j["room_connected"].is_array())
        {
            for (const auto& connected : j["room_connected"])
                room.connectedRooms.push_back(static_cast<Entity>(connected.get<uint32_t>()));
        }
        m_registry.AddComponent<RoomComponent>(e, room);
    }

    if (j.value("has_oxygen_generator", false))
    {
        OxygenGeneratorComponent generator;
        generator.oxygenPerSecond = j.value("oxygen_generator_oxygen_per_second", generator.oxygenPerSecond);
        generator.pressurePerSecond = j.value("oxygen_generator_pressure_per_second", generator.pressurePerSecond);
        generator.temperatureRecoveryPerSecond = j.value(
            "oxygen_generator_temperature_recovery_per_second",
            generator.temperatureRecoveryPerSecond);
        generator.targetTemperature = j.value(
            "oxygen_generator_target_temperature",
            generator.targetTemperature);
        generator.powerUsage = j.value("oxygen_generator_power_usage", generator.powerUsage);
        generator.enabled = j.value("oxygen_generator_enabled", generator.enabled);
        generator.hasPower = j.value("oxygen_generator_has_power", generator.hasPower);
        generator.broken = j.value("oxygen_generator_broken", generator.broken);
        m_registry.AddComponent<OxygenGeneratorComponent>(e, generator);
    }
    if (j.value("has_hull_panel", false))
    {
        HullPanelComponent panel;
        panel.panelName = j.value("panel_name", std::string("Panel"));
        panel.roomName = j.value("panel_room_name", std::string());
        panel.room = static_cast<Entity>(j.value("panel_room_entity", static_cast<uint32_t>(INVALID_ENTITY)));
        panel.maxHealth = j.value("panel_max_health", 100.0f);
        panel.health = j.value("panel_health", panel.maxHealth);
        panel.accumulatedDamage = j.value("panel_accumulated_damage", 0.0f);
        panel.armorResistance = j.value("panel_armor_resistance", 5.0f);
        panel.breachCount = j.value("panel_breach_count", 0);
        panel.maxBreaches = j.value("panel_max_breaches", 3);
        panel.isExternal = j.value("panel_is_external", true);
        panel.isDestroyed = j.value("panel_is_destroyed", false);
        panel.collisionDisabledWhenDestroyed = j.value("panel_collision_disabled_when_destroyed", true);
        m_registry.AddComponent<HullPanelComponent>(e, panel);
    }

    if (j.value("has_damage_decals", false))
    {
        DamageDecalComponent decals;
        if (j.contains("damage_decals") && j["damage_decals"].is_array())
        {
            for (const auto& crackJson : j["damage_decals"])
            {
                if (decals.cracks.size() >= DamageDecalComponent::MAX_CRACKS)
                    break;

                DamageDecalComponent::CrackEntry crack;
                if (crackJson.contains("local_position"))
                    crack.localPosition = readVec3Array(crackJson["local_position"], crack.localPosition);
                if (crackJson.contains("local_normal"))
                    crack.localNormal = readVec3Array(crackJson["local_normal"], crack.localNormal);
                crack.radius = crackJson.value("radius", crack.radius);
                crack.intensity = crackJson.value("intensity", crack.intensity);
                crack.rotation = crackJson.value("rotation", crack.rotation);
                decals.cracks.push_back(crack);
            }
        }
        m_registry.AddComponent<DamageDecalComponent>(e, std::move(decals));
    }
    if (j.value("has_hull_breach", false))
    {
        HullBreachComponent breach;
        breach.panel = static_cast<Entity>(j.value("breach_panel", static_cast<uint32_t>(INVALID_ENTITY)));
        breach.room = static_cast<Entity>(j.value("breach_room", static_cast<uint32_t>(INVALID_ENTITY)));
        if (j.contains("breach_local_position")) breach.localPosition = readVec3Array(j["breach_local_position"], breach.localPosition);
        if (j.contains("breach_local_normal")) breach.localNormal = readVec3Array(j["breach_local_normal"], breach.localNormal);
        if (j.contains("breach_world_position")) breach.worldPosition = readVec3Array(j["breach_world_position"], breach.worldPosition);
        breach.radius = j.value("breach_radius", breach.radius);
        breach.leakRate = j.value("breach_leak_rate", breach.leakRate);
        breach.sealed = j.value("breach_sealed", breach.sealed);
        m_registry.AddComponent<HullBreachComponent>(e, breach);
    }

    if (j.value("has_repairable", false) || j.value("has_hull_breach", false))
    {
        RepairableComponent repairable;
        repairable.progress = j.value("repair_progress", repairable.progress);
        repairable.requiredProgress = j.value("repair_required_progress", repairable.requiredProgress);
        repairable.repairRate = j.value("repair_rate", repairable.repairRate);
        repairable.maxRayDistance = j.value("repair_max_ray_distance", repairable.maxRayDistance);
        repairable.repaired = j.value("repair_repaired", repairable.repaired);
        m_registry.AddComponent<RepairableComponent>(e, repairable);
    }

    if (j.value("has_work_zone", false))
    {
        WorkZoneComponent zone;
        zone.owner = static_cast<Entity>(
            j.value("work_zone_owner", static_cast<uint32_t>(INVALID_ENTITY)));
        m_registry.AddComponent<WorkZoneComponent>(e, zone);
    }

    if (j.value("has_compartment", false))
    {
        CompartmentComponent comp;
        if (j.contains("compartment_local_min")) comp.localMin = readVec3Array(j["compartment_local_min"], comp.localMin);
        if (j.contains("compartment_local_max")) comp.localMax = readVec3Array(j["compartment_local_max"], comp.localMax);
        comp.volume = j.value("compartment_volume", comp.volume);
        comp.waterVolume = j.value("compartment_water_volume", comp.waterVolume);
        comp.waterLevel = j.value("compartment_water_level", comp.waterLevel);
        comp.isSealed = j.value("compartment_is_sealed", comp.isSealed);
        comp.hasPower = j.value("compartment_has_power", comp.hasPower);
        comp.pumpRate = j.value("compartment_pump_rate", comp.pumpRate);
        comp.damageVisual = j.value("compartment_damage_visual", comp.damageVisual);
        if (j.contains("compartment_breaches") && j["compartment_breaches"].is_array())
        {
            for (const auto& bJson : j["compartment_breaches"])
            {
                BreachData breach;
                if (bJson.contains("local_position")) breach.localPosition = readVec3Array(bJson["local_position"], breach.localPosition);
                breach.size = bJson.value("size", breach.size);
                breach.flowRate = bJson.value("flow_rate", breach.flowRate);
                breach.isSealed = bJson.value("is_sealed", breach.isSealed);
                comp.breaches.push_back(breach);
            }
        }
        if (j.contains("compartment_connected") && j["compartment_connected"].is_array())
        {
            for (const auto& connected : j["compartment_connected"])
                comp.connectedCompartments.push_back(static_cast<Entity>(connected.get<uint32_t>()));
        }
        comp.CacheSize();
        m_registry.AddComponent<CompartmentComponent>(e, comp);
    }

    if (j.value("has_submarine", false))
    {
        SubmarineComponent sub;
        sub.submarineRoot = static_cast<Entity>(j.value("sub_root_entity", static_cast<uint32_t>(INVALID_ENTITY)));
        sub.compartmentIndex = j.value("sub_compartment_index", sub.compartmentIndex);
        sub.isDetached = j.value("sub_is_detached", sub.isDetached);
        if (j.contains("sub_all_compartments") && j["sub_all_compartments"].is_array())
        {
            for (const auto& comp : j["sub_all_compartments"])
                sub.allCompartments.push_back(static_cast<Entity>(comp.get<uint32_t>()));
        }
        sub.needsPhysicsUpdate = true;
        sub.compartmentCountLastFrame = -1;
        m_registry.AddComponent<SubmarineComponent>(e, sub);
    }

    if (j.value("has_pressure_hull", false))
    {
        PressureHullComponent phull;
        phull.maxPressure = j.value("hull_max_pressure", phull.maxPressure);
        phull.currentHealth = j.value("hull_current_health", phull.currentHealth);
        phull.repairRate = j.value("hull_repair_rate", phull.repairRate);
        phull.isLeaking = j.value("hull_is_leaking", phull.isLeaking);
        phull.leakIntensity = j.value("hull_leak_intensity", phull.leakIntensity);
        m_registry.AddComponent<PressureHullComponent>(e, phull);
    }

    if (j.value("has_sub_joint", false))
    {
        SubmarineJointComponent joint;
        joint.compartmentA = static_cast<Entity>(j.value("sub_joint_comp_a", static_cast<uint32_t>(INVALID_ENTITY)));
        joint.compartmentB = static_cast<Entity>(j.value("sub_joint_comp_b", static_cast<uint32_t>(INVALID_ENTITY)));
        joint.breakForce = j.value("sub_joint_break_force", joint.breakForce);
        joint.currentStress = j.value("sub_joint_current_stress", joint.currentStress);
        joint.isBroken = j.value("sub_joint_is_broken", joint.isBroken);
        joint.joint = nullptr;
        m_registry.AddComponent<SubmarineJointComponent>(e, joint);
    }

    if (j.value("has_joint_connection", false))
    {
        JointConnectionComponent conn;
        conn.targetEntity = static_cast<Entity>(j.value("joint_target_entity", static_cast<uint32_t>(INVALID_ENTITY)));
        if (j.contains("joint_pivot_a") && j["joint_pivot_a"].is_array()) conn.pivotA = btVector3(j["joint_pivot_a"][0].get<float>(), j["joint_pivot_a"][1].get<float>(), j["joint_pivot_a"][2].get<float>());
        if (j.contains("joint_pivot_b") && j["joint_pivot_b"].is_array()) conn.pivotB = btVector3(j["joint_pivot_b"][0].get<float>(), j["joint_pivot_b"][1].get<float>(), j["joint_pivot_b"][2].get<float>());
        conn.breakForce = j.value("joint_break_force", conn.breakForce);
        conn.disableCollisions = j.value("joint_disable_collisions", conn.disableCollisions);
        conn.isBroken = j.value("joint_is_broken", conn.isBroken);
        conn.showGizmo = j.value("joint_show_gizmo", conn.showGizmo);
        if (j.contains("joint_gizmo_color") && j["joint_gizmo_color"].is_array())
        {
            const auto& c = j["joint_gizmo_color"];
            conn.gizmoColor = { c[0], c[1], c[2], c[3] };
        }
        conn.joint = nullptr;
        m_registry.AddComponent<JointConnectionComponent>(e, conn);
    }

    if (j.value("has_player_tag", false))
        m_registry.AddComponent<PlayerGameTag>(e, PlayerGameTag{});

    if (j.value("has_first_person_view", false))
    {
        FirstPersonViewComponent firstPerson;
        firstPerson.owner = static_cast<Entity>(
            j.value("first_person_owner", static_cast<uint32_t>(INVALID_ENTITY)));
        if (j.contains("first_person_camera_offset"))
            firstPerson.cameraOffset = readVec3Array(
                j["first_person_camera_offset"], firstPerson.cameraOffset);
        if (j.contains("first_person_rotation_offset"))
            firstPerson.rotationOffset = readVec3Array(
                j["first_person_rotation_offset"], firstPerson.rotationOffset);
        if (j.contains("first_person_scale"))
            firstPerson.scale = readVec3Array(j["first_person_scale"], firstPerson.scale);
        firstPerson.pitchScale = j.value("first_person_pitch_scale", firstPerson.pitchScale);
        firstPerson.pitchOffset = j.value("first_person_pitch_offset", firstPerson.pitchOffset);
        firstPerson.followCamera = j.value("first_person_follow_camera", firstPerson.followCamera);
        firstPerson.syncAnimation = j.value("first_person_sync_animation", firstPerson.syncAnimation);
        firstPerson.hideOwner = j.value("first_person_hide_owner", firstPerson.hideOwner);
        m_registry.AddComponent<FirstPersonViewComponent>(e, firstPerson);
    }

    if (j.value("has_bone_attachment", false))
    {
        BoneAttachmentComponent attachment;
        attachment.parent = static_cast<Entity>(
            j.value("bone_attachment_parent", static_cast<uint32_t>(INVALID_ENTITY)));
        attachment.boneName = j.value("bone_attachment_name", attachment.boneName);
        if (j.contains("bone_attachment_position"))
            attachment.localPosition = readVec3Array(
                j["bone_attachment_position"], attachment.localPosition);
        if (j.contains("bone_attachment_rotation"))
            attachment.localRotation = readVec3Array(
                j["bone_attachment_rotation"], attachment.localRotation);
        if (j.contains("bone_attachment_scale"))
            attachment.localScale = readVec3Array(
                j["bone_attachment_scale"], attachment.localScale);
        attachment.enabled = j.value("bone_attachment_enabled", attachment.enabled);
        m_registry.AddComponent<BoneAttachmentComponent>(e, attachment);
    }

    if (j.value("has_equipped_weapon", false))
    {
        EquippedWeaponComponent equipped;
        equipped.weapon = static_cast<Entity>(
            j.value("equipped_weapon", static_cast<uint32_t>(INVALID_ENTITY)));
        m_registry.AddComponent<EquippedWeaponComponent>(e, equipped);
    }

    if (j.value("has_player_component", false))
    {
        PlayerComponent player;
        player.maxHealth = j.value("player_max_health", j.value("Max_Health", player.maxHealth));
        player.maxStamina = j.value("player_max_stamina", j.value("Max_Stamina", player.maxStamina));
        player.maxOxygen = j.value("player_max_oxygen", j.value("Max_Oxygen", player.maxOxygen));
        player.health = j.value("player_health", j.value("Current_Health", player.health));
        player.stamina = j.value("player_stamina", j.value("Current_Stamina", player.stamina));
        player.oxygen = j.value("player_oxygen", j.value("Current_Oxygen", player.oxygen));
        player.Normalize();
        m_registry.AddComponent<PlayerComponent>(e, player);
    }

    if (j.value("has_weapon", false))
    {
        WeaponComponent weapon;
        weapon.type = static_cast<WeaponType>(j.value("weapon_type", static_cast<int>(weapon.type)));
        weapon.damage = j.value("weapon_damage", weapon.damage);
        weapon.range = j.value("weapon_range", weapon.range);
        weapon.fireRate = j.value("weapon_fire_rate", weapon.fireRate);
        weapon.currentCooldown = 0.0f;
        weapon.breachRadius = j.value("weapon_breach_radius", weapon.breachRadius);
        weapon.visualModelPath = j.value("weapon_visual_model", weapon.visualModelPath);
        weapon.rightHandBone = j.value("weapon_right_hand_bone", weapon.rightHandBone);
        weapon.rifleIdleAnimation = j.value(
            "weapon_rifle_idle_animation", weapon.rifleIdleAnimation);
        weapon.rifleGunAnimation = j.value(
            "weapon_rifle_gun_animation", weapon.rifleGunAnimation);
        if (j.contains("weapon_world_grip_position"))
            weapon.worldGrip.position = readVec3Array(
                j["weapon_world_grip_position"], weapon.worldGrip.position);
        if (j.contains("weapon_world_grip_rotation"))
            weapon.worldGrip.rotation = readVec3Array(
                j["weapon_world_grip_rotation"], weapon.worldGrip.rotation);
        if (j.contains("weapon_world_grip_scale"))
            weapon.worldGrip.scale = readVec3Array(
                j["weapon_world_grip_scale"], weapon.worldGrip.scale);
        if (j.contains("weapon_fp_grip_position"))
            weapon.firstPersonGrip.position = readVec3Array(
                j["weapon_fp_grip_position"], weapon.firstPersonGrip.position);
        if (j.contains("weapon_fp_grip_rotation"))
            weapon.firstPersonGrip.rotation = readVec3Array(
                j["weapon_fp_grip_rotation"], weapon.firstPersonGrip.rotation);
        if (j.contains("weapon_fp_grip_scale"))
            weapon.firstPersonGrip.scale = readVec3Array(
                j["weapon_fp_grip_scale"], weapon.firstPersonGrip.scale);
        weapon.firstPersonPitchScale = j.value(
            "weapon_fp_pitch_scale", weapon.firstPersonPitchScale);
        weapon.firstPersonPitchOffset = j.value(
            "weapon_fp_pitch_offset", weapon.firstPersonPitchOffset);
        if (j.contains("weapon_left_hand_grip"))
            weapon.leftHandGrip = readVec3Array(
                j["weapon_left_hand_grip"], weapon.leftHandGrip);
        if (j.contains("weapon_muzzle_position"))
            weapon.muzzlePosition = readVec3Array(
                j["weapon_muzzle_position"], weapon.muzzlePosition);
        weapon.showWorldVisual = j.value(
            "weapon_show_world_visual", weapon.showWorldVisual);
        weapon.showFirstPersonVisual = j.value(
            "weapon_show_fp_visual", weapon.showFirstPersonVisual);
        weapon.enableLeftHandIK = j.value(
            "weapon_enable_left_hand_ik", weapon.enableLeftHandIK);
        m_registry.AddComponent<WeaponComponent>(e, weapon);
    }

    if (j.value("has_weapon_owner", false))
    {
        WeaponOwnerComponent owner;
        owner.owner = static_cast<Entity>(
            j.value("weapon_owner", static_cast<uint32_t>(INVALID_ENTITY)));
        m_registry.AddComponent<WeaponOwnerComponent>(e, owner);
    }

    if (j.value("has_character_movement", false))
    {
        CharacterMovement movement;
        if (j.contains("movement_walk_direction"))
            movement.walkDirection = readVec3Array(j["movement_walk_direction"], movement.walkDirection);
        movement.moveSpeed = j.value("movement_move_speed", movement.moveSpeed);
        movement.runSpeed = j.value("movement_run_speed", movement.runSpeed);
        movement.rotationSpeed = j.value("movement_rotation_speed", movement.rotationSpeed);
        movement.currentSpeed = j.value("movement_current_speed", movement.currentSpeed);
        m_registry.AddComponent<CharacterMovement>(e, movement);
    }

    if (j.value("has_character_controller", false))
    {
        CharacterController controller;
        controller.capsuleRadius = j.value("controller_capsule_radius", controller.capsuleRadius);
        controller.capsuleHeight = j.value("controller_capsule_height", controller.capsuleHeight);
        controller.capsuleCenterOffsetY = j.value(
            "controller_capsule_center_offset_y",
            controller.capsuleHeight * 0.5f + controller.capsuleRadius);
        controller.stepHeight = j.value("controller_step_height", controller.stepHeight);
        controller.jumpSpeed = j.value("controller_jump_speed", controller.jumpSpeed);
        controller.gravity = j.value("controller_gravity", controller.gravity);
        controller.maxSlope = j.value("controller_max_slope", controller.maxSlope);
        m_registry.AddComponent<CharacterController>(e, std::move(controller));

        // The tuning block is optional for backward compatibility. Old scenes
        // receive documented safe defaults and are upgraded on the next save.
        CharacterPhysicsTuningComponent tuning;
        tuning.autoCalculateCapsuleCenterOffset = j.value(
            "character_auto_center_offset", tuning.autoCalculateCapsuleCenterOffset);
        tuning.manualCapsuleCenterOffsetY = j.value(
            "character_manual_center_offset_y", tuning.manualCapsuleCenterOffsetY);
        tuning.capsuleFootClearance = j.value(
            "character_foot_clearance", tuning.capsuleFootClearance);
        tuning.forceYAxis = j.value("character_force_y_axis", tuning.forceYAxis);
        tuning.useGhostSweepTest = j.value("character_use_ghost_sweep", tuning.useGhostSweepTest);
        tuning.maxPenetrationDepth = j.value(
            "character_max_penetration_depth", tuning.maxPenetrationDepth);
        tuning.fallSpeed = j.value("character_fall_speed", tuning.fallSpeed);
        tuning.clampNormalStepHeight = j.value("character_clamp_step", tuning.clampNormalStepHeight);
        tuning.minimumStepHeight = j.value("character_minimum_step", tuning.minimumStepHeight);
        tuning.maximumStepHeight = j.value("character_maximum_step", tuning.maximumStepHeight);
        tuning.migrateLegacyStepHeight = j.value(
            "character_migrate_legacy_step", tuning.migrateLegacyStepHeight);
        tuning.legacyStepHeight = j.value("character_legacy_step", tuning.legacyStepHeight);
        tuning.legacyReplacementStepHeight = j.value(
            "character_legacy_step_replacement", tuning.legacyReplacementStepHeight);
        tuning.automaticStepEnabled = j.value(
            "character_auto_step_enabled", tuning.automaticStepEnabled);
        tuning.automaticStepPredictive = j.value(
            "character_auto_step_predictive", tuning.automaticStepPredictive);
        tuning.automaticStepHeight = j.value(
            "character_auto_step_height", tuning.automaticStepHeight);
        tuning.automaticStepDelay = j.value(
            "character_auto_step_delay", tuning.automaticStepDelay);
        tuning.automaticStepDuration = j.value(
            "character_auto_step_duration", tuning.automaticStepDuration);
        tuning.automaticStepCooldown = j.value(
            "character_auto_step_cooldown", tuning.automaticStepCooldown);
        tuning.automaticStepProgressDistance = j.value(
            "character_auto_step_progress_distance", tuning.automaticStepProgressDistance);
        tuning.automaticStepForwardProbe = j.value(
            "character_auto_step_forward_probe", tuning.automaticStepForwardProbe);
        tuning.automaticStepDownProbe = j.value(
            "character_auto_step_down_probe", tuning.automaticStepDownProbe);
        tuning.automaticStepSkin = j.value(
            "character_auto_step_skin", tuning.automaticStepSkin);
        m_registry.AddComponent<CharacterPhysicsTuningComponent>(e, tuning);
    }

    if (j.value("has_npc", false))
    {
        NPCComponent npc;
        npc.name = j.value("npc_name", npc.name);
        npc.description = j.value("npc_description", npc.description);
        npc.maxHealth = j.value("npc_max_health", npc.maxHealth);
        npc.health = std::clamp(j.value("npc_health", npc.health), 0.0f, npc.maxHealth);
        npc.isHostile = j.value("npc_hostile", npc.isHostile);
        m_registry.AddComponent<NPCComponent>(e, npc);

        NPCMovementComponent npcMovement;
        npcMovement.speed = j.value("npc_speed", npcMovement.speed);
        npcMovement.runSpeed = j.value("npc_run_speed", npcMovement.runSpeed);
        npcMovement.runWhenFartherThan = j.value("npc_run_distance", npcMovement.runWhenFartherThan);
        npcMovement.arrivalDistance = j.value("npc_arrival_distance", npcMovement.arrivalDistance);
        npcMovement.doorOpenDistance = j.value("npc_door_open_distance", npcMovement.doorOpenDistance);
        npcMovement.doorWaitDistance = j.value("npc_door_wait_distance", npcMovement.doorWaitDistance);
        npcMovement.allowRunning = j.value("npc_allow_running", npcMovement.allowRunning);
        npcMovement.canFly = j.value("npc_can_fly", npcMovement.canFly);
        m_registry.AddComponent<NPCMovementComponent>(e, npcMovement);

        NpcBrainComponent brain;
        brain.detectionRange = j.value("npc_detection_range", brain.detectionRange);
        brain.repairDistance = j.value("npc_repair_distance", brain.repairDistance);
        brain.repairDuration = j.value("npc_repair_duration", brain.repairDuration);
        brain.repathInterval = j.value("npc_repath_interval", brain.repathInterval);
        brain.stuckTimeout = j.value("npc_stuck_timeout", brain.stuckTimeout);
        brain.wanderRadius = j.value("npc_wander_radius", brain.wanderRadius);
        m_registry.AddComponent<NpcBrainComponent>(e, brain);

        if (!m_registry.HasComponent<CharacterMovement>(e))
            m_registry.AddComponent<CharacterMovement>(e, CharacterMovement{});
        if (!m_registry.HasComponent<CharacterController>(e))
            m_registry.AddComponent<CharacterController>(e, CharacterController{});
        if (!m_registry.HasComponent<CharacterPhysicsTuningComponent>(e))
            m_registry.AddComponent<CharacterPhysicsTuningComponent>(
                e, CharacterPhysicsTuningComponent{});
    }

    return e;
}

json SceneManager::SerializeMaterial(const MaterialClass* mat) { return json(); }

// ============================================================================
// GetOrCreateModel вЂ” РєСЌС€ РјРѕРґРµР»РµР№
// ============================================================================
AssimpModelClass* SceneManager::GetOrCreateModel(
    const std::string& path,
    ID3D11Device* device, ID3D11DeviceContext* deviceContext,
    const std::string& texturePath,
    bool allowFallback)
{
    if (path.empty()) return allowFallback ? m_mainModelFallback : nullptr;

    auto it = m_modelCache.find(path);
    if (it != m_modelCache.end() && it->second) return it->second;

    AssimpModelClass* model = new AssimpModelClass();

    if (GetFileAttributesA(path.c_str()) == INVALID_FILE_ATTRIBUTES)
    {
        OutputDebugStringA(("GetOrCreateModel: not found: " + path + "\n").c_str());
        delete model;
        return allowFallback ? m_mainModelFallback : nullptr;
    }

    // Р”Р»СЏ GLB texturePath Р±СѓРґРµС‚ РїСѓСЃС‚С‹Рј вЂ” РїРµСЂРµРґР°С‘Рј nullptr,
    // С‡С‚РѕР±С‹ AssimpModelClass РЅРµ РіСЂСѓР·РёР» fallback С‚РµРєСЃС‚СѓСЂСѓ РїРѕРІРµСЂС… РІС€РёС‚С‹С…
    const std::string ext = fs::path(path).extension().string();
    const bool isGLB = (ext == ".glb" || ext == ".gltf");
    const char* texPath = (!isGLB && !texturePath.empty()) ? texturePath.c_str() : nullptr;

    if (model->Initialize(device, deviceContext, path.c_str(), texPath))
    {
        m_modelCache[path] = model;
        return model;
    }

    delete model;
    return allowFallback ? m_mainModelFallback : nullptr;
}


void SceneManager::SetObjectShader(Entity entity, ShaderBaseClass* shader, ID3D11Device* device)
{
    auto* mesh = m_registry.GetComponents<MeshRendererComponent>().Get(entity);
    if (!mesh) return;

    // ?? FIX: Р•СЃР»Рё РѕР±СЉРµРєС‚ РёСЃРїРѕР»СЊР·СѓРµС‚ РѕР±С‰РёР№ defaultMaterial вЂ” СЃРѕР·РґР°С‘Рј РµРјСѓ Р»РёС‡РЅС‹Р№ РєРѕРїРёСЋ
    if (!mesh->material || mesh->material == m_defaultMaterial)
    {
        if (!device) return;

        auto* newMat = new MaterialClass();
        newMat->Initialize(device, L"RuntimeMaterial_Custom");

        // РљРѕРїРёСЂСѓРµРј С‚РµРєСЃС‚СѓСЂС‹ РёР· РґРµС„РѕР»С‚РЅРѕРіРѕ РјР°С‚РµСЂРёР°Р»Р°
        if (m_defaultMaterial)
        {
            newMat->SetMainTexture(m_defaultMaterial->GetMainTexture());
            // Р•СЃР»Рё РµСЃС‚СЊ РґСЂСѓРіРёРµ СЃР»РѕС‚С‹ вЂ” СЃРєРѕРїРёСЂСѓР№ РёС… С‚РѕР¶Рµ:
            // newMat->SetTexture(1, m_defaultMaterial->GetTexture(1));
        }

        mesh->material = newMat;  // РўРµРїРµСЂСЊ Сѓ РѕР±СЉРµРєС‚Р° СЃРІРѕР№ РјР°С‚РµСЂРёР°Р»!
    }

    // РўРµРїРµСЂСЊ Р±РµР·РѕРїР°СЃРЅРѕ РјРµРЅСЏРµРј С€РµР№РґРµСЂ вЂ” С‚РѕР»СЊРєРѕ Сѓ СЌС‚РѕРіРѕ РѕР±СЉРµРєС‚Р°
    if (mesh->material && shader)
    {
        mesh->material->SetShader(shader);

        // Glass-С€РµР№РґРµСЂ С‚СЂРµР±СѓРµС‚ refractionSRV + normalTexture (slot 1)
        if (shader->GetType() == ShaderType::Light || shader->GetType() == ShaderType::Glass)
        {
            if (!mesh->material->GetMainTexture() && m_defaultMaterial && m_defaultMaterial->GetMainTexture())
            {
                mesh->material->SetMainTexture(m_defaultMaterial->GetMainTexture());
            }
        }
    }
}
void SceneManager::SetObjectTexture(Entity entity, int slot,
    const std::string& path,
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    bool forceSRGB)
{
    auto* mesh = m_registry.GetComponents<MeshRendererComponent>().Get(entity);
    if (!mesh) return;
    if (!device || !context) return;

    // 1. РЎРѕР·РґР°С‘Рј РјР°С‚РµСЂРёР°Р», РµСЃР»Рё РЅРµС‚
    if (!mesh->material)
    {
        auto* mat = new MaterialClass();
        mat->Initialize(device, L"RuntimeMaterial");
        if (m_defaultMaterial && m_defaultMaterial->GetShader())
            mat->SetShader(m_defaultMaterial->GetShader());
        mesh->material = mat;
    }

    // РџРѕР·РІРѕР»СЏРµРј РѕС‡РёС‰Р°С‚СЊ СЃР»РѕС‚ РїСѓСЃС‚С‹Рј РїСѓС‚С‘Рј
    if (path.empty())
    {
        mesh->material->SetTexture(slot, nullptr);
        mesh->material->SetTexturePath(slot, "");
        return;
    }

    // РџСЂРѕРІРµСЂРєР° РЅР°Р»РёС‡РёСЏ С„Р°Р№Р»Р°
    if (GetFileAttributesA(path.c_str()) == INVALID_FILE_ATTRIBUTES)
    {
        OutputDebugStringA(("SetObjectTexture: File not found: " + path + "\n").c_str());
        return;
    }

    // 2. Р—Р°РіСЂСѓР¶Р°РµРј С‚РµРєСЃС‚СѓСЂСѓ (РёСЃРїРѕР»СЊР·СѓРµРј TextureClass, РєР°Рє РІ SpawnObjectFromRequest)
    auto loadedTexture = std::make_unique<TextureClass>();
    if (!loadedTexture->Initialize(device, context, path.c_str(), forceSRGB))
    {
        OutputDebugStringA(("SetObjectTexture: TextureClass::Initialize FAILED for: " + path + "\n").c_str());
        return;
    }

    // 3. Assign the runtime-owned texture to the material.
    mesh->material->AdoptTexture(slot, std::move(loadedTexture));
    mesh->material->SetTexturePath(slot, path);

    // 4. Р•СЃР»Рё СЌС‚Рѕ РіР»Р°РІРЅР°СЏ С‚РµРєСЃС‚СѓСЂР° (slot 0) Рё С€РµР№РґРµСЂ РµС‰С‘ РЅРµ РЅР°Р·РЅР°С‡РµРЅ вЂ” РјРѕР¶РЅРѕ РїРѕРґС‚СЏРЅСѓС‚СЊ РґРµС„РѕР»С‚РЅС‹Р№ С€РµР№РґРµСЂ
    if (slot == 0 && !mesh->material->GetShader() && m_defaultMaterial)
    {
        mesh->material->SetShader(m_defaultMaterial->GetShader());
    }
}




// ============================================================================
// SpawnLight вЂ” СЃРѕР·РґР°С‚СЊ РёСЃС‚РѕС‡РЅРёРє СЃРІРµС‚Р° РєР°Рє ECS-СЃСѓС‰РЅРѕСЃС‚СЊ
// ============================================================================
Entity SceneManager::SpawnLight(const XMFLOAT3& position, const XMFLOAT4& color, bool isGizmo)
{
    AssimpModelClass* model = m_lightModel ? m_lightModel : m_mainModelFallback;
    if (!model) return INVALID_ENTITY;

    Entity e = SpawnObject(model, position.x, position.y, position.z);
    if (e == INVALID_ENTITY) return INVALID_ENTITY;

    // РњР°СЃС€С‚Р°Р±
    if (auto* t = m_registry.GetComponent<TransformComponent>(e))
    {
        float s = isGizmo ? 0.2f : 0.3f;
        t->scale = { s, s, s };
    }

    // ? РќРћР’РћР•: РЎРѕР·РґР°С‘Рј РјР°С‚РµСЂРёР°Р» СЃ ColorShader, С‡С‚РѕР±С‹ СЃС„РµСЂР° СЃРІРµС‚РёР»Р°СЃСЊ
    if (auto* mesh = m_registry.GetComponent<MeshRendererComponent>(e))
    {
        // Р—РґРµСЃСЊ РЅСѓР¶РЅРѕ РїРµСЂРµРґР°С‚СЊ device РґР»СЏ СЃРѕР·РґР°РЅРёСЏ РјР°С‚РµСЂРёР°Р»Р°
        // РР»Рё СѓСЃС‚Р°РЅРѕРІРёС‚СЊ СѓР¶Рµ СЃСѓС‰РµСЃС‚РІСѓСЋС‰РёР№ РјР°С‚РµСЂРёР°Р» СЃ ColorShader
    }

    // Р”РѕР±Р°РІР»СЏРµРј LightComponent
    LightComponent light;
    light.color = color;
    light.enabled = true;
    light.isEditorGizmo = isGizmo;
    light.radius = 10.0f;
    m_registry.AddComponent<LightComponent>(e, light);

    return e;
}



// ============================================================================
// RemoveLight вЂ” СѓРґР°Р»РёС‚СЊ РёСЃС‚РѕС‡РЅРёРє СЃРІРµС‚Р°
// ============================================================================
bool SceneManager::RemoveLight(Entity entity)
{
    if (!m_registry.HasComponent<LightComponent>(entity))
        return false;

    // Р”РµР°РєС‚РёРІРёСЂСѓРµРј вЂ” RenderSystem Рё GatherLights РїСЂРѕРїСѓСЃС‚СЏС‚
    m_registry.RemoveComponent<ActiveComponent>(entity);
    return true;
}

// ============================================================================
// РҐРµР»РїРµСЂС‹ РґР»СЏ СЂРµРґР°РєС‚РѕСЂР°
// ============================================================================
void SceneManager::SetLightColor(Entity entity, const XMFLOAT4& color)
{
    if (auto* light = m_registry.GetComponent<LightComponent>(entity))
        light->color = color;
}

bool SceneManager::SetObjectActive(Entity entity, bool active)
{
    const bool isSceneObject =
        m_registry.HasComponent<TransformComponent>(entity) ||
        m_registry.HasComponent<WorldAreaComponent>(entity);

    if (!isSceneObject)
    {
        return false;
    }

    bool isActive = m_registry.HasComponent<ActiveComponent>(entity);

    if (active && !isActive)
    {
        m_registry.AddComponent<ActiveComponent>(entity, ActiveComponent{});
    }
    else if (!active && isActive)
    {
        m_registry.RemoveComponent<ActiveComponent>(entity);
    }

    return true;
}

void SceneManager::SetLightPosition(Entity entity, const XMFLOAT3& position)
{
    if (auto* transform = m_registry.GetComponent<TransformComponent>(entity))
        transform->position = position;
}

void SceneManager::SetLightEnabled(Entity entity, bool enabled)
{
    if (auto* light = m_registry.GetComponent<LightComponent>(entity))
        light->enabled = enabled;
}

void SceneManager::AttachLightToObject(Entity lightEntity, Entity targetEntity, const XMFLOAT3& offset)
{
    auto* light = m_registry.GetComponent<LightComponent>(lightEntity);
    if (!light) return;

    light->attachedTo = targetEntity;
    light->offset = offset;
    // РџРѕР·РёС†РёСЏ РѕР±РЅРѕРІРёС‚СЃСЏ РІ UpdateLights()
}

void SceneManager::DetachLight(Entity lightEntity)
{
    auto* light = m_registry.GetComponent<LightComponent>(lightEntity);
    if (light)
    {
        light->attachedTo = INVALID_ENTITY;
        light->offset = { 0,0,0 };
    }
}

// ============================================================================
// GatherLightsForShader вЂ” СЃРѕР±СЂР°С‚СЊ РґР°РЅРЅС‹Рµ РґР»СЏ РїРµСЂРµРґР°С‡Рё РІ С€РµР№РґРµСЂ
// Р’С‹Р·С‹РІР°РµС‚СЃСЏ РїРµСЂРµРґ RenderSystem::Render
// ============================================================================
void SceneManager::GatherLightsForShader(
    XMFLOAT4 outColors[],
    XMFLOAT4 outPositions[],
    XMFLOAT4 outDirectionsAndOuterCos[],
    XMFLOAT4 outInnerCosAndFlags[],
    int maxLights)
{
    // РћР±РЅСѓР»СЏРµРј СЃР»РѕС‚С‹
    for (int i = 0; i < maxLights; ++i)
    {
        outColors[i] = XMFLOAT4(0, 0, 0, 0);
        outPositions[i] = XMFLOAT4(0, 0, 0, 0);
        outDirectionsAndOuterCos[i] = XMFLOAT4(0, 0, -1, 0);
        outInnerCosAndFlags[i] = XMFLOAT4(0, 0, 0, 0);
    }

    int slot = 0;
    auto& lights = m_registry.GetComponents<LightComponent>();
    auto& transforms = m_registry.GetComponents<TransformComponent>();

    for (auto& [entity, light] : lights)
    {
        if (slot >= maxLights) break;
        if (!light.enabled) continue;

        // РџСЂРѕРїСѓСЃРєР°РµРј СЂРµРґР°РєС‚РѕСЂРЅС‹Рµ РіРёР·РјРѕ РІ РёРіСЂРѕРІРѕРј СЂРµРЅРґРµСЂРµ
        if (light.isEditorGizmo) continue;

        if (!m_registry.HasComponent<ActiveComponent>(entity)) continue;

        auto* transform = transforms.Get(entity);
        if (!transform) continue;

        // пїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅ пїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅ.
        // пїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅ пїЅ пїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅ пїЅпїЅ пїЅпїЅпїЅпїЅпїЅпїЅпїЅпїЅ.
        const TransformComponent* sourceTransform = transform;
        if (light.attachedTo != INVALID_ENTITY)
        {
            auto* targetTransform = transforms.Get(light.attachedTo);
            if (targetTransform)
                sourceTransform = targetTransform;
        }

        const XMMATRIX sourceWorld = sourceTransform->GetWorldMatrix();
        XMFLOAT3 worldPos{};
        XMStoreFloat3(&worldPos,
            XMVector3TransformCoord(XMLoadFloat3(&light.offset), sourceWorld));

        outColors[slot] = light.color;
        outPositions[slot] = XMFLOAT4(
            worldPos.x,
            worldPos.y,
            worldPos.z,
            (std::max)(light.radius, 0.01f));

        if (const auto* spot = m_registry.GetComponent<SpotLightComponent>(entity))
        {
            // A direction is affected by orientation only. Applying world scale
            // here used to skew it and negative scale could flip the cone.
            const XMMATRIX sourceRotation =
                XMMatrixRotationX(sourceTransform->rotation.x) *
                XMMatrixRotationY(sourceTransform->rotation.y) *
                XMMatrixRotationZ(sourceTransform->rotation.z);
            XMVECTOR localDirection = XMLoadFloat3(&spot->localDirection);
            if (XMVectorGetX(XMVector3LengthSq(localDirection)) < 0.000001f)
                localDirection = XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f);
            XMVECTOR direction = XMVector3Normalize(
                XMVector3TransformNormal(localDirection, sourceRotation));
            XMFLOAT3 worldDirection{};
            XMStoreFloat3(&worldDirection, direction);

            const float outerDegrees = std::clamp(spot->outerAngleDegrees, 1.0f, 89.0f);
            const float innerDegrees = std::clamp(spot->innerAngleDegrees, 0.1f, outerDegrees);
            outDirectionsAndOuterCos[slot] = XMFLOAT4(
                worldDirection.x, worldDirection.y, worldDirection.z,
                std::cos(XMConvertToRadians(outerDegrees)));
            outInnerCosAndFlags[slot] = XMFLOAT4(
                std::cos(XMConvertToRadians(innerDegrees)), 1.0f, 0.0f, 0.0f);
        }
        ++slot;
    }
}

// ============================================================================
// UpdateLights вЂ” РѕР±РЅРѕРІРёС‚СЊ РїРѕР·РёС†РёРё РїСЂРёРІСЏР·Р°РЅРЅС‹С… РёСЃС‚РѕС‡РЅРёРєРѕРІ
// Р’С‹Р·С‹РІР°РµС‚СЃСЏ РєР°Р¶РґС‹Р№ РєР°РґСЂ РїРµСЂРµРґ СЂРµРЅРґРµСЂРѕРј
// ============================================================================
void SceneManager::UpdateLights()
{
    auto& lights = m_registry.GetComponents<LightComponent>();
    auto& transforms = m_registry.GetComponents<TransformComponent>();

    for (auto& [entity, light] : lights)
    {
        if (!light.enabled || light.attachedTo == INVALID_ENTITY) continue;

        auto* lightTransform = transforms.Get(entity);
        auto* targetTransform = transforms.Get(light.attachedTo);

        if (lightTransform && targetTransform)
        {
            // Offset is local to the attachment, exactly like it is interpreted
            // by GatherLightsForShader. The previous world-space addition made
            // attached lights drift when their parent rotated or scaled.
            XMStoreFloat3(
                &lightTransform->position,
                XMVector3TransformCoord(
                    XMLoadFloat3(&light.offset),
                    targetTransform->GetWorldMatrix()));
        }
    }
}

void SceneManager::RemapEntityReferences(
    const std::unordered_map<uint32_t, Entity>& remap)
{
    auto remapEntity = [&](Entity old) -> Entity
        {
            auto it = remap.find(static_cast<uint32_t>(old));
            if (it != remap.end()) return it->second;

            // ?? ID РЅРµ РЅР°Р№РґРµРЅ РІ С‚Р°Р±Р»РёС†Рµ вЂ” Р·РЅР°С‡РёС‚ РѕР±СЉРµРєС‚ РЅРµ Р±С‹Р» СЃРѕС…СЂР°РЅС‘РЅ
            char buf[128];
            sprintf_s(buf, "[Remap] keeping current entity %u; not found in saved remap table\n",
                static_cast<uint32_t>(old));
            OutputDebugStringA(buf);
            return old;
        };

    // -- SubmarineComponent ----------------------------------------------------
    auto& subPool = m_registry.GetComponents<SubmarineComponent>();
    for (auto& [entity, sub] : subPool)
    {
        sub.submarineRoot = remapEntity(sub.submarineRoot);

        for (Entity& c : sub.allCompartments)
        {
            c = remapEntity(c);
        }
    }

    // -- CompartmentComponent.connectedCompartments ----------------------------
    auto& compPool = m_registry.GetComponents<CompartmentComponent>();
    for (auto& [entity, comp] : compPool)
    {
        for (Entity& c : comp.connectedCompartments)
            c = remapEntity(c);
    }

    // -- RoomComponent.connectedRooms ------------------------------------------
    auto& roomPool = m_registry.GetComponents<RoomComponent>();
    for (auto& [entity, room] : roomPool)
    {
        for (Entity& connectedRoom : room.connectedRooms)
            connectedRoom = remapEntity(connectedRoom);
    }
    // -- DoorComponent room/compartment links ----------------------------------
    auto& doorPool = m_registry.GetComponents<DoorComponent>();
    for (auto& [entity, door] : doorPool)
    {
        if (door.compartmentA != INVALID_ENTITY)
            door.compartmentA = remapEntity(door.compartmentA);
        if (door.compartmentB != INVALID_ENTITY)
            door.compartmentB = remapEntity(door.compartmentB);
        if (door.roomA != INVALID_ENTITY)
            door.roomA = remapEntity(door.roomA);
        if (door.roomB != INVALID_ENTITY)
            door.roomB = remapEntity(door.roomB);
        if (door.leftLeaf != INVALID_ENTITY)
            door.leftLeaf = remapEntity(door.leftLeaf);
        if (door.rightLeaf != INVALID_ENTITY)
            door.rightLeaf = remapEntity(door.rightLeaf);
    }
    // -- LightComponent.attachedTo ---------------------------------------------
    auto& lightPool = m_registry.GetComponents<LightComponent>();
    for (auto& [entity, light] : lightPool)
    {
        if (light.attachedTo != INVALID_ENTITY)
            light.attachedTo = remapEntity(light.attachedTo);
    }

    auto& equippedPool = m_registry.GetComponents<EquippedWeaponComponent>();
    for (auto& [entity, equipped] : equippedPool)
    {
        if (equipped.weapon != INVALID_ENTITY)
            equipped.weapon = remapEntity(equipped.weapon);
    }

    auto& weaponOwnerPool = m_registry.GetComponents<WeaponOwnerComponent>();
    for (auto& [entity, owner] : weaponOwnerPool)
    {
        if (owner.owner != INVALID_ENTITY)
            owner.owner = remapEntity(owner.owner);
    }

    auto& firstPersonPool = m_registry.GetComponents<FirstPersonViewComponent>();
    for (auto& [entity, firstPerson] : firstPersonPool)
    {
        if (firstPerson.owner != INVALID_ENTITY)
            firstPerson.owner = remapEntity(firstPerson.owner);
    }

    auto& attachmentPool = m_registry.GetComponents<BoneAttachmentComponent>();
    for (auto& [entity, attachment] : attachmentPool)
    {
        if (attachment.parent != INVALID_ENTITY)
            attachment.parent = remapEntity(attachment.parent);
    }

    // -- SubmarineJointComponent -----------------------------------------------
    auto& jointPool = m_registry.GetComponents<SubmarineJointComponent>();
    for (auto& [entity, joint] : jointPool)
    {
        joint.compartmentA = remapEntity(joint.compartmentA);
        joint.compartmentB = remapEntity(joint.compartmentB);
    }

    // -- JointConnectionComponent ----------------------------------------------
    auto& connPool = m_registry.GetComponents<JointConnectionComponent>();
    for (auto& [entity, conn] : connPool)
    {
        if (conn.targetEntity != INVALID_ENTITY)
            conn.targetEntity = remapEntity(conn.targetEntity);
    }

    auto& zonePool = m_registry.GetComponents<WorkZoneComponent>();
    for (auto& [entity, zone] : zonePool)
    {
        if (zone.owner != INVALID_ENTITY)
            zone.owner = remapEntity(zone.owner);
    }
}





void SceneManager::ResolveRepairZones()
{
    auto normalizeRepairKey = [](std::string value)
    {
        value = StripBlenderNumericSuffix(ToUpper(value));

        std::string key;
        key.reserve(value.size());
        for (unsigned char c : value)
        {
            if (std::isalnum(c))
                key.push_back(static_cast<char>(c));
        }
        return key;
    };

    auto payloadMatchesOwner = [&](
        const std::string& payload,
        Entity owner,
        const std::string& ownerName)
    {
        const std::string payloadUpper = StripBlenderNumericSuffix(ToUpper(payload));
        const std::string payloadKey = normalizeRepairKey(payloadUpper);
        const std::string ownerKey = normalizeRepairKey(ownerName);

        const bool wantsOxygen =
            payloadUpper.find("OXYGEN") != std::string::npos;

        const bool wantsGenericGenerator =
            !wantsOxygen &&
            (payloadUpper == "GENERATOR" ||
                StartsWith(payloadUpper, "GENERATOR_"));

        if (wantsOxygen &&
            !m_registry.HasComponent<OxygenGeneratorComponent>(owner))
        {
            return false;
        }

        if (wantsGenericGenerator &&
            !m_registry.HasComponent<GeneratorComponent>(owner) &&
            !m_registry.HasComponent<OxygenGeneratorComponent>(owner))
        {
            return false;
        }

        std::vector<std::string> candidates;
        candidates.push_back(payloadKey);

        if (!StartsWith(payloadUpper, "SHIP_"))
            candidates.push_back(normalizeRepairKey("SHIP_" + payloadUpper));

        if (payloadUpper == "OXYGEN" ||
            payloadUpper == "OXYGEN_GENERATOR")
        {
            candidates.push_back(normalizeRepairKey("SHIP_OXYGEN_GENERATOR"));
        }

        for (const std::string& candidate : candidates)
        {
            if (!candidate.empty() &&
                (ownerKey == candidate ||
                    ownerKey.find(candidate) != std::string::npos ||
                    candidate.find(ownerKey) != std::string::npos))
            {
                return true;
            }
        }

        return false;
    };

    for (auto& [zoneEntity, zone] :
        m_registry.GetComponents<WorkZoneComponent>())
    {
        const auto* zoneName =
            m_registry.GetComponent<NameComponent>(zoneEntity);

        if (!zoneName)
            continue;

        const std::string upperZoneName =
            ToUpper(zoneName->name);

        constexpr const char* prefix =
            "REPAIR_ZONE_";

        if (!StartsWith(
            upperZoneName,
            prefix))
        {
            continue;
        }

        const std::string payload =
            upperZoneName.substr(
                std::strlen(prefix));

        Entity foundOwner = INVALID_ENTITY;
        float bestDistanceSq = -1.0f;
        const auto* zoneTransform =
            m_registry.GetComponent<TransformComponent>(zoneEntity);


        for (auto& [entity, name] :
            m_registry.GetComponents<NameComponent>())
        {
            if (entity == zoneEntity)
                continue;

            const std::string upperName =
                ToUpper(name.name);

            if (!payloadMatchesOwner(payload, entity, upperName))
            {
                continue;
            }

            float distanceSq = 0.0f;
            if (zoneTransform)
            {
                if (const auto* ownerTransform =
                    m_registry.GetComponent<TransformComponent>(entity))
                {
                    const float dx =
                        ownerTransform->position.x -
                        zoneTransform->position.x;
                    const float dy =
                        ownerTransform->position.y -
                        zoneTransform->position.y;
                    const float dz =
                        ownerTransform->position.z -
                        zoneTransform->position.z;
                    distanceSq = dx * dx + dy * dy + dz * dz;
                }
            }

            if (foundOwner == INVALID_ENTITY ||
                distanceSq < bestDistanceSq)
            {
                foundOwner = entity;
                bestDistanceSq = distanceSq;
            }
        }


        if (foundOwner != INVALID_ENTITY)
        {
            zone.owner =
                foundOwner;

#ifdef _DEBUG

            char buffer[256]{};

            sprintf_s(
                buffer,
                "[RepairZone] %s -> owner=%u\n",
                zoneName->name.c_str(),
                static_cast<unsigned>(foundOwner));

            OutputDebugStringA(buffer);

#endif
        }
        else
        {
#ifdef _DEBUG

            char buffer[256]{};

            sprintf_s(
                buffer,
                "[RepairZone] OWNER NOT FOUND for %s\n",
                zoneName->name.c_str());

            OutputDebugStringA(buffer);

#endif
        }
    }
}
