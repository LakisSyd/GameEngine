// ============================================================================
// SceneManager.cpp
//
// НЕДЕЛЯ 4: GetObject(int) и m_legacyObjects удалены.
//           EditorUI и PickingSystem работают через Entity + ECS.
// ============================================================================

#include "SceneManager.h"
#include "FileSystem.h"
#include "ShaderManager.h"
#include "PhysicsSystem.h"
#include "../AnimationSystem.h"
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

    if (model && model->HasAnimations()) {
        m_registry.AddComponent<AnimationComponent>(e, AnimationComponent{});
    }

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
    AnimationSystem::Update(m_registry, deltaTime);
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
    XMFLOAT3 cameraPos,
    float deltaTime)
{
    GatherLightsForShader(lightColors, lightPositions, NUM_LIGHTS);

    auto& meshPool = m_registry.GetComponents<MeshRendererComponent>();


    // ── Проход 1: только непрозрачные объекты (запись в z-buffer) ────────────
    for (auto& [entity, mesh] : meshPool)
    {
        if (!m_registry.HasComponent<ActiveComponent>(entity)) continue;
        auto* light = m_registry.GetComponent<LightComponent>(entity);
        if (light && light->isEditorGizmo) continue;
        auto* transform = m_registry.GetComponent<TransformComponent>(entity);
        if (!transform || !mesh.model) continue;
        auto* mat = mesh.material ? mesh.material : defaultMaterial;
        if (!mat) continue;

        // Пропускаем стеклянные объекты в первом проходе
        if (mat->IsGlass()) continue;

        // Проверяем также embedded transparent meshes
        bool hasTransparentParts = false;
        for (const auto& part : mesh.model->GetMeshParts())
            if (part.isTransparent) { hasTransparentParts = true; break; }
        if (hasTransparentParts && mat->GetShader() &&
            mat->GetShader()->GetType() == ShaderType::PBR) continue;

        XMMATRIX world = transform->GetWorldMatrix();

        auto* animComp = m_registry.GetComponent<AnimationComponent>(entity);

        RenderMesh(context, mesh.model, world, viewMatrix, projectionMatrix,
            mat, lightColors, lightPositions, fog,
            entity == selectedEntity, cameraPos, deltaTime, animComp);
    }

    // ── Проход 2: прозрачные объекты (стекло) — z-buffer на чтение ──────────
    // Отключаем запись в z-buffer чтобы стекло не перекрывало само себя
    // и не скрывало объекты за ним
    for (auto& [entity, mesh] : meshPool)
    {
        if (!m_registry.HasComponent<ActiveComponent>(entity)) continue;
        auto* light = m_registry.GetComponent<LightComponent>(entity);
        if (light && light->isEditorGizmo) continue;
        auto* transform = m_registry.GetComponent<TransformComponent>(entity);
        if (!transform || !mesh.model) continue;
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
        RenderMesh(context, mesh.model, world, viewMatrix, projectionMatrix,
            mat, lightColors, lightPositions, fog,
            entity == selectedEntity, cameraPos, deltaTime);
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
    Entity               excludeEntity,
    float                deltaTime)
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
        auto* animComp = m_registry.GetComponent<AnimationComponent>(entity);

        RenderMesh(context, mesh.model, world, viewMatrix, projectionMatrix,
            mat, lightColors, lightPositions, fog, false, XMFLOAT3(), deltaTime, animComp);
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

    int id = 0;

    // ── Сохраняем объекты с мешем (обычные объекты, корни подлодок и т.д.) ──
    auto& meshPool = m_registry.GetComponents<MeshRendererComponent>();
    for (auto& [entity, mesh] : meshPool)
    {
        if (!m_registry.HasComponent<ActiveComponent>(entity)) continue;
        scene["objects"].push_back(SerializeObject(entity, id++));
    }

    // ── Сохраняем отсеки БЕЗ меша (логические CompartmentComponent) ──────────
    // Отсеки могут не иметь MeshRendererComponent — они всё равно должны быть сохранены,
    // иначе SubmarineComponent.allCompartments будет указывать на невалидные Entity.
    auto& compPool = m_registry.GetComponents<CompartmentComponent>();
    for (auto& [entity, comp] : compPool)
    {
        // Уже сохранён через meshPool — пропускаем
        if (m_registry.HasComponent<MeshRendererComponent>(entity)) continue;
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
    objJson["entity_id"] = static_cast<uint32_t>(entity); // ← сохраняем оригинальный Entity ID

    // ── Model / Texture paths ─────────────────────────────────────────────────
    std::string modelPath = "assets/model/sphere.obj";
    std::string texturePath = "assets/texture/stone.tga";

    if (auto* mesh = m_registry.GetComponent<MeshRendererComponent>(entity))
    {
        for (auto& [path, cachedModel] : m_modelCache)
        {
            if (cachedModel == mesh->model) { modelPath = path; break; }
        }
    }
    objJson["model_path"] = modelPath;
    objJson["texture_path"] = texturePath;

    // ── TransformComponent ────────────────────────────────────────────────────
    if (auto* t = m_registry.GetComponent<TransformComponent>(entity))
    {
        objJson["position"] = { {"x", t->position.x}, {"y", t->position.y}, {"z", t->position.z} };
        objJson["rotation"] = { {"x", t->rotation.x}, {"y", t->rotation.y}, {"z", t->rotation.z} };
        objJson["scale"] = { {"x", t->scale.x},    {"y", t->scale.y},    {"z", t->scale.z} };
    }

    // ── Shader ────────────────────────────────────────────────────────────────
    if (auto* mesh = m_registry.GetComponent<MeshRendererComponent>(entity))
    {
        if (mesh->material && mesh->material->GetShader())
            objJson["shader_type"] = static_cast<int>(mesh->material->GetShader()->GetType());
    }

    // ── LightComponent ────────────────────────────────────────────────────────
    if (auto* light = m_registry.GetComponent<LightComponent>(entity))
    {
        objJson["is_light"] = true;
        objJson["light_color"] = { light->color.x, light->color.y, light->color.z, light->color.w };
        objJson["light_enabled"] = light->enabled;
        objJson["light_radius"] = light->radius;
        objJson["light_offset"] = { light->offset.x, light->offset.y, light->offset.z };
        objJson["light_is_gizmo"] = light->isEditorGizmo;
        objJson["light_attached_to"] = static_cast<uint32_t>(light->attachedTo);
    }

    // ── RigidBodyComponent ────────────────────────────────────────────────────
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
            e["box_offset_min"] = { elem.boxOffsetMin.x, elem.boxOffsetMin.y, elem.boxOffsetMin.z };
            e["box_offset_max"] = { elem.boxOffsetMax.x, elem.boxOffsetMax.y, elem.boxOffsetMax.z };
            e["local_position"] = { elem.localPosition.x, elem.localPosition.y, elem.localPosition.z };
            e["local_rotation"] = { elem.localRotation.x, elem.localRotation.y,
                                       elem.localRotation.z, elem.localRotation.w };
            e["radius_offset"] = elem.radiusOffset;
            e["height_offset_top"] = elem.heightOffsetTop;
            e["height_offset_bot"] = elem.heightOffsetBottom;
            e["debug_weight"] = elem.debugWeight;
            // editorEulerDegrees — только для UI, не сохраняем
            elemsJson.push_back(e);
        }
        objJson["rb_compound_elements"] = elemsJson;
    }

    // ── SubmarineHullConfig ───────────────────────────────────────────────────
    if (auto* hull = m_registry.GetComponent<SubmarineHullConfig>(entity))
    {
        objJson["has_hull_config"] = true;
        objJson["hull_extents"] = { hull->hullExtents.x, hull->hullExtents.y, hull->hullExtents.z };
        objJson["hull_wall_thickness"] = hull->wallThickness;
        objJson["hull_is_kinematic"] = hull->isKinematic;
        objJson["hull_show_debug"] = hull->showDebugHull;
    }

    // ── StaticColliderComponent ───────────────────────────────────────────────
    if (auto* collider = m_registry.GetComponent<StaticColliderComponent>(entity))
    {
        objJson["has_static_collider"] = true;
    }

    // ── BuoyancyComponent ─────────────────────────────────────────────────────
    if (auto* buoyancy = m_registry.GetComponent<BuoyancyComponent>(entity))
    {
        objJson["has_buoyancy"] = true;
        objJson["buoyancy_fluid_density"] = buoyancy->fluidDensity;
        objJson["buoyancy_volume"] = buoyancy->volume;
        objJson["buoyancy_height"] = buoyancy->height;
        objJson["buoyancy_multiplier"] = buoyancy->buoyancyMultiplier;
    }

    // ── CompartmentComponent ──────────────────────────────────────────────────
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

        json connectionsJson = json::array();
        for (Entity connected : comp->connectedCompartments)
            connectionsJson.push_back(static_cast<uint32_t>(connected));
        objJson["compartment_connected"] = connectionsJson;
    }

    // ── SubmarineComponent ────────────────────────────────────────────────────
    if (auto* sub = m_registry.GetComponent<SubmarineComponent>(entity))
    {
        objJson["has_submarine"] = true;
        objJson["sub_root_entity"] = static_cast<uint32_t>(sub->submarineRoot);
        objJson["sub_compartment_index"] = sub->compartmentIndex;
        objJson["sub_is_detached"] = sub->isDetached;

        json compartmentsJson = json::array();
        for (Entity c : sub->allCompartments)
            compartmentsJson.push_back(static_cast<uint32_t>(c));
        objJson["sub_all_compartments"] = compartmentsJson;
    }

    // ── PressureHullComponent ─────────────────────────────────────────────────
    if (auto* phull = m_registry.GetComponent<PressureHullComponent>(entity))
    {
        objJson["has_pressure_hull"] = true;
        objJson["hull_max_pressure"] = phull->maxPressure;
        objJson["hull_current_health"] = phull->currentHealth;
        objJson["hull_repair_rate"] = phull->repairRate;
        objJson["hull_is_leaking"] = phull->isLeaking;
        objJson["hull_leak_intensity"] = phull->leakIntensity;
    }

    // ── SubmarineJointComponent ───────────────────────────────────────────────
    if (auto* joint = m_registry.GetComponent<SubmarineJointComponent>(entity))
    {
        objJson["has_sub_joint"] = true;
        objJson["sub_joint_comp_a"] = static_cast<uint32_t>(joint->compartmentA);
        objJson["sub_joint_comp_b"] = static_cast<uint32_t>(joint->compartmentB);
        objJson["sub_joint_break_force"] = joint->breakForce;
        objJson["sub_joint_current_stress"] = joint->currentStress;
        objJson["sub_joint_is_broken"] = joint->isBroken;
    }

    // ── JointConnectionComponent ──────────────────────────────────────────────
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
        objJson["joint_gizmo_color"] = { conn->gizmoColor.x, conn->gizmoColor.y,
                                                  conn->gizmoColor.z, conn->gizmoColor.w };
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

    // ── Таблица ремаппинга: старый Entity ID → новый Entity ID ──────────────
    std::unordered_map<uint32_t, Entity> entityRemap;

    if (j.contains("objects") && j["objects"].is_array())
    {
        for (const auto& objJson : j["objects"])
        {
            Entity newEntity = DeserializeObject(objJson, device, deviceContext);
            if (newEntity != INVALID_ENTITY && objJson.contains("entity_id"))
            {
                uint32_t oldId = objJson["entity_id"].get<uint32_t>();
                entityRemap[oldId] = newEntity;

                char buf[128];
                sprintf_s(buf, "[LoadScene] Remap: old=%u → new=%u\n", oldId, newEntity);
                OutputDebugStringA(buf);
            }
        }
    }

    // ── Фаза 2: перебиндить все ссылки на Entity ─────────────────────────────
    RemapEntityReferences(entityRemap);

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
    std::string modelPath = j.value("model_path", "");
    std::string texturePath = j.value("texture_path", "assets/texture/stone.tga");

    // Отсек без меша — только логическая сущность (Transform + Active)
    const bool isLogicalOnly = j.value("has_compartment", false) && modelPath.empty();

    Entity e = INVALID_ENTITY;

    if (isLogicalOnly)
    {
        e = m_registry.CreateEntity();
        float x = j.contains("position") ? j["position"].value("x", 0.0f) : 0.0f;
        float y = j.contains("position") ? j["position"].value("y", 0.0f) : 0.0f;
        float z = j.contains("position") ? j["position"].value("z", 0.0f) : 0.0f;
        m_registry.AddComponent<TransformComponent>(e, TransformComponent{ {x, y, z}, {0,0,0}, {1,1,1} });
        m_registry.AddComponent<ActiveComponent>(e, {});
    }
    else
    {
        AssimpModelClass* model = modelPath.empty()
            ? m_mainModelFallback
            : GetOrCreateModel(modelPath, device, deviceContext, texturePath);
        if (!model) model = m_mainModelFallback;
        if (!model) return INVALID_ENTITY;

        float x = j.contains("position") ? j["position"].value("x", 0.0f) : 0.0f;
        float y = j.contains("position") ? j["position"].value("y", 0.0f) : 0.0f;
        float z = j.contains("position") ? j["position"].value("z", 0.0f) : 0.0f;
        e = SpawnObject(model, x, y, z);
    }

    if (e == INVALID_ENTITY) return INVALID_ENTITY;

    // ── TransformComponent ────────────────────────────────────────────────────
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

    // ── Shader ────────────────────────────────────────────────────────────────
    if (j.contains("shader_type"))
    {
        ShaderType type = static_cast<ShaderType>(j["shader_type"].get<int>());
        ShaderBaseClass* shader = ShaderManager::Get().GetShaderType(type);
        if (shader && device)
        {
            auto* mesh = m_registry.GetComponent<MeshRendererComponent>(e);
            if (mesh)
            {
                if (mesh->material && mesh->material != m_defaultMaterial)
                    delete mesh->material;
                auto* newMat = new MaterialClass();
                newMat->Initialize(device, L"RuntimeMaterial_Loaded");
                if (m_defaultMaterial && m_defaultMaterial->GetMainTexture())
                    newMat->SetMainTexture(m_defaultMaterial->GetMainTexture());
                newMat->SetShader(shader);
                mesh->material = newMat;
            }
        }
    }

    // ── LightComponent ────────────────────────────────────────────────────────
    if (j.value("is_light", false))
    {
        LightComponent light;
        if (j.contains("light_color"))
        {
            auto& c = j["light_color"];
            light.color = { c[0], c[1], c[2], c[3] };
        }
        light.enabled = j.value("light_enabled", true);
        light.radius = j.value("light_radius", 10.0f);
        light.isEditorGizmo = j.value("light_is_gizmo", false);
        if (j.contains("light_offset"))
        {
            auto& o = j["light_offset"];
            light.offset = { o[0], o[1], o[2] };
        }
        light.attachedTo = static_cast<Entity>(
            j.value("light_attached_to", static_cast<uint32_t>(INVALID_ENTITY)));
        m_registry.AddComponent<LightComponent>(e, light);
    }

    // ── RigidBodyComponent ────────────────────────────────────────────────────
    if (j.value("has_rigidbody", false))
    {
        RigidBodyComponent rig;
        rig.mass = j.value("rb_mass", 1.0f);
        rig.isDynamic = j.value("rb_is_dynamic", true);
        rig.isKinematic = j.value("rb_is_kinematic", false);
        rig.enableCCD = j.value("rb_enable_ccd", false);
        rig.linearDamping = j.value("rb_linear_damping", 0.7f);
        rig.angularDamping = j.value("rb_angular_damping", 0.8f);
        rig.friction = j.value("rb_friction", 0.0f);
        rig.shapeType = static_cast<RigidBodyComponent::ShapeType>(j.value("rb_shape_type", 0));
        rig.shapeRadius = j.value("rb_shape_radius", 0.5f);
        rig.useManualSize = j.value("rb_use_manual_size", false);
        rig.isCompound = j.value("rb_is_compound", false);
        if (j.contains("rb_shape_size"))
        {
            auto& s = j["rb_shape_size"];
            rig.shapeSize = btVector3(s[0], s[1], s[2]);
        }
        if (j.contains("rb_compound_elements") && j["rb_compound_elements"].is_array())
        {
            for (const auto& ejson : j["rb_compound_elements"])
            {
                RigidBodyComponent::CompoundElement elem;
                elem.type = static_cast<RigidBodyComponent::CompoundElement::ShapeType>(ejson.value("type", 0));
                elem.size = ejson.value("size", 1.0f);
                elem.radius = ejson.value("radius", 0.5f);
                elem.height = ejson.value("height", 1.0f);
                if (ejson.contains("box_offset_min")) { auto& m = ejson["box_offset_min"]; elem.boxOffsetMin = { m[0], m[1], m[2] }; }
                if (ejson.contains("box_offset_max")) { auto& m = ejson["box_offset_max"]; elem.boxOffsetMax = { m[0], m[1], m[2] }; }
                if (ejson.contains("local_position")) { auto& p = ejson["local_position"]; elem.localPosition = { p[0], p[1], p[2] }; }
                if (ejson.contains("local_rotation")) { auto& r = ejson["local_rotation"]; elem.localRotation = { r[0], r[1], r[2], r[3] }; }
                elem.radiusOffset = ejson.value("radius_offset", 0.0f);
                elem.heightOffsetTop = ejson.value("height_offset_top", 0.0f);
                elem.heightOffsetBottom = ejson.value("height_offset_bot", 0.0f);
                elem.debugWeight = ejson.value("debug_weight", 1.0f);
                rig.compoundElements.push_back(elem);
            }
        }
        m_registry.AddComponent<RigidBodyComponent>(e, std::move(rig));
        PhysicsSystem::CreateRigidBody(m_registry, e);
    }

    // ── SubmarineHullConfig ───────────────────────────────────────────────────
    if (j.value("has_hull_config", false))
    {
        SubmarineHullConfig hull;
        if (j.contains("hull_extents")) { auto& he = j["hull_extents"]; hull.hullExtents = { he[0], he[1], he[2] }; }
        hull.wallThickness = j.value("hull_wall_thickness", 0.5f);
        hull.isKinematic = j.value("hull_is_kinematic", false);
        hull.showDebugHull = j.value("hull_show_debug", true);
        m_registry.AddComponent<SubmarineHullConfig>(e, hull);
    }

    // ── StaticColliderComponent ───────────────────────────────────────────────
    if (j.value("has_static_collider", false))
        m_registry.AddComponent<StaticColliderComponent>(e, StaticColliderComponent{});

    // ── BuoyancyComponent ─────────────────────────────────────────────────────
    if (j.value("has_buoyancy", false))
    {
        BuoyancyComponent buoyancy;
        buoyancy.fluidDensity = j.value("buoyancy_fluid_density", 1025.0f);
        buoyancy.volume = j.value("buoyancy_volume", 10.0f);
        buoyancy.height = j.value("buoyancy_height", 5.0f);
        buoyancy.buoyancyMultiplier = j.value("buoyancy_multiplier", 1.0f);
        buoyancy.lastCalculatedVolume = -1.0f;
        m_registry.AddComponent<BuoyancyComponent>(e, buoyancy);
    }

    // ── CompartmentComponent ──────────────────────────────────────────────────
    if (j.value("has_compartment", false))
    {
        CompartmentComponent comp;
        if (j.contains("compartment_local_min")) { auto& mn = j["compartment_local_min"]; comp.localMin = { mn[0], mn[1], mn[2] }; }
        if (j.contains("compartment_local_max")) { auto& mx = j["compartment_local_max"]; comp.localMax = { mx[0], mx[1], mx[2] }; }
        comp.volume = j.value("compartment_volume", 8.0f);
        comp.waterVolume = j.value("compartment_water_volume", 0.0f);
        comp.waterLevel = j.value("compartment_water_level", 0.0f);
        comp.isSealed = j.value("compartment_is_sealed", true);
        comp.hasPower = j.value("compartment_has_power", true);
        comp.pumpRate = j.value("compartment_pump_rate", 0.1f);
        comp.damageVisual = j.value("compartment_damage_visual", 0.0f);
        if (j.contains("compartment_breaches") && j["compartment_breaches"].is_array())
        {
            for (const auto& bJson : j["compartment_breaches"])
            {
                BreachData breach;
                if (bJson.contains("local_position")) { auto& p = bJson["local_position"]; breach.localPosition = { p[0], p[1], p[2] }; }
                breach.size = bJson.value("size", 0.5f);
                breach.flowRate = bJson.value("flow_rate", 0.0f);
                breach.isSealed = bJson.value("is_sealed", false);
                comp.breaches.push_back(breach);
            }
        }
        if (j.contains("compartment_connected") && j["compartment_connected"].is_array())
        {
            for (const auto& entJson : j["compartment_connected"])
                comp.connectedCompartments.push_back(static_cast<Entity>(entJson.get<uint32_t>()));
        }
        comp.CacheSize();
        m_registry.AddComponent<CompartmentComponent>(e, comp);
    }

    // ── SubmarineComponent ────────────────────────────────────────────────────
    if (j.value("has_submarine", false))
    {
        SubmarineComponent sub;
        sub.submarineRoot = static_cast<Entity>(j.value("sub_root_entity", static_cast<uint32_t>(INVALID_ENTITY)));
        sub.compartmentIndex = j.value("sub_compartment_index", 0);
        sub.isDetached = j.value("sub_is_detached", false);
        if (j.contains("sub_all_compartments") && j["sub_all_compartments"].is_array())
        {
            for (const auto& ej : j["sub_all_compartments"])
                sub.allCompartments.push_back(static_cast<Entity>(ej.get<uint32_t>()));
        }
        sub.needsPhysicsUpdate = true;
        sub.compartmentCountLastFrame = -1;
        m_registry.AddComponent<SubmarineComponent>(e, sub);
    }

    // ── PressureHullComponent ─────────────────────────────────────────────────
    if (j.value("has_pressure_hull", false))
    {
        PressureHullComponent phull;
        phull.maxPressure = j.value("hull_max_pressure", 202000.0f);
        phull.currentHealth = j.value("hull_current_health", 100.0f);
        phull.repairRate = j.value("hull_repair_rate", 5.0f);
        phull.isLeaking = j.value("hull_is_leaking", false);
        phull.leakIntensity = j.value("hull_leak_intensity", 0.0f);
        m_registry.AddComponent<PressureHullComponent>(e, phull);
    }

    // ── SubmarineJointComponent ───────────────────────────────────────────────
    if (j.value("has_sub_joint", false))
    {
        SubmarineJointComponent joint;
        joint.compartmentA = static_cast<Entity>(j.value("sub_joint_comp_a", static_cast<uint32_t>(INVALID_ENTITY)));
        joint.compartmentB = static_cast<Entity>(j.value("sub_joint_comp_b", static_cast<uint32_t>(INVALID_ENTITY)));
        joint.breakForce = j.value("sub_joint_break_force", 50000.0f);
        joint.currentStress = j.value("sub_joint_current_stress", 0.0f);
        joint.isBroken = j.value("sub_joint_is_broken", false);
        joint.joint = nullptr;
        m_registry.AddComponent<SubmarineJointComponent>(e, joint);
    }

    // ── JointConnectionComponent ──────────────────────────────────────────────
    if (j.value("has_joint_connection", false))
    {
        JointConnectionComponent conn;
        conn.targetEntity = static_cast<Entity>(j.value("joint_target_entity", static_cast<uint32_t>(INVALID_ENTITY)));
        if (j.contains("joint_pivot_a")) { auto& a = j["joint_pivot_a"]; conn.pivotA = btVector3(a[0], a[1], a[2]); }
        if (j.contains("joint_pivot_b")) { auto& b = j["joint_pivot_b"]; conn.pivotB = btVector3(b[0], b[1], b[2]); }
        conn.breakForce = j.value("joint_break_force", 150000.0f);
        conn.disableCollisions = j.value("joint_disable_collisions", true);
        conn.isBroken = j.value("joint_is_broken", false);
        conn.showGizmo = j.value("joint_show_gizmo", true);
        if (j.contains("joint_gizmo_color")) { auto& c = j["joint_gizmo_color"]; conn.gizmoColor = { c[0], c[1], c[2], c[3] }; }
        conn.joint = nullptr;
        m_registry.AddComponent<JointConnectionComponent>(e, conn);
    }

    // ── DefaultMaterial fallback ──────────────────────────────────────────────
    if (m_defaultMaterial)
    {
        if (auto* mesh = m_registry.GetComponent<MeshRendererComponent>(e))
            if (!mesh->material)
                mesh->material = m_defaultMaterial;
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

void SceneManager::RemapEntityReferences(
    const std::unordered_map<uint32_t, Entity>& remap)
{
    auto remapEntity = [&](Entity old) -> Entity
        {
            auto it = remap.find(static_cast<uint32_t>(old));
            if (it != remap.end()) return it->second;

            // ⚠️ ID не найден в таблице — значит объект не был сохранён
            char buf[128];
            sprintf_s(buf, "[Remap] WARNING: old entity %u not found in remap table!\n",
                static_cast<uint32_t>(old));
            OutputDebugStringA(buf);
            return INVALID_ENTITY;
        };

    // ── SubmarineComponent ────────────────────────────────────────────────────
    auto& subPool = m_registry.GetComponents<SubmarineComponent>();
    for (auto& [entity, sub] : subPool)
    {
        char buf[256];
        sprintf_s(buf, "[Remap] SubComp on entity=%u: root=%u, compartments=%zu\n",
            entity, static_cast<uint32_t>(sub.submarineRoot),
            sub.allCompartments.size());
        OutputDebugStringA(buf);

        sub.submarineRoot = remapEntity(sub.submarineRoot);

        for (Entity& c : sub.allCompartments)
        {
            uint32_t oldId = static_cast<uint32_t>(c);
            c = remapEntity(c);
            sprintf_s(buf, "[Remap]   compartment old=%u → new=%u, hasComp=%s\n",
                oldId, static_cast<uint32_t>(c),
                (c != INVALID_ENTITY && m_registry.HasComponent<CompartmentComponent>(c))
                ? "YES" : "NO");
            OutputDebugStringA(buf);
        }
    }

    // ── CompartmentComponent.connectedCompartments ────────────────────────────
    auto& compPool = m_registry.GetComponents<CompartmentComponent>();
    for (auto& [entity, comp] : compPool)
    {
        for (Entity& c : comp.connectedCompartments)
            c = remapEntity(c);
    }

    // ── LightComponent.attachedTo ─────────────────────────────────────────────
    auto& lightPool = m_registry.GetComponents<LightComponent>();
    for (auto& [entity, light] : lightPool)
    {
        if (light.attachedTo != INVALID_ENTITY)
            light.attachedTo = remapEntity(light.attachedTo);
    }

    // ── SubmarineJointComponent ───────────────────────────────────────────────
    auto& jointPool = m_registry.GetComponents<SubmarineJointComponent>();
    for (auto& [entity, joint] : jointPool)
    {
        joint.compartmentA = remapEntity(joint.compartmentA);
        joint.compartmentB = remapEntity(joint.compartmentB);
    }

    // ── JointConnectionComponent ──────────────────────────────────────────────
    auto& connPool = m_registry.GetComponents<JointConnectionComponent>();
    for (auto& [entity, conn] : connPool)
    {
        if (conn.targetEntity != INVALID_ENTITY)
            conn.targetEntity = remapEntity(conn.targetEntity);
    }

    OutputDebugStringA("[LoadScene] Entity remapping complete\n");
}