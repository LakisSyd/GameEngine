// ============================================================================
// SceneManager.cpp
//
// НЕДЕЛЯ 4: GetObject(int) и m_legacyObjects удалены.
//           EditorUI и PickingSystem работают через Entity + ECS.
// ============================================================================

#include "SceneManager.h"
#include "FileSystem.h"
#include "ShaderManager.h"
// ============================================================================
// Конструктор
// ============================================================================
// ✅ СТАЛО:
SceneManager::SceneManager()
    : m_defaultMaterial(nullptr)
    , m_mainModelFallback(nullptr)
{
    // Инициализация света больше не нужна — света создаются через SpawnLight()
}

SceneManager::~SceneManager()
{
    Clear();
}

// ============================================================================
// SpawnObject — создать объект в сцене
//
// НЕДЕЛЯ 3: создаёт ТОЛЬКО ECS-компоненты. Никакого GameObject в векторе.
// m_legacyObjects заполняется «на лету» в GetObject() при первом обращении.
//
// Возвращает Entity — используй его как «ключ» к объекту.
// ============================================================================
Entity SceneManager::SpawnObject(AssimpModelClass* model, float x, float y, float z)
{
    Entity e = m_registry.CreateEntity();

    // ── Компоненты объекта в ECS ──────────────────────────────────────────────
    m_registry.AddComponent<TransformComponent>(e,
        TransformComponent{ {x, y, z}, {0, 0, 0}, {1, 1, 1} });

    m_registry.AddComponent<MeshRendererComponent>(e,
        MeshRendererComponent{ model, nullptr });

    // ActiveComponent — тег: объект существует и должен рендериться
    // RemoveObject() удалит этот компонент → RenderSystem его пропустит
    m_registry.AddComponent<ActiveComponent>(e, {});

    return e;
}

// ============================================================================
// RemoveObject — деактивировать и полностью удалить сущность
// НЕДЕЛЯ 4: теперь вызываем DestroyEntity — EditorUI хранит Entity, не int index.
// ============================================================================
bool SceneManager::RemoveObject(Entity entity)
{
    if (!m_registry.HasComponent<TransformComponent>(entity))
        return false; // сущность не существует

    // Убираем тег активности — RenderSystem пропустит эту сущность
    m_registry.RemoveComponent<ActiveComponent>(entity);

    return true;
}

// ============================================================================
// Clear — очистить всю сцену
// ============================================================================
void SceneManager::Clear()
{
    // Неделя 3: очищаем ECS реестр через пересоздание
    // (полный сброс — все Entity и все компоненты)
    m_registry = ECSRegistry{};
}

// ============================================================================
// UpdateAll — обновить сцену за кадр
//
// НЕДЕЛЯ 3: TransformSystem УДАЛЁН.
// Данные трансформации живут ТОЛЬКО в TransformComponent.
// TransformComponent::GetWorldMatrix() используется напрямую в RenderSystem.
//
// Здесь можно добавить другие системы: физику, анимации, скрипты.
// ============================================================================
void SceneManager::UpdateAll(float deltaTime)
{
    // Неделя 4: здесь будет UpdateSystem::Update(m_registry, deltaTime)
    // Пока — пусто, т.к. у нас нет логики обновления компонентов
    (void)deltaTime;
}

// ============================================================================
// RenderAllECS — рендер всех активных объектов через RenderSystem
//
// НЕДЕЛЯ 3: заменяет старый RenderAll() через m_objects.
//
// RenderSystem::Render итерирует ECS:
//   1. Берёт все MeshRendererComponent
//   2. Проверяет ActiveComponent (тег)
//   3. Берёт TransformComponent → GetWorldMatrix()
//   4. Вызывает RenderMesh()
//
// Свет копируется из m_lights в переданные массивы ЗДЕСЬ — до вызова RenderSystem.
// ============================================================================
void SceneManager::RenderAllECS(
    ID3D11DeviceContext* context,
    const XMMATRIX& viewMatrix,
    const XMMATRIX& projectionMatrix,
    MaterialClass* defaultMaterial,
    XMFLOAT4 lightColors[],
    XMFLOAT4 lightPositions[],
    const Settings::FogSettings& fog,
    Entity selectedEntity,
    XMFLOAT3 cameraPos)
{
    // Собираем света из ECS
    GatherLightsForShader(lightColors, lightPositions, NUM_LIGHTS);

    // ✅ FIX: Рендерим объекты, пропуская редакторные гизмо
    auto& meshPool = m_registry.GetComponents<MeshRendererComponent>();

    for (auto& [entity, mesh] : meshPool)
    {
        if (!m_registry.HasComponent<ActiveComponent>(entity)) continue;

        // 🔥 НОВОЕ: Пропускаем редакторные гизмо источников света
        auto* light = m_registry.GetComponent<LightComponent>(entity);
        if (light && light->isEditorGizmo) continue;

        auto* transform = m_registry.GetComponent<TransformComponent>(entity);
        if (!transform || !mesh.model) continue;

        auto* mat = mesh.material ? mesh.material : defaultMaterial;
        if (!mat) continue;

        XMMATRIX world = transform->GetWorldMatrix();
        RenderMesh(context, mesh.model, world, viewMatrix, projectionMatrix,
            mat, lightColors, lightPositions, fog,
            entity == selectedEntity, cameraPos);
    }
}

// ============================================================================
// RenderAllECSExcluding — рендер всех объектов кроме одного
//
// НЕДЕЛЯ 3: заменяет Render3DObjectsExcluding() в ApplicationClass.
// Используется для reflection pass: пластина-зеркало исключается
// чтобы она не отражала саму себя (рекурсия).
/// TODO: проверить работоспособность с новой архитектурой
// ============================================================================
void SceneManager::RenderAllECSExcluding(
    ID3D11DeviceContext* context,
    const XMMATRIX& viewMatrix,
    const XMMATRIX& projectionMatrix,
    MaterialClass* defaultMaterial,
    XMFLOAT4             lightColors[],
    XMFLOAT4             lightPositions[],
    const Settings::FogSettings& fog,
    Entity               excludeEntity)
{
   
        // 🔥 НОВОЕ: Собираем света из ECS (как в RenderAllECS)
    GatherLightsForShader(lightColors, lightPositions, NUM_LIGHTS);

    // Итерируем вручную чтобы пропустить excludeEntity
    auto& meshPool = m_registry.GetComponents<MeshRendererComponent>();

    for (auto& [entity, mesh] : meshPool)
    {
        // Пропускаем исключённую сущность (reflection объект)
        if (entity == excludeEntity) continue;

        // Пропускаем неактивные
        if (!m_registry.HasComponent<ActiveComponent>(entity)) continue;

        auto* transform = m_registry.GetComponent<TransformComponent>(entity);
        if (!transform || !mesh.model) continue;

        auto* mat = mesh.material ? mesh.material : defaultMaterial;
        if (!mat) continue;

        XMMATRIX world = transform->GetWorldMatrix();
        RenderMesh(context, mesh.model, world, viewMatrix, projectionMatrix,
            mat, lightColors, lightPositions, fog, false);
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
// Light-сферы — только визуальные индикаторы редактора, не часть ECS.
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



// ============================================================================
// SaveScene — сериализация
//
// НЕДЕЛЯ 3: читаем данные из ECS (TransformComponent, MeshRendererComponent)
// ============================================================================
bool SceneManager::SaveScene(const std::string& filename)
{
    json scene;
    scene["scene_name"] = "MyLevel";
    scene["version"] = 1;
    scene["objects"] = json::array();

    // Итерируем по всем сущностям с MeshRendererComponent
    auto& meshPool = m_registry.GetComponents<MeshRendererComponent>();
    int id = 0;
    for (auto& [entity, mesh] : meshPool)
    {
        // Сохраняем только активные объекты
        if (!m_registry.HasComponent<ActiveComponent>(entity)) continue;
        scene["objects"].push_back(SerializeObject(entity, id++));
    }

    std::ofstream file(filename);
    if (!file.is_open()) return false;
    file << scene.dump(4);
    return true;
}

// ============================================================================
// SerializeObject — упаковать ECS-сущность в JSON
//
// НЕДЕЛЯ 3: читает из TransformComponent, не из GameObject
// ============================================================================
json SceneManager::SerializeObject(Entity entity, int id)
{
    json objJson;
    objJson["id"] = id;

    // Читаем реальный путь модели из кэша, а не хардкод
    std::string modelPath = "assets/model/sphere.obj";
    std::string texturePath = "assets/texture/stone.tga";

    if (auto* mesh = m_registry.GetComponent<MeshRendererComponent>(entity))
    {
        // Ищем путь модели в кэше по указателю
        for (auto& [path, cachedModel] : m_modelCache)
        {
            if (cachedModel == mesh->model)
            {
                modelPath = path;
                break;
            }
        }
    }

    // В SerializeObject:
    if (auto* light = m_registry.GetComponent<LightComponent>(entity))
    {
        objJson["is_light"] = true;
        objJson["light_color"] = { light->color.x, light->color.y, light->color.z, light->color.w };
        objJson["light_enabled"] = light->enabled;
        
    }
    // ============================================================================
// StaticColliderComponent
// ============================================================================
    if (auto* collider = m_registry.GetComponent<StaticColliderComponent>(entity))
    {
        objJson["has_static_collider"] = true;
        // Сохраняем тип формы и размеры (аналогично RigidBody)
        // Примечание: btCollisionShape* не сериализуется, воссоздаём по типу/размерам
        objJson["collider_shape_type"] = "box"; // TODO: определить тип динамически, если нужно
        objJson["collider_size"] = { 1.0f, 1.0f, 1.0f }; // TODO: извлечь из shape, если возможно
    }

    // ============================================================================
    // BuoyancyComponent
    // ============================================================================
    if (auto* buoyancy = m_registry.GetComponent<BuoyancyComponent>(entity))
    {
        objJson["has_buoyancy"] = true;
        objJson["buoyancy_fluid_density"] = buoyancy->fluidDensity;
        objJson["buoyancy_volume"] = buoyancy->volume;
        objJson["buoyancy_height"] = buoyancy->height;
        objJson["buoyancy_multiplier"] = buoyancy->buoyancyMultiplier;
        // lastCalculatedVolume — внутренний кэш, не сохраняем
    }

    // ============================================================================
    // CompartmentComponent + BreachData
    // ============================================================================
    if (auto* compartment = m_registry.GetComponent<CompartmentComponent>(entity))
    {
        objJson["has_compartment"] = true;

        // Габариты
        objJson["compartment_local_min"] = { compartment->localMin.x, compartment->localMin.y, compartment->localMin.z };
        objJson["compartment_local_max"] = { compartment->localMax.x, compartment->localMax.y, compartment->localMax.z };
        objJson["compartment_volume"] = compartment->volume;

        // Состояние воды
        objJson["compartment_water_volume"] = compartment->waterVolume;
        objJson["compartment_water_level"] = compartment->waterLevel;

        // Флаги
        objJson["compartment_is_sealed"] = compartment->isSealed;
        objJson["compartment_has_power"] = compartment->hasPower;
        objJson["compartment_pump_rate"] = compartment->pumpRate;
        objJson["compartment_damage_visual"] = compartment->damageVisual;

        // 🔥 Breaches — массив структур
        json breachesJson = json::array();
        for (const auto& breach : compartment->breaches)
        {
            json b;
            b["local_position"] = { breach.localPosition.x, breach.localPosition.y, breach.localPosition.z };
            b["size"] = breach.size;
            b["flow_rate"] = breach.flowRate;
            b["is_sealed"] = breach.isSealed;
            breachesJson.push_back(b);
        }
        objJson["compartment_breaches"] = breachesJson;

        // connectedCompartments — сохраняем как индексы сущностей (Entity — это uint32_t)
        json connectionsJson = json::array();
        for (Entity connected : compartment->connectedCompartments)
        {
            connectionsJson.push_back(static_cast<uint32_t>(connected));
        }
        objJson["compartment_connected"] = connectionsJson;
    }

    // ============================================================================
    // SubmarineComponent
    // ============================================================================
    if (auto* sub = m_registry.GetComponent<SubmarineComponent>(entity))
    {
        objJson["has_submarine"] = true;
        objJson["sub_root_entity"] = static_cast<uint32_t>(sub->submarineRoot);
        objJson["sub_compartment_index"] = sub->compartmentIndex;
        objJson["sub_is_detached"] = sub->isDetached;

        // Список всех отсеков
        json compartmentsJson = json::array();
        for (Entity comp : sub->allCompartments)
        {
            compartmentsJson.push_back(static_cast<uint32_t>(comp));
        }
        objJson["sub_all_compartments"] = compartmentsJson;

        // Кэшированные значения — не сохраняем, пересчитаются при загрузке
    }

    // ============================================================================
    // PressureHullComponent
    // ============================================================================
    if (auto* hull = m_registry.GetComponent<PressureHullComponent>(entity))
    {
        objJson["has_pressure_hull"] = true;
        objJson["hull_max_pressure"] = hull->maxPressure;
        objJson["hull_current_health"] = hull->currentHealth;
        objJson["hull_repair_rate"] = hull->repairRate;
        objJson["hull_is_leaking"] = hull->isLeaking;
        objJson["hull_leak_intensity"] = hull->leakIntensity;
        // lastDepth/lastPressure — кэш, не сохраняем
    }

    // ============================================================================
    // SubmarineJointComponent
    // ============================================================================
    if (auto* joint = m_registry.GetComponent<SubmarineJointComponent>(entity))
    {
        objJson["has_sub_joint"] = true;
        objJson["sub_joint_comp_a"] = static_cast<uint32_t>(joint->compartmentA);
        objJson["sub_joint_comp_b"] = static_cast<uint32_t>(joint->compartmentB);
        objJson["sub_joint_break_force"] = joint->breakForce;
        objJson["sub_joint_current_stress"] = joint->currentStress;
        objJson["sub_joint_is_broken"] = joint->isBroken;
        // btTypedConstraint* не сериализуется — воссоздаётся в PhysicsSystem
    }

    // ============================================================================
    // JointConnectionComponent
    // ============================================================================
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

    if (auto* rig = m_registry.GetComponent<RigidBodyComponent>(entity))
    {
        objJson["is_mass"] = rig->mass;
        objJson["ShapeType"] = rig->shapeType;
        objJson["ShapeSize"] = { rig->shapeSize.x(), rig->shapeSize.y(), rig->shapeSize.z()};

    }

    if (auto* mesh = m_registry.GetComponent<MeshRendererComponent>(entity))
    {
        if (mesh->material && mesh->material->GetShader())
        {
            // 🔥 Сохраняем тип шейдера через его метод
            ShaderType type = mesh->material->GetShader()->GetType();
            objJson["shader_type"] = static_cast<int>(type);
        }
    }


    objJson["model_path"] = modelPath;
    objJson["texture_path"] = texturePath;

    // Читаем трансформацию из ECS
    if (auto* t = m_registry.GetComponent<TransformComponent>(entity))
    {
        objJson["position"] = { {"x", t->position.x}, {"y", t->position.y}, {"z", t->position.z} };
        objJson["rotation"] = { {"x", t->rotation.x}, {"y", t->rotation.y}, {"z", t->rotation.z} };
        objJson["scale"] = { {"x", t->scale.x},    {"y", t->scale.y},    {"z", t->scale.z} };
    }

    return objJson;
}
// ============================================================================
// LoadScene — десериализация
// ============================================================================
bool SceneManager::LoadScene(const std::string& filename,
    ID3D11Device* device, ID3D11DeviceContext* deviceContext)
{
    OutputDebugStringA(("LoadScene: " + filename + "\n").c_str());

    if (filename.empty()) return false;

    std::ifstream file(filename);
    if (!file.is_open())
    {
        OutputDebugStringA(("LoadScene: cannot open: " + filename + "\n").c_str());
        return false;
    }

    json j;
    try { file >> j; }
    catch (const json::exception& e)
    {
        OutputDebugStringA(("LoadScene: JSON error: " + std::string(e.what()) + "\n").c_str());
        return false;
    }
    file.close();

    Clear();

    if (j.contains("objects") && j["objects"].is_array())
    {
        for (const auto& objJson : j["objects"])
            DeserializeObject(objJson, device, deviceContext);
    }


    OutputDebugStringA("LoadScene: OK\n");
    return true;
}

// ============================================================================
// DeserializeObject — создать ECS-сущность из JSON
//
// НЕДЕЛЯ 3: возвращает Entity вместо GameObject
Entity SceneManager::DeserializeObject(const json& j,
    ID3D11Device* device, ID3D11DeviceContext* deviceContext)
{
    std::string modelPath = j.value("model_path", "assets/model/sphere.obj");
    std::string texturePath = j.value("texture_path", "assets/texture/stone.tga");

    AssimpModelClass* model = GetOrCreateModel(modelPath, device, deviceContext, texturePath);
    if (!model) model = m_mainModelFallback;
    if (!model) return INVALID_ENTITY;

    float x = 0, y = 0, z = 0;
    if (j.contains("position"))
    {
        x = j["position"].value("x", 0.0f);
        y = j["position"].value("y", 0.0f);
        z = j["position"].value("z", 0.0f);
    }

    Entity e = SpawnObject(model, x, y, z);

    if (auto* t = m_registry.GetComponent<TransformComponent>(e))
    {
        if (j.contains("rotation"))
        {
            t->rotation.x = j["rotation"].value("x", 0.0f);
            t->rotation.y = j["rotation"].value("y", 0.0f);
            t->rotation.z = j["rotation"].value("z", 0.0f);
        }
        if (j.contains("scale"))
        {
            t->scale.x = j["scale"].value("x", 1.0f);
            t->scale.y = j["scale"].value("y", 1.0f);
            t->scale.z = j["scale"].value("z", 1.0f);
        }
    }
    if (j.value("is_light", false))
    {
        LightComponent light;

        if (j.contains("light_color")) {
            auto& c = j["light_color"];
            light.color = { c[0], c[1], c[2], c[3] };
        }
        light.enabled = j.value("light_enabled", true);
        light.radius = j.value("light_radius", 10.0f);

        if (j.contains("light_offset")) {
            auto& o = j["light_offset"];
            light.offset = { o[0], o[1], o[2] };
        }

        int attachedIdx = j.value("light_attached_to", -1);
        light.attachedTo = (attachedIdx >= 0) ? static_cast<Entity>(attachedIdx) : INVALID_ENTITY;

        light.isEditorGizmo = j.value("light_is_gizmo", false);

        // 🔥 КЛЮЧЕВОЕ: Добавляем компонент в сущность 'e'
        m_registry.AddComponent<LightComponent>(e, light);
    }
    if(j.contains("is_mass") && j["is_mass"].is_number())
    {
        RigidBodyComponent rig;
        rig.mass = j["is_mass"].get<float>();

        if (j.contains("ShapeType"))
        {
           rig.shapeType = j["ShapeType"];
        }

        if (j.contains("ShapeSize"))
        {
            auto& o = j["ShapeSize"];
            rig.shapeSize = { o[0], o[1], o[2] };
        }
        
      
    }

    if (j.contains("shader_type"))
    {
        int shaderTypeInt = j["shader_type"].get<int>();
        ShaderType type = static_cast<ShaderType>(shaderTypeInt);

        // Получаем шейдер по типу (например, через ShaderManager)
        ShaderBaseClass* shader = ShaderManager::Get().GetShaderType(type);

        if (shader && device)
        {
            auto* mesh = m_registry.GetComponent<MeshRendererComponent>(e);
            if (mesh)
            {
                // Если уже есть материал — удаляем (кроме дефолтного)
                if (mesh->material && mesh->material != m_defaultMaterial)
                    delete mesh->material;

                // Создаём новый материал
                auto* newMat = new MaterialClass();
                newMat->Initialize(device, L"RuntimeMaterial_Loaded");

                // Копируем текстуру из дефолтного, если есть
                if (m_defaultMaterial && m_defaultMaterial->GetMainTexture())
                    newMat->SetMainTexture(m_defaultMaterial->GetMainTexture());

                // Назначаем шейдер
                newMat->SetShader(shader);
                mesh->material = newMat;

                OutputDebugStringA("[Deserialize] Restored custom shader\n");
            }
        }
    }

    if (j.value("has_static_collider", false))
    {
        StaticColliderComponent collider;
        // TODO: здесь нужно воссоздать btCollisionShape по типу и размерам
        // Например: collider.shape = new btBoxShape(btVector3(...));
        m_registry.AddComponent<StaticColliderComponent>(e, collider);
    }

    // ============================================================================
    // BuoyancyComponent
    // ============================================================================
    if (j.value("has_buoyancy", false))
    {
        BuoyancyComponent buoyancy;
        buoyancy.fluidDensity = j.value("buoyancy_fluid_density", 1025.0f);
        buoyancy.volume = j.value("buoyancy_volume", 10.0f);
        buoyancy.height = j.value("buoyancy_height", 5.0f);
        buoyancy.buoyancyMultiplier = j.value("buoyancy_multiplier", 1.0f);
        buoyancy.lastCalculatedVolume = -1.0f; // сброс кэша
        m_registry.AddComponent<BuoyancyComponent>(e, buoyancy);
    }

    // ============================================================================
    // CompartmentComponent
    // ============================================================================
    if (j.value("has_compartment", false))
    {
        CompartmentComponent compartment;

        // Габариты
        if (j.contains("compartment_local_min")) {
            auto& min = j["compartment_local_min"];
            compartment.localMin = { min[0], min[1], min[2] };
        }
        if (j.contains("compartment_local_max")) {
            auto& max = j["compartment_local_max"];
            compartment.localMax = { max[0], max[1], max[2] };
        }
        compartment.volume = j.value("compartment_volume", 8.0f);

        // Состояние воды
        compartment.waterVolume = j.value("compartment_water_volume", 0.0f);
        compartment.waterLevel = j.value("compartment_water_level", 0.0f);

        // Флаги
        compartment.isSealed = j.value("compartment_is_sealed", true);
        compartment.hasPower = j.value("compartment_has_power", true);
        compartment.pumpRate = j.value("compartment_pump_rate", 0.1f);
        compartment.damageVisual = j.value("compartment_damage_visual", 0.0f);

        // 🔥 Breaches
        if (j.contains("compartment_breaches") && j["compartment_breaches"].is_array())
        {
            for (const auto& bJson : j["compartment_breaches"])
            {
                BreachData breach;
                if (bJson.contains("local_position")) {
                    auto& pos = bJson["local_position"];
                    breach.localPosition = { pos[0], pos[1], pos[2] };
                }
                breach.size = bJson.value("size", 0.5f);
                breach.flowRate = bJson.value("flow_rate", 0.0f);
                breach.isSealed = bJson.value("is_sealed", false);
                compartment.breaches.push_back(breach);
            }
        }

        // connectedCompartments — восстанавливаем Entity из uint32_t
        if (j.contains("compartment_connected") && j["compartment_connected"].is_array())
        {
            for (const auto& entJson : j["compartment_connected"])
            {
                Entity connected = static_cast<Entity>(entJson.get<uint32_t>());
                compartment.connectedCompartments.push_back(connected);
            }
        }

        // Кэш размеров
        compartment.CacheSize();

        m_registry.AddComponent<CompartmentComponent>(e, compartment);
    }

    // ============================================================================
    // SubmarineComponent
    // ============================================================================
    if (j.value("has_submarine", false))
    {
        SubmarineComponent sub;
        sub.submarineRoot = static_cast<Entity>(j.value("sub_root_entity", static_cast<uint32_t>(INVALID_ENTITY)));
        sub.compartmentIndex = j.value("sub_compartment_index", 0);
        sub.isDetached = j.value("sub_is_detached", false);

        if (j.contains("sub_all_compartments") && j["sub_all_compartments"].is_array())
        {
            for (const auto& entJson : j["sub_all_compartments"])
            {
                Entity comp = static_cast<Entity>(entJson.get<uint32_t>());
                sub.allCompartments.push_back(comp);
            }
        }

        // Сброс кэшей — пересчитаются в системе
        sub.cachedTotalMass = 0.0f;
        sub.cachedTotalVolume = 0.0f;
        sub.needsPhysicsUpdate = true;
        sub.compartmentCountLastFrame = -1;

        m_registry.AddComponent<SubmarineComponent>(e, sub);
    }

    // ============================================================================
    // PressureHullComponent
    // ============================================================================
    if (j.value("has_pressure_hull", false))
    {
        PressureHullComponent hull;
        hull.maxPressure = j.value("hull_max_pressure", 202000.0f);
        hull.currentHealth = j.value("hull_current_health", 100.0f);
        hull.repairRate = j.value("hull_repair_rate", 5.0f);
        hull.isLeaking = j.value("hull_is_leaking", false);
        hull.leakIntensity = j.value("hull_leak_intensity", 0.0f);
        hull.lastDepth = 0.0f;
        hull.lastPressure = 0.0f;
        m_registry.AddComponent<PressureHullComponent>(e, hull);
    }

    // ============================================================================
    // SubmarineJointComponent
    // ============================================================================
    if (j.value("has_sub_joint", false))
    {
        SubmarineJointComponent joint;
        joint.compartmentA = static_cast<Entity>(j.value("sub_joint_comp_a", static_cast<uint32_t>(INVALID_ENTITY)));
        joint.compartmentB = static_cast<Entity>(j.value("sub_joint_comp_b", static_cast<uint32_t>(INVALID_ENTITY)));
        joint.breakForce = j.value("sub_joint_break_force", 50000.0f);
        joint.currentStress = j.value("sub_joint_current_stress", 0.0f);
        joint.isBroken = j.value("sub_joint_is_broken", false);
        joint.joint = nullptr; // btTypedConstraint воссоздаётся в PhysicsSystem
        m_registry.AddComponent<SubmarineJointComponent>(e, joint);
    }

    // ============================================================================
    // JointConnectionComponent
    // ============================================================================
    if (j.value("has_joint_connection", false))
    {
        JointConnectionComponent conn;
        conn.targetEntity = static_cast<Entity>(j.value("joint_target_entity", static_cast<uint32_t>(INVALID_ENTITY)));

        if (j.contains("joint_pivot_a")) {
            auto& a = j["joint_pivot_a"];
            conn.pivotA = btVector3(a[0], a[1], a[2]);
        }
        if (j.contains("joint_pivot_b")) {
            auto& b = j["joint_pivot_b"];
            conn.pivotB = btVector3(b[0], b[1], b[2]);
        }

        conn.breakForce = j.value("joint_break_force", 150000.0f);
        conn.disableCollisions = j.value("joint_disable_collisions", true);
        conn.isBroken = j.value("joint_is_broken", false);
        conn.showGizmo = j.value("joint_show_gizmo", true);

        if (j.contains("joint_gizmo_color")) {
            auto& c = j["joint_gizmo_color"];
            conn.gizmoColor = { c[0], c[1], c[2], c[3] };
        }

        conn.joint = nullptr; // воссоздаётся позже
        m_registry.AddComponent<JointConnectionComponent>(e, conn);
    }

    // 🔥 FIX: Назначаем defaultMaterial ТОЛЬКО если у объекта ещё нет материала
    if (m_defaultMaterial)
    {
        if (auto* mesh = m_registry.GetComponent<MeshRendererComponent>(e))
        {
            if (!mesh->material)  // ← Важно: не перезаписываем уже назначенный!
                mesh->material = m_defaultMaterial;
        }
    }


    return e;
}

json SceneManager::SerializeMaterial(const MaterialClass* mat) { return json(); }

// ============================================================================
// GetOrCreateModel — кэш моделей
// ============================================================================
AssimpModelClass* SceneManager::GetOrCreateModel(
    const std::string& path,
    ID3D11Device* device, ID3D11DeviceContext* deviceContext,
    const std::string& texturePath)
{
    if (path.empty()) return m_mainModelFallback;

    auto it = m_modelCache.find(path);
    if (it != m_modelCache.end() && it->second) return it->second;

    AssimpModelClass* model = new AssimpModelClass();

    if (GetFileAttributesA(path.c_str()) == INVALID_FILE_ATTRIBUTES)
    {
        OutputDebugStringA(("GetOrCreateModel: not found: " + path + "\n").c_str());
        delete model;
        return m_mainModelFallback;
    }

    // Для GLB texturePath будет пустым — передаём nullptr,
    // чтобы AssimpModelClass не грузил fallback текстуру поверх вшитых
    const std::string ext = fs::path(path).extension().string();
    const bool isGLB = (ext == ".glb" || ext == ".gltf");
    const char* texPath = (!isGLB && !texturePath.empty()) ? texturePath.c_str() : nullptr;

    if (model->Initialize(device, deviceContext, path.c_str(), texPath))
    {
        m_modelCache[path] = model;
        return model;
    }

    delete model;
    return m_mainModelFallback;
}


void SceneManager::SetObjectShader(Entity entity, ShaderBaseClass* shader, ID3D11Device* device)
{
    auto* mesh = m_registry.GetComponents<MeshRendererComponent>().Get(entity);
    if (!mesh) return;

    // 🔥 FIX: Если объект использует общий defaultMaterial — создаём ему личный копию
    if (mesh->material == m_defaultMaterial)
    {
        if (!device) return;

        auto* newMat = new MaterialClass();
        newMat->Initialize(device, L"RuntimeMaterial_Custom");

        // Копируем текстуры из дефолтного материала
        if (m_defaultMaterial)
        {
            newMat->SetMainTexture(m_defaultMaterial->GetMainTexture());
            // Если есть другие слоты — скопируй их тоже:
            // newMat->SetTexture(1, m_defaultMaterial->GetTexture(1));
        }

        mesh->material = newMat;  // Теперь у объекта свой материал!
        OutputDebugStringA("[SetObjectShader] Created unique material for entity\n");
    }

    // Теперь безопасно меняем шейдер — только у этого объекта
    if (mesh->material && shader)
    {
        mesh->material->SetShader(shader);

        // Glass-шейдер требует refractionSRV + normalTexture (slot 1)
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
    ID3D11DeviceContext* context)
{
    auto* mesh = m_registry.GetComponents<MeshRendererComponent>().Get(entity);
    if (!mesh) return;
    if (!device || !context) return;

    // 1. Создаём материал, если нет
    if (!mesh->material)
    {
        auto* mat = new MaterialClass();
        mat->Initialize(device, L"RuntimeMaterial");
        if (m_defaultMaterial && m_defaultMaterial->GetShader())
            mat->SetShader(m_defaultMaterial->GetShader());
        mesh->material = mat;
    }

    // Позволяем очищать слот пустым путём
    if (path.empty())
    {
        mesh->material->SetTexture(slot, nullptr);
        OutputDebugStringA(("SetObjectTexture: Cleared texture slot " + std::to_string(slot) + "\n").c_str());
        return;
    }

    // Проверка наличия файла
    if (GetFileAttributesA(path.c_str()) == INVALID_FILE_ATTRIBUTES)
    {
        OutputDebugStringA(("SetObjectTexture: File not found: " + path + "\n").c_str());
        return;
    }

    // 2. Загружаем текстуру (используем TextureClass, как в SpawnObjectFromRequest)
    TextureClass* loadedTexture = new TextureClass();
    if (!loadedTexture->Initialize(device, context, path.c_str()))
    {
        OutputDebugStringA(("SetObjectTexture: TextureClass::Initialize FAILED for: " + path + "\n").c_str());
        delete loadedTexture;
        return;
    }

    // 3. Назначаем текстуру в нужный слот материала
    mesh->material->SetTexture(slot, loadedTexture);

    // 4. Если это главная текстура (slot 0) и шейдер ещё не назначен — можно подтянуть дефолтный шейдер
    if (slot == 0 && !mesh->material->GetShader() && m_defaultMaterial)
    {
        mesh->material->SetShader(m_defaultMaterial->GetShader());
    }

    OutputDebugStringA(("SetObjectTexture: Assigned texture to slot " + std::to_string(slot) + " -> " + path + "\n").c_str());
}




// ============================================================================
// SpawnLight — создать источник света как ECS-сущность
// ============================================================================
Entity SceneManager::SpawnLight(const XMFLOAT3& position, const XMFLOAT4& color, bool isGizmo)
{
    AssimpModelClass* model = m_lightModel ? m_lightModel : m_mainModelFallback;
    if (!model) return INVALID_ENTITY;

    Entity e = SpawnObject(model, position.x, position.y, position.z);
    if (e == INVALID_ENTITY) return INVALID_ENTITY;

    // Масштаб
    if (auto* t = m_registry.GetComponent<TransformComponent>(e))
    {
        float s = isGizmo ? 0.2f : 0.3f;
        t->scale = { s, s, s };
    }

    // ✅ НОВОЕ: Создаём материал с ColorShader, чтобы сфера светилась
    if (auto* mesh = m_registry.GetComponent<MeshRendererComponent>(e))
    {
        // Здесь нужно передать device для создания материала
        // Или установить уже существующий материал с ColorShader
    }

    // Добавляем LightComponent
    LightComponent light;
    light.color = color;
    light.enabled = true;
    light.isEditorGizmo = isGizmo;
    light.radius = 10.0f;
    m_registry.AddComponent<LightComponent>(e, light);

    return e;
}



// ============================================================================
// RemoveLight — удалить источник света
// ============================================================================
bool SceneManager::RemoveLight(Entity entity)
{
    if (!m_registry.HasComponent<LightComponent>(entity))
        return false;

    // Деактивируем — RenderSystem и GatherLights пропустят
    m_registry.RemoveComponent<ActiveComponent>(entity);
    return true;
}

// ============================================================================
// Хелперы для редактора
// ============================================================================
void SceneManager::SetLightColor(Entity entity, const XMFLOAT4& color)
{
    if (auto* light = m_registry.GetComponent<LightComponent>(entity))
        light->color = color;
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
    // Позиция обновится в UpdateLights()
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
// GatherLightsForShader — собрать данные для передачи в шейдер
// Вызывается перед RenderSystem::Render
// ============================================================================
void SceneManager::GatherLightsForShader(XMFLOAT4 outColors[], XMFLOAT4 outPositions[], int maxLights)
{
    // Обнуляем слоты
    for (int i = 0; i < maxLights; ++i)
    {
        outColors[i] = XMFLOAT4(0, 0, 0, 0);
        outPositions[i] = XMFLOAT4(0, 0, 0, 0);
    }

    int slot = 0;
    auto& lights = m_registry.GetComponents<LightComponent>();
    auto& transforms = m_registry.GetComponents<TransformComponent>();

    for (auto& [entity, light] : lights)
    {
        if (slot >= maxLights) break;
        if (!light.enabled) continue;

        // Пропускаем редакторные гизмо в игровом рендере
        if (light.isEditorGizmo) continue;

        // Пропускаем неактивные объекты
        if (!m_registry.HasComponent<ActiveComponent>(entity)) continue;

        auto* transform = transforms.Get(entity);
        if (!transform) continue;

        // Если свет привязан к объекту — добавляем оффсет
        XMFLOAT3 worldPos = transform->position;
        if (light.attachedTo != INVALID_ENTITY)
        {
            auto* targetTransform = transforms.Get(light.attachedTo);
            if (targetTransform)
            {
                // Простое сложение (для полноценной привязки нужно учитывать ротацию)
                worldPos = {
                    targetTransform->position.x + light.offset.x,
                    targetTransform->position.y + light.offset.y,
                    targetTransform->position.z + light.offset.z
                };
            }
        }

        outColors[slot] = light.color;
        outPositions[slot] = XMFLOAT4(worldPos.x, worldPos.y, worldPos.z, 1.0f);
        ++slot;
    }
}

// ============================================================================
// UpdateLights — обновить позиции привязанных источников
// Вызывается каждый кадр перед рендером
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
            lightTransform->position = {
                targetTransform->position.x + light.offset.x,
                targetTransform->position.y + light.offset.y,
                targetTransform->position.z + light.offset.z
            };
        }
    }
}