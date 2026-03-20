// ============================================================================
// SceneManager.cpp
//
// НЕДЕЛЯ 4: GetObject(int) и m_legacyObjects удалены.
//           EditorUI и PickingSystem работают через Entity + ECS.
// ============================================================================

#include "SceneManager.h"
#include "FileSystem.h"
// ============================================================================
// Конструктор
// ============================================================================
SceneManager::SceneManager()
    : m_defaultMaterial(nullptr)
    , m_mainModelFallback(nullptr)
{
    // Гасим все источники света — включать будем явно через SetLight()
    for (auto& light : m_lights)
    {
        light.active = false;
        light.color = XMFLOAT4(0, 0, 0, 1);
    }
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
    XMFLOAT4             lightColors[],
    XMFLOAT4             lightPositions[],
    const Settings::FogSettings& fog,
    Entity               selectedEntity,
    XMFLOAT3             cameraPos)
{
    // ── Шаг 1: заполняем слоты из LightComponent (ECS-светильники) ───────────
    // Сначала обнуляем
    for (int i = 0; i < NUM_LIGHTS; ++i)
    {
        lightColors[i] = XMFLOAT4(0, 0, 0, 0);
        lightPositions[i] = XMFLOAT4(0, 0, 0, 0);
    }

    int slot = 0;

    // ECS-светильники (LightComponent на обычных объектах)
    auto& lightPool = m_registry.GetComponents<LightComponent>();
    for (auto& [entity, light] : lightPool)
    {
        if (slot >= NUM_LIGHTS) break;
        if (!light.enabled) continue;
        if (!m_registry.HasComponent<ActiveComponent>(entity)) continue;

        auto* transform = m_registry.GetComponent<TransformComponent>(entity);
        if (!transform) continue;

        lightColors[slot] = light.color;
        lightPositions[slot] = XMFLOAT4(
            transform->position.x,
            transform->position.y,
            transform->position.z,
            1.0f);
        ++slot;
    }

    // ── Шаг 2: оставшиеся слоты заполняем из legacy m_lights[] ──────────────
    // Это позволяет эдитору (SetLight) и ECS-компонентам работать вместе
    for (int i = 0; i < NUM_LIGHTS && slot < NUM_LIGHTS; ++i)
    {
        if (!m_lights[i].active) continue;

        lightColors[slot] = m_lights[i].color;
        lightPositions[slot] = m_lights[i].position;
        ++slot;
    }

    // ── Шаг 3: рендерим сцену ────────────────────────────────────────────────
    RenderSystem::Render(
        m_registry,
        context,
        viewMatrix, projectionMatrix,
        defaultMaterial,
        lightColors, lightPositions,
        fog,
        selectedEntity,
        cameraPos);
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
    // Копируем данные о свете
    for (int i = 0; i < NUM_LIGHTS; ++i)
    {
        lightColors[i] = m_lights[i].color;
        lightPositions[i] = m_lights[i].position;
    }

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
// UpdateLights — пересчитать позиции привязанных источников света
//
// НЕДЕЛЯ 3: читает позицию из TransformComponent напрямую (не из GameObject)
// ============================================================================
void SceneManager::UpdateLights()
{
    for (auto& light : m_lights)
    {
        if (!light.active || light.attachedToObjectIndex < 0) continue;

        // Читаем позицию объекта из ECS (не из m_objects!)
        Entity e = static_cast<Entity>(light.attachedToObjectIndex);
        auto* transform = m_registry.GetComponent<TransformComponent>(e);
        if (!transform) continue;

        light.position = XMFLOAT4(
            transform->position.x + light.offset.x,
            transform->position.y + light.offset.y,
            transform->position.z + light.offset.z,
            1.0f);
    }
}

// ============================================================================
// SetLight / AttachLightToObject / DetachLight
// ============================================================================
void SceneManager::SetLight(int index, const XMFLOAT3& pos, const XMFLOAT4& color, bool active)
{
    if (index < 0 || index >= NUM_LIGHTS) return;
    m_lights[index].position = XMFLOAT4(pos.x, pos.y, pos.z, 1.0f);
    m_lights[index].color = color;
    m_lights[index].active = active;
}

void SceneManager::AttachLightToObject(int lightIndex, int objectIndex, const XMFLOAT3& offset)
{
    if (lightIndex < 0 || lightIndex >= NUM_LIGHTS) return;
    m_lights[lightIndex].attachedToObjectIndex = objectIndex;
    m_lights[lightIndex].offset = offset;
}

void SceneManager::DetachLight(int lightIndex)
{
    if (lightIndex < 0 || lightIndex >= NUM_LIGHTS) return;
    m_lights[lightIndex].attachedToObjectIndex = -1;
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

void SceneManager::RenderLightSpheres(
    std::vector<GameObject>& spheres,
    ID3D11DeviceContext* context,
    XMMATRIX view, XMMATRIX projection, TextureShaderClass* shader)
{
    for (size_t i = 0; i < spheres.size() && i < NUM_LIGHTS; ++i)
    {
        if (!m_lights[i].active) continue;
        spheres[i].SetPosition(m_lights[i].position.x, m_lights[i].position.y, m_lights[i].position.z);
        spheres[i].RenderSimple(context, view, projection, shader);
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

    if (j.contains("lights") && j["lights"].is_array())
    {
        for (const auto& lightJson : j["lights"])
        {
            int index = lightJson.value("index", -1);
            if (index >= 0 && index < NUM_LIGHTS)
                DeserializeLight(lightJson, m_lights[index]);
        }
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

    // Назначаем defaultMaterial сразу — объект не будет рендериться с nullptr
    if (m_defaultMaterial)
    {
        if (auto* mesh = m_registry.GetComponent<MeshRendererComponent>(e))
            mesh->material = m_defaultMaterial;
    }

    return e;
}
json SceneManager::SerializeLight(const LightSource& light, int index) { return json(); }
void SceneManager::DeserializeLight(const json& j, LightSource& light) {}
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

    if (!mesh->material)
    {
        if (!device) return;

        auto* mat = new MaterialClass();
        mat->Initialize(device, L"RuntimeMaterial");
        if (m_defaultMaterial && m_defaultMaterial->GetMainTexture())
            mat->SetMainTexture(m_defaultMaterial->GetMainTexture());
        mesh->material = mat;
    }

    mesh->material->SetShader(shader);

    // Если для light-шейдера нет основной текстуры — подставляем default, чтобы не было "бежевого" результата.
    if (shader && shader->GetType() == ShaderType::Light)
    {
        if (!mesh->material->GetMainTexture() && m_defaultMaterial && m_defaultMaterial->GetMainTexture())
        {
            mesh->material->SetMainTexture(m_defaultMaterial->GetMainTexture());
            OutputDebugStringA("SetObjectShader: Light shader had no main texture — assigned default main texture.\n");
        }
    }

    // Glass-шейдер требует refractionSRV + normalTexture (slot 1)
    // Если они не назначены — подключаем автоматически
   

    
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