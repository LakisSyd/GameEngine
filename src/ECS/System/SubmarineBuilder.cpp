#include "SubmarineBuilder.h"
#include "Components.h"
#include "PhysicsSystem.h"
#include "SubmarineJointSystem.h"

// ══════════════════════════════════════════════════════════════════════════════
// Создать контейнер подлодки
// ══════════════════════════════════════════════════════════════════════════════
Entity SubmarineBuilder::CreateSubmarine(ECSRegistry& registry, const std::string& name,
    const XMFLOAT3& position)
{
    Entity submarine = registry.CreateEntity();

    // Трансформ (корневая позиция)
    TransformComponent transform{};
    transform.position = position;
    registry.AddComponent<TransformComponent>(submarine, transform);

    // Компонент подлодки
    SubmarineComponent subComp{};
    subComp.submarineRoot = submarine;
    registry.AddComponent<SubmarineComponent>(submarine, subComp);

    // Активный тег
    registry.AddComponent<ActiveComponent>(submarine, {});

    char buf[256];
    sprintf_s(buf, "[SubmarineBuilder] Created submarine '%s' (Entity %u)\n",
        name.c_str(), submarine);
    OutputDebugStringA(buf);

    return submarine;
}

// ══════════════════════════════════════════════════════════════════════════════
// Добавить отсек к подлодке
// ══════════════════════════════════════════════════════════════════════════════
Entity SubmarineBuilder::AddCompartment(ECSRegistry& registry,
    Entity submarine,
    AssimpModelClass* model,
    const XMFLOAT3& localPosition,
    const XMFLOAT3& localSize,
    float mass)
{
    auto* subComp = registry.GetComponent<SubmarineComponent>(submarine);
    auto* subTransform = registry.GetComponent<TransformComponent>(submarine);
    if (!subComp || !subTransform)
    {
        OutputDebugStringA("[SubmarineBuilder] ERROR: Invalid submarine entity\n");
        return INVALID_ENTITY;
    }

    // Вычисляем мировую позицию отсека
    XMFLOAT3 worldPos = {
        subTransform->position.x + localPosition.x,
        subTransform->position.y + localPosition.y,
        subTransform->position.z + localPosition.z
    };

    // ── 1. Создаём сущность отсека ────────────────────────────────────────────
    Entity compartment = registry.CreateEntity();

    TransformComponent transform{};
    transform.position = worldPos;
    transform.scale = { 1, 1, 1 };
    registry.AddComponent<TransformComponent>(compartment, transform);

    // ── 2. Mesh Renderer ───────────────────────────────────────────────────────
    MeshRendererComponent meshRenderer{};
    meshRenderer.model = model;
    meshRenderer.material = nullptr; // Будет назначен позже
    registry.AddComponent<MeshRendererComponent>(compartment, meshRenderer);

    // ── 3. RigidBody (физика) ──────────────────────────────────────────────────
    RigidBodyComponent rb{};
    rb.mass = mass;
    rb.shapeType = RigidBodyComponent::Box;
    rb.shapeSize = btVector3(localSize.x * 0.5f, localSize.y * 0.5f, localSize.z * 0.5f);
    rb.friction = 0.5f;
    rb.linearDamping = 0.3f;
    rb.angularDamping = 0.5f;
    registry.AddComponent<RigidBodyComponent>(compartment, rb);
    PhysicsSystem::CreateRigidBody(registry, compartment);

    // ── 4. Compartment (отсек) ─────────────────────────────────────────────────
    CompartmentComponent comp{};
    comp.localMin = { -localSize.x * 0.5f, -localSize.y * 0.5f, -localSize.z * 0.5f };
    comp.localMax = { localSize.x * 0.5f,  localSize.y * 0.5f,  localSize.z * 0.5f };
    comp.RecalculateVolume();
    comp.hasPower = true;
    comp.isSealed = true;
    comp.pumpRate = 0.2f; // 200 л/с
    registry.AddComponent<CompartmentComponent>(compartment, comp);

    // ── 5. Buoyancy (плавучесть) ───────────────────────────────────────────────
    BuoyancyComponent buoy{};
    buoy.volume = comp.volume;
    buoy.height = localSize.y;
    buoy.fluidDensity = 1025.0f; // Морская вода
    buoy.buoyancyMultiplier = 1.0f;
    registry.AddComponent<BuoyancyComponent>(compartment, buoy);

    // ── 6. Pressure Hull (корпус под давлением) ────────────────────────────────
    PressureHullComponent hull{};
    hull.maxPressure = 300000.0f; // 3 атм (~20м)
    hull.currentHealth = 100.0f;
    hull.repairRate = 2.0f;
    registry.AddComponent<PressureHullComponent>(compartment, hull);

    // ── 7. Submarine Component (привязка к подлодке) ───────────────────────────
    SubmarineComponent compartmentSubComp{};
    compartmentSubComp.submarineRoot = submarine;
    compartmentSubComp.compartmentIndex = (int)subComp->allCompartments.size();
    compartmentSubComp.isDetached = false;
    registry.AddComponent<SubmarineComponent>(compartment, compartmentSubComp);

    // ── 8. Активный тег ────────────────────────────────────────────────────────
    registry.AddComponent<ActiveComponent>(compartment, {});

    // ── 9. Добавляем в список отсеков подлодки ────────────────────────────────
    subComp->allCompartments.push_back(compartment);

    char buf[256];
    sprintf_s(buf, "[SubmarineBuilder] Added compartment (Entity %u) at (%.1f, %.1f, %.1f)\n",
        compartment, worldPos.x, worldPos.y, worldPos.z);
    OutputDebugStringA(buf);

    return compartment;
}

// ══════════════════════════════════════════════════════════════════════════════
// Соединить два отсека
// ══════════════════════════════════════════════════════════════════════════════
void SubmarineBuilder::ConnectCompartments(ECSRegistry& registry,
    Entity compartmentA, Entity compartmentB,
    const btVector3& pivotA, const btVector3& pivotB,
    float breakForce)
{
    // Добавляем друг друга в список соседей (для перетекания воды)
    auto* compA = registry.GetComponent<CompartmentComponent>(compartmentA);
    auto* compB = registry.GetComponent<CompartmentComponent>(compartmentB);

    if (compA && compB)
    {
        compA->connectedCompartments.push_back(compartmentB);
        compB->connectedCompartments.push_back(compartmentA);
    }

    // Создаём физическое соединение
    SubmarineJointSystem::CreateJoint(registry, compartmentA, compartmentB,
        pivotA, pivotB, breakForce);

    OutputDebugStringA("[SubmarineBuilder] Connected compartments\n");
}
// В SubmarineBuilder.cpp:

CompartmentGeometry SubmarineBuilder::CalculateGeometryFromModel(AssimpModelClass* model)
{
    if (!model) return {};

    XMFLOAT3 min, max;
    model->GetBoundingBox(&min, &max);

    CompartmentGeometry geom;
    geom.size = {
        max.x - min.x,
        max.y - min.y,
        max.z - min.z
    };
    geom.center = {
        (min.x + max.x) * 0.5f,
        (min.y + max.y) * 0.5f,
        (min.z + max.z) * 0.5f
    };
    geom.volume = geom.size.x * geom.size.y * geom.size.z;

    char buf[256];
    sprintf_s(buf, "[Geometry] Size: %.2f x %.2f x %.2f, Volume: %.2f m³\n",
        geom.size.x, geom.size.y, geom.size.z, geom.volume);
    OutputDebugStringA(buf);

    return geom;
}

// ══════════════════════════════════════════════════════════════════════════════
// 🔹 ПРЕФАБ: Базовая подлодка (3 отсека)
// ══════════════════════════════════════════════════════════════════════════════
Entity SubmarineBuilder::CreateBasicSubmarine(ECSRegistry& registry,
    AssimpModelClass* compartmentModel,
    const XMFLOAT3& spawnPos)
{
    OutputDebugStringA("\n════════════════════════════════════════════════════════\n");
    OutputDebugStringA("     🚢 CREATING BASIC SUBMARINE (3 compartments)\n");
    OutputDebugStringA("════════════════════════════════════════════════════════\n\n");

    // Создаём контейнер подлодки
    Entity submarine = CreateSubmarine(registry, "Basic Submarine", spawnPos);

    // Размеры отсека
    XMFLOAT3 compSize = { 2.0f, 2.0f, 3.0f }; // Ширина x Высота x Длина
    float spacing = 3.0f; // Расстояние между центрами отсеков

    // ── НОСОВОЙ ОТСЕК (Bow) ───────────────────────────────────────────────────
    Entity bow = AddCompartment(registry, submarine, compartmentModel,
        XMFLOAT3(-spacing, 0, 0), compSize, 80.0f);

    // ── ЦЕНТРАЛЬНЫЙ ОТСЕК (Main) ───────────────────────────────────────────────
    Entity main = AddCompartment(registry, submarine, compartmentModel,
        XMFLOAT3(0, 0, 0), compSize, 120.0f);

    // ── КОРМОВОЙ ОТСЕК (Stern) ─────────────────────────────────────────────────
    Entity stern = AddCompartment(registry, submarine, compartmentModel,
        XMFLOAT3(spacing, 0, 0), compSize, 100.0f);

    // ── СОЕДИНЕНИЯ ─────────────────────────────────────────────────────────────
    // Носовой ↔ Центральный
    ConnectCompartments(registry, bow, main,
        btVector3(compSize.z * 0.5f, 0, 0),   // Правая сторона bow
        btVector3(-compSize.z * 0.5f, 0, 0),  // Левая сторона main
        150000.0f);

    // Центральный ↔ Кормовой
    ConnectCompartments(registry, main, stern,
        btVector3(compSize.z * 0.5f, 0, 0),
        btVector3(-compSize.z * 0.5f, 0, 0),
        150000.0f);

    OutputDebugStringA("\n✅ Basic submarine created successfully!\n\n");

    return submarine;
}

// ══════════════════════════════════════════════════════════════════════════════
// 🔹 ПРЕФАБ: Большая подлодка (5 отсеков)
// ══════════════════════════════════════════════════════════════════════════════
Entity SubmarineBuilder::CreateLargeSubmarine(ECSRegistry& registry,
    AssimpModelClass* compartmentModel,
    const XMFLOAT3& spawnPos)
{
    OutputDebugStringA("\n════════════════════════════════════════════════════════\n");
    OutputDebugStringA("     🚢 CREATING LARGE SUBMARINE (5 compartments)\n");
    OutputDebugStringA("════════════════════════════════════════════════════════\n\n");

    Entity submarine = CreateSubmarine(registry, "Large Submarine", spawnPos);

    XMFLOAT3 compSize = { 2.5f, 2.5f, 3.5f };
    float spacing = 3.5f;

    // ── 5 ОТСЕКОВ В РЯД ────────────────────────────────────────────────────────
    Entity comp1 = AddCompartment(registry, submarine, compartmentModel,
        XMFLOAT3(-spacing * 2, 0, 0), compSize, 100.0f);

    Entity comp2 = AddCompartment(registry, submarine, compartmentModel,
        XMFLOAT3(-spacing, 0, 0), compSize, 120.0f);

    Entity comp3 = AddCompartment(registry, submarine, compartmentModel,
        XMFLOAT3(0, 0, 0), compSize, 150.0f);

    Entity comp4 = AddCompartment(registry, submarine, compartmentModel,
        XMFLOAT3(spacing, 0, 0), compSize, 120.0f);

    Entity comp5 = AddCompartment(registry, submarine, compartmentModel,
        XMFLOAT3(spacing * 2, 0, 0), compSize, 100.0f);

    // ── СОЕДИНЕНИЯ ─────────────────────────────────────────────────────────────
    ConnectCompartments(registry, comp1, comp2,
        btVector3(compSize.z * 0.5f, 0, 0),
        btVector3(-compSize.z * 0.5f, 0, 0), 200000.0f);

    ConnectCompartments(registry, comp2, comp3,
        btVector3(compSize.z * 0.5f, 0, 0),
        btVector3(-compSize.z * 0.5f, 0, 0), 250000.0f); // Центр прочнее

    ConnectCompartments(registry, comp3, comp4,
        btVector3(compSize.z * 0.5f, 0, 0),
        btVector3(-compSize.z * 0.5f, 0, 0), 250000.0f);

    ConnectCompartments(registry, comp4, comp5,
        btVector3(compSize.z * 0.5f, 0, 0),
        btVector3(-compSize.z * 0.5f, 0, 0), 200000.0f);

    OutputDebugStringA("\n✅ Large submarine created successfully!\n\n");

    return submarine;
}

// ══════════════════════════════════════════════════════════════════════════════
// Отделить отсек (когда джойнт сломался)
// ══════════════════════════════════════════════════════════════════════════════
void SubmarineBuilder::DetachCompartment(ECSRegistry& registry, Entity compartment)
{
    auto* subComp = registry.GetComponent<SubmarineComponent>(compartment);
    if (!subComp) return;

    subComp->isDetached = true;

    // Убираем из списка соседей
    auto* comp = registry.GetComponent<CompartmentComponent>(compartment);
    if (comp)
    {
        for (Entity neighbor : comp->connectedCompartments)
        {
            auto* neighborComp = registry.GetComponent<CompartmentComponent>(neighbor);
            if (neighborComp)
            {
                auto& list = neighborComp->connectedCompartments;
                list.erase(std::remove(list.begin(), list.end(), compartment), list.end());
            }
        }
        comp->connectedCompartments.clear();
    }

    OutputDebugStringA("[SubmarineBuilder] Compartment detached\n");
}

// ══════════════════════════════════════════════════════════════════════════════
// Получить все отсеки подлодки
// ══════════════════════════════════════════════════════════════════════════════
std::vector<Entity> SubmarineBuilder::GetAllCompartments(ECSRegistry& registry, Entity submarine)
{
    auto* subComp = registry.GetComponent<SubmarineComponent>(submarine);
    if (!subComp) return {};

    return subComp->allCompartments;
}

// ══════════════════════════════════════════════════════════════════════════════
// Проверить целостность подлодки
// ══════════════════════════════════════════════════════════════════════════════
bool SubmarineBuilder::CheckIntegrity(ECSRegistry& registry, Entity submarine)
{
    auto compartments = GetAllCompartments(registry, submarine);

    for (Entity comp : compartments)
    {
        auto* subComp = registry.GetComponent<SubmarineComponent>(comp);
        if (subComp && subComp->isDetached)
            return false; // Есть оторванные отсеки
    }

    return true; // Все отсеки на месте
}
void SubmarineBuilder::ConnectCompartmentsAuto(ECSRegistry& registry,
    Entity compartmentA, Entity compartmentB,
    float breakForce, float connectionMargin)
{
    auto* transformA = registry.GetComponent<TransformComponent>(compartmentA);
    auto* transformB = registry.GetComponent<TransformComponent>(compartmentB);
    auto* rbA = registry.GetComponent<RigidBodyComponent>(compartmentA);
    auto* rbB = registry.GetComponent<RigidBodyComponent>(compartmentB);

    if (!transformA || !transformB || !rbA || !rbB)
    {
        OutputDebugStringA("[ConnectCompartmentsAuto] ERROR: Missing components\n");
        return;
    }

    // 1. Вычисляем вектор от A к B
    btVector3 posA(transformA->position.x, transformA->position.y, transformA->position.z);
    btVector3 posB(transformB->position.x, transformB->position.y, transformB->position.z);
    btVector3 direction = posB - posA;
    direction.normalize();

    // 2. Определяем главную ось соединения
    btVector3 pivotA, pivotB;
    float absX = fabs(direction.x());
    float absY = fabs(direction.y());
    float absZ = fabs(direction.z());

    // 3. Выбираем ось с максимальным отклонением
    if (absX > absY && absX > absZ)
    {
        // Соединение по оси X
        float halfSizeA = rbA->shapeSize.x(); // половина ширины
        float halfSizeB = rbB->shapeSize.x();

        pivotA.setValue(direction.x() > 0 ? halfSizeA : -halfSizeA, 0, 0);
        pivotB.setValue(direction.x() > 0 ? -halfSizeB : halfSizeB, 0, 0);
    }
    else if (absZ > absX && absZ > absY)
    {
        // Соединение по оси Z
        float halfSizeA = rbA->shapeSize.z(); // половина длины
        float halfSizeB = rbB->shapeSize.z();

        pivotA.setValue(0, 0, direction.z() > 0 ? halfSizeA : -halfSizeA);
        pivotB.setValue(0, 0, direction.z() > 0 ? -halfSizeB : halfSizeB);
    }
    else
    {
        // Соединение по оси Y (вертикальное)
        float halfSizeA = rbA->shapeSize.y(); // половина высоты
        float halfSizeB = rbB->shapeSize.y();

        pivotA.setValue(0, direction.y() > 0 ? halfSizeA : -halfSizeA, 0);
        pivotB.setValue(0, direction.y() > 0 ? -halfSizeB : halfSizeB, 0);
    }

    // 4. Добавляем небольшой зазор если нужно
    if (connectionMargin > 0)
    {
        pivotA += direction * connectionMargin;
        pivotB -= direction * connectionMargin;
    }

    // 5. Создаём соединение
    ConnectCompartments(registry, compartmentA, compartmentB,
        pivotA, pivotB, breakForce);

    // Отладка
    char buf[512];
    sprintf_s(buf,
        "[ConnectCompartmentsAuto] Connected %u ↔ %u\n"
        "  Direction: (%.2f, %.2f, %.2f)\n"
        "  Pivot A (local): (%.3f, %.3f, %.3f)\n"
        "  Pivot B (local): (%.3f, %.3f, %.3f)\n",
        compartmentA, compartmentB,
        direction.x(), direction.y(), direction.z(),
        pivotA.x(), pivotA.y(), pivotA.z(),
        pivotB.x(), pivotB.y(), pivotB.z());
    OutputDebugStringA(buf);
}