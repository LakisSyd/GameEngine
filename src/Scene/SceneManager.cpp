#include "SceneManager.h"

SceneManager::SceneManager()
    : m_defaultMaterial(nullptr)  // ← Добавили
{
    m_objects.reserve(64);
    for (auto& light : m_lights) {
        light.active = false;
        light.color = XMFLOAT4(0, 0, 0, 1);
    }
}

SceneManager::~SceneManager()
{
    Clear();
}

// === Спавн объекта ===
int SceneManager::SpawnObject(AssimpModelClass* model, float x, float y, float z)
{
    GameObject newObj;
    newObj.Initialize(model, x, y, z);
    m_objects.push_back(newObj);
    return static_cast<int>(m_objects.size()) - 1;
}

// === Удаление объекта ===
bool SceneManager::RemoveObject(int index)
{
    if (index < 0 || index >= static_cast<int>(m_objects.size()))
        return false;

    // Если объект удалён, отвязываем от него свет
    for (auto& light : m_lights) {
        if (light.attachedToObjectIndex == index) {
            light.attachedToObjectIndex = -1;
        }
    }

    m_objects.erase(m_objects.begin() + index);
    return true;
}

void SceneManager::Clear()
{
    m_objects.clear();
}

GameObject* SceneManager::GetObject(int index)
{
    if (index < 0 || index >= static_cast<int>(m_objects.size()))
        return nullptr;
    return &m_objects[index];
}

bool SceneManager::SaveScene(const std::string& filename)
{
    json scene;

    // 1. Метаданные
    scene["scene_name"] = "MyLevel";
    scene["version"] = 1;

    // 2. Массив объектов
    scene["objects"] = json::array();

    // 3. Сериализация КАЖДОГО объекта (передаём ID явно)
    for (size_t i = 0; i < m_objects.size(); ++i)
    {
        json objJson = SerializeObject(m_objects[i], static_cast<int>(i));
        scene["objects"].push_back(objJson);  // ← Добавляем ОДИН объект в массив
    }

    // 4. Запись в файл
    std::ofstream file(filename);
    if (!file.is_open()) return false;

    file << scene.dump(4);
    return true;
}

bool SceneManager::LoadScene(const std::string& filename,
    ID3D11Device* device,
    ID3D11DeviceContext* deviceContext)  // ✅ ДОБАВЛЕНО
{
    OutputDebugStringA(("🔍 LoadScene: Trying to open: " + filename + "\n").c_str());

    if (filename.empty()) {
        OutputDebugStringA("❌ LoadScene: filename is empty!\n");
        return false;
    }

    std::ifstream file(filename);
    if (!file.is_open()) {
        OutputDebugStringA(("❌ LoadScene: Failed to open file: " + filename + "\n").c_str());
        char cwd[MAX_PATH];
        if (GetCurrentDirectoryA(MAX_PATH, cwd)) {
            OutputDebugStringA(("📁 Current directory: " + std::string(cwd) + "\n").c_str());
        }
        return false;
    }
    OutputDebugStringA("✅ LoadScene: File opened successfully\n");

    json j;
    try {
        file >> j;
        OutputDebugStringA(("✅ LoadScene: JSON parsed, objects count: " +
            std::to_string(j["objects"].size()) + "\n").c_str());
    }
    catch (const json::exception& e) {
        OutputDebugStringA(("❌ LoadScene: JSON parse error: " + std::string(e.what()) + "\n").c_str());
        file.close();
        return false;
    }
    file.close();

    OutputDebugStringA("🧹 LoadScene: Clearing old scene...\n");
    Clear();

    if (j.contains("objects") && j["objects"].is_array()) {
        OutputDebugStringA(("📦 LoadScene: Loading " + std::to_string(j["objects"].size()) + " objects\n").c_str());

        for (const auto& objJson : j["objects"]) {
            OutputDebugStringA("🔄 LoadScene: Deserializing one object...\n");
            GameObject obj = DeserializeObject(objJson, device, deviceContext);  // ✅ ПЕРЕДАЁМ
            m_objects.push_back(obj);
        }
        OutputDebugStringA(("✅ LoadScene: Loaded " + std::to_string(m_objects.size()) + " objects\n").c_str());
    }

    if (j.contains("lights") && j["lights"].is_array()) {
        OutputDebugStringA(("💡 LoadScene: Loading " + std::to_string(j["lights"].size()) + " lights\n").c_str());
        for (const auto& lightJson : j["lights"]) {
            int index = lightJson.value("index", -1);
            if (index >= 0 && index < NUM_LIGHTS) {
                DeserializeLight(lightJson, m_lights[index]);
            }
        }
    }

    OutputDebugStringA("🎉 LoadScene: FINISHED SUCCESSFULLY\n");
    return true;
}

// === 🔹 НАСТРОЙКА ИСТОЧНИКА СВЕТА ===
void SceneManager::SetLight(int index, const XMFLOAT3& pos, const XMFLOAT4& color, bool active)
{
    if (index < 0 || index >= NUM_LIGHTS) return;

    m_lights[index].position = XMFLOAT4(pos.x, pos.y, pos.z, 1.0f);
    m_lights[index].color = color;
    m_lights[index].active = active;
}

// === 🔹 ПРИВЯЗКА СВЕТА К ОБЪЕКТУ ===
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

// === 🔹 ОБНОВЛЕНИЕ ПОЗИЦИЙ СВЕТА (перед рендером!) ===
void SceneManager::UpdateLights()
{
    for (auto& light : m_lights)
    {
        if (light.active && light.attachedToObjectIndex >= 0)
        {
            GameObject* obj = GetObject(light.attachedToObjectIndex);
            if (obj)
            {
                XMFLOAT3 objPos = obj->GetPosition();
                light.position = XMFLOAT4(
                    objPos.x + light.offset.x,
                    objPos.y + light.offset.y,
                    objPos.z + light.offset.z,
                    1.0f
                );
            }
        }
    }
}

int SceneManager::SpawnSprite(ID3D11Device* device, ID3D11DeviceContext* deviceContext, const char* textureName, float x, float y, float z, float width, float height)
{
    auto sprite = std::make_unique<WorldSpriteClass>();

    if (!sprite->Initialize(device, deviceContext, textureName, width, height))
    {
        OutputDebugStringA("FILED TO INITIALIZE WORLDSPRITE");
        return -1;
    }
    sprite->SetPosition(x, y, z);
    m_sprites.push_back(std::move(sprite));

    return static_cast<int>(m_sprites.size());
}

// === Обновление всех объектов ===
void SceneManager::UpdateAll(float deltaTime)
{
    for (auto& obj : m_objects)
    {
        obj.Update(deltaTime);
    }
}

void SceneManager::RenderAll(
    ID3D11DeviceContext* context,
    ID3D11Device* device,
    XMMATRIX viewMatrix,
    XMMATRIX projectionMatrix,
    MaterialClass* material,  // ← Теперь MaterialClass*
    XMFLOAT4 lightColors[],
    XMFLOAT4 lightPositions[],
    int selectedObjectIndex)
{
    // Копируем данные света в массивы для шейдера
    for (int i = 0; i < NUM_LIGHTS; ++i) {
        lightColors[i] = m_lights[i].color;
        lightPositions[i] = m_lights[i].position;
    }

    // Рендерим объекты
    int index = 0;
    for (auto& obj : m_objects)
    {
        // 🔥 Если у объекта есть свой материал — используем его, иначе — переданный
        auto* objMaterial = obj.GetMaterial();
        auto* renderMaterial = objMaterial ? objMaterial : material;

        obj.Render(context, device, viewMatrix, projectionMatrix,
            renderMaterial,  // ← Передаём материал
            lightColors, lightPositions,
            index == selectedObjectIndex);
        index++;
    }
}


// === 🔹 Настройка сфер-индикаторов света ===
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

// === 🔹 Рендер сфер-индикаторов ===
void SceneManager::RenderLightSpheres(
    std::vector<GameObject>& spheres,
    ID3D11DeviceContext* context,
    XMMATRIX view,
    XMMATRIX projection,
    TextureShaderClass* shader)
{
    for (size_t i = 0; i < spheres.size() && i < NUM_LIGHTS; ++i)
    {
        if (!m_lights[i].active) continue;

        // Синхронизируем позицию сферы с источником света
        spheres[i].SetPosition(
            m_lights[i].position.x,
            m_lights[i].position.y,
            m_lights[i].position.z
        );

        spheres[i].RenderSimple(context, view, projection, shader);
    }
}

json SceneManager::SerializeObject(const GameObject& obj, int id)
{
    json objJson;

    // Простые поля
    objJson["id"] = id;
    objJson["model_path"] = "sphere.obj";  // Заглушка: в будущем бери из модели

    objJson["texture_path"] = "stone.tga";  // или получите из obj.GetModel()->GetTexturePath()

    // 🔥 Правильная сериализация позиции (ОБЪЕКТ, а не массив!)
    XMFLOAT3 pos = obj.GetPosition();
    objJson["position"] = {
        {"x", pos.x},
        {"y", pos.y},
        {"z", pos.z}
    };

    // Остальные поля (заглушки для начала)
    objJson["rotation"] = { {"x", 0.0f}, {"y", 0.0f}, {"z", 0.0f} };
    objJson["scale"] = { {"x", 1.0f}, {"y", 1.0f}, {"z", 1.0f} };

    return objJson;  // ← Возвращаем ОДИН объект, а не массив!
}

GameObject SceneManager::DeserializeObject(const json& j,
    ID3D11Device* device,
    ID3D11DeviceContext* deviceContext)
{
    GameObject obj;

    std::string modelPath = j.value("model_path", "sphere.obj");
    std::string texturePath = j.value("texture_path", "stone.tga");

    AssimpModelClass* model = GetOrCreateModel(modelPath, device, deviceContext, texturePath);

    if (!model) {
        OutputDebugStringA("⚠️  DeserializeObject: model is nullptr, using fallback\n");
        model = m_mainModelFallback;
        if (!model) return GameObject();
    }

    float x = 0.0f, y = 0.0f, z = 0.0f;
    if (j.contains("position")) {
        x = j["position"].value("x", 0.0f);
        y = j["position"].value("y", 0.0f);
        z = j["position"].value("z", 0.0f);
    }

    obj.Initialize(model, x, y, z);

    if (j.contains("rotation")) {
        obj.SetRotation(
            j["rotation"].value("x", 0.0f),
            j["rotation"].value("y", 0.0f),
            j["rotation"].value("z", 0.0f)
        );
    }
    if (j.contains("scale")) {
        obj.SetScale(
            j["scale"].value("x", 1.0f),
            j["scale"].value("y", 1.0f),
            j["scale"].value("z", 1.0f)
        );
    }

    return obj;
}

json SceneManager::SerializeLight(const LightSource& light, int index)
{
    return json();
}

void SceneManager::DeserializeLight(const json& j, LightSource& light)
{
}

json SceneManager::SerializeMaterial(const MaterialClass* mat)
{
    return json();
}


bool SceneManager::RemoveSprite(int index)
{
    if (index < 0 || index >= static_cast<int>(m_sprites.size()))
        return false;
    m_sprites.erase(m_sprites.begin() + index);
    return true;
}

WorldSpriteClass* SceneManager::GetSprite(int index)
{
    if (index < 0 || index >= static_cast<int>(m_sprites.size()))
        return nullptr;
    return m_sprites[index].get();
}

void SceneManager::RenderSprites(
    ID3D11DeviceContext* context,
    XMMATRIX view,
    XMMATRIX projection,
    TextureShaderClass* shader)
{
    for (auto& sprite : m_sprites)
    {
        sprite->Render(context, view, projection, shader);
        // Если спрайт должен использовать свою внутреннюю позицию,
        // не передаём customWorldMatrix (оставляем nullptr).
        // При необходимости можно передать кастомную матрицу через отдельный метод.
    }
}



AssimpModelClass * SceneManager::GetOrCreateModel(
    const std::string & path,
    ID3D11Device * device,
    ID3D11DeviceContext * deviceContext,
    const std::string & texturePath)
{
    if (path.empty()) {
        OutputDebugStringA("❌ GetOrCreateModel: empty path!\n");
        return m_mainModelFallback;
    }

    auto it = m_modelCache.find(path);
    if (it != m_modelCache.end() && it->second != nullptr) {
        return it->second;
    }

    AssimpModelClass* model = new AssimpModelClass();

    if (GetFileAttributesA(path.c_str()) == INVALID_FILE_ATTRIBUTES) {
        OutputDebugStringA(("⚠️  GetOrCreateModel: File not found: " + path + "\n").c_str());
        delete model;
        return m_mainModelFallback;
    }

    const char* texPath = nullptr;
    if (!texturePath.empty()) {
        texPath = texturePath.c_str();
    }

    // 🔥 ПЕРЕДАЁМ deviceContext ВМЕСТО nullptr!
    if (model->Initialize(device, deviceContext, path.c_str(), texPath)) {
        m_modelCache[path] = model;
        return model;
    }

    OutputDebugStringA(("❌ GetOrCreateModel: Initialize failed for: " + path + "\n").c_str());
    delete model;
    return m_mainModelFallback;
}