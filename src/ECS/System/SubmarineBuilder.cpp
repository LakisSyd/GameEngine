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

    XMFLOAT3 worldPos = {
        subTransform->position.x + localPosition.x,
        subTransform->position.y + localPosition.y,
        subTransform->position.z + localPosition.z
    };

    Entity compartment = registry.CreateEntity();

    TransformComponent transform{};
    transform.position = worldPos;
    transform.scale = { 1, 1, 1 };
    registry.AddComponent<TransformComponent>(compartment, transform);

    // ── Mesh Renderer ──────────────────────────────────────────────────────────
    MeshRendererComponent meshRenderer{};
    meshRenderer.model = model;
    meshRenderer.material = nullptr;
    registry.AddComponent<MeshRendererComponent>(compartment, meshRenderer);

    // ── CompartmentComponent ───────────────────────────────────────────────────
    CompartmentComponent comp{};
    comp.localMin = { -localSize.x * 0.5f, -localSize.y * 0.5f, -localSize.z * 0.5f };
    comp.localMax = { localSize.x * 0.5f,  localSize.y * 0.5f,  localSize.z * 0.5f };
    comp.localCenter = localPosition;  // ✅ сохраняем локальный offset
    comp.RecalculateVolume();
    comp.hasPower = true;
    comp.isSealed = true;
    comp.pumpRate = 0.2f;
    registry.AddComponent<CompartmentComponent>(compartment, comp);

    // ── SubmarineComponent (привязка к корню) ──────────────────────────────────
    SubmarineComponent compartmentSubComp{};
    compartmentSubComp.submarineRoot = submarine;
    compartmentSubComp.compartmentIndex = (int)subComp->allCompartments.size();
    compartmentSubComp.isDetached = false;
    registry.AddComponent<SubmarineComponent>(compartment, compartmentSubComp);

    // ── PressureHull ───────────────────────────────────────────────────────────
    PressureHullComponent hull{};
    hull.maxPressure = 300000.0f;
    hull.currentHealth = 100.0f;
    hull.repairRate = 2.0f;
    registry.AddComponent<PressureHullComponent>(compartment, hull);

    // ── Активный тег ──────────────────────────────────────────────────────────
    registry.AddComponent<ActiveComponent>(compartment, {});

    // ── Добавляем в список подлодки ────────────────────────────────────────────
    subComp->allCompartments.push_back(compartment);

    // ══════════════════════════════════════════════════════════════════════════
    // ✅ ИСПРАВЛЕНИЕ: отсек НЕ получает свой RigidBody и BuoyancyComponent!
    //    Физика и плавучесть — только у корневой Entity подлодки.
    //    Отсек — это логическая единица (вода, давление, герметичность),
    //    а не отдельное физическое тело.
    //    Корень берёт суммарный объём всех отсеков в RecalculateSubmarineMass().
    // ══════════════════════════════════════════════════════════════════════════

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

    Entity submarine = CreateSubmarine(registry, "Basic Submarine", spawnPos);

    XMFLOAT3 compSize = { 2.0f, 2.0f, 3.0f };
    float spacing = 3.0f;

    Entity bow  = AddCompartment(registry, submarine, compartmentModel,
        XMFLOAT3(-spacing, 0, 0), compSize, 80.0f);
    Entity main = AddCompartment(registry, submarine, compartmentModel,
        XMFLOAT3(0, 0, 0), compSize, 120.0f);
    Entity stern = AddCompartment(registry, submarine, compartmentModel,
        XMFLOAT3(spacing, 0, 0), compSize, 100.0f);

    ConnectCompartments(registry, bow, main,
        btVector3(compSize.z * 0.5f, 0, 0),
        btVector3(-compSize.z * 0.5f, 0, 0), 150000.0f);
    ConnectCompartments(registry, main, stern,
        btVector3(compSize.z * 0.5f, 0, 0),
        btVector3(-compSize.z * 0.5f, 0, 0), 150000.0f);

    // ══════════════════════════════════════════════════════════════════════
    // ✅ КРИТИЧНО: Добавляем физику ПОСЛЕ всех отсеков — volume уже известен!
    // ══════════════════════════════════════════════════════════════════════
    FinalizeSubmarinePhysics(registry, submarine);

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

    ConnectCompartments(registry, comp1, comp2,
        btVector3(compSize.z * 0.5f, 0, 0),
        btVector3(-compSize.z * 0.5f, 0, 0), 200000.0f);
    ConnectCompartments(registry, comp2, comp3,
        btVector3(compSize.z * 0.5f, 0, 0),
        btVector3(-compSize.z * 0.5f, 0, 0), 250000.0f);
    ConnectCompartments(registry, comp3, comp4,
        btVector3(compSize.z * 0.5f, 0, 0),
        btVector3(-compSize.z * 0.5f, 0, 0), 250000.0f);
    ConnectCompartments(registry, comp4, comp5,
        btVector3(compSize.z * 0.5f, 0, 0),
        btVector3(-compSize.z * 0.5f, 0, 0), 200000.0f);

    // ══════════════════════════════════════════════════════════════════════
    // ✅ КРИТИЧНО: Добавляем физику ПОСЛЕ всех отсеков!
    // ══════════════════════════════════════════════════════════════════════
    FinalizeSubmarinePhysics(registry, submarine);

    OutputDebugStringA("\n✅ Large submarine created successfully!\n\n");
    return submarine;
}

// ══════════════════════════════════════════════════════════════════════════════
// Финализация физики подлодки — вызывать ПОСЛЕ добавления всех отсеков!
// ══════════════════════════════════════════════════════════════════════════════
void SubmarineBuilder::FinalizeSubmarinePhysics(ECSRegistry& registry, Entity submarine)
{
    auto* subComp = registry.GetComponent<SubmarineComponent>(submarine);
    if (!subComp)
    {
        OutputDebugStringA("[FinalizePhysics] ERROR: No SubmarineComponent on root!\n");
        return;
    }

    // 1. Считаем суммарный объём всех отсеков
    float totalVolume = 0.0f;
    float totalMass   = 3000.0f; // базовая масса корпуса

    for (Entity compEntity : subComp->allCompartments)
    {
        auto* comp = registry.GetComponent<CompartmentComponent>(compEntity);
        if (comp)
        {
            totalVolume += comp->volume;
            totalMass   += 500.0f; // масса материала отсека
        }
    }

    // Добавляем запас объёма за корпус (+15 м³)
    totalVolume += 15.0f;

    char dbg[256];
    sprintf_s(dbg,
        "[FinalizePhysics] Compartments: %zu | TotalVolume: %.2f m³ | TotalMass: %.0f kg\n",
        subComp->allCompartments.size(), totalVolume, totalMass);
    OutputDebugStringA(dbg);

    // 2. RigidBodyComponent на корень (если ещё нет)
    if (!registry.HasComponent<RigidBodyComponent>(submarine))
    {
        RigidBodyComponent rb{};
        rb.mass            = totalMass;
        rb.shapeType       = RigidBodyComponent::Box;
        rb.shapeSize       = btVector3(6.0f, 2.0f, 2.0f); // примерный размер корпуса
        rb.linearDamping   = 0.5f;
        rb.angularDamping  = 0.8f;
        rb.friction        = 0.1f;
        registry.AddComponent<RigidBodyComponent>(submarine, rb);
    }
    else
    {
        // Обновляем массу в существующем компоненте
        auto* rb = registry.GetComponent<RigidBodyComponent>(submarine);
        rb->mass = totalMass;
    }

    // 3. BuoyancyComponent на корень (если ещё нет)
    if (!registry.HasComponent<BuoyancyComponent>(submarine))
    {
        BuoyancyComponent buoy{};
        buoy.volume               = totalVolume;
        buoy.lastCalculatedVolume = totalVolume;
        buoy.fluidDensity         = 1025.0f; // солёная вода
        registry.AddComponent<BuoyancyComponent>(submarine, buoy);
    }
    else
    {
        // Обновляем объём в существующем компоненте
        auto* buoy = registry.GetComponent<BuoyancyComponent>(submarine);
        buoy->volume               = totalVolume;
        buoy->lastCalculatedVolume = totalVolume;
        buoy->fluidDensity         = 1025.0f;
    }

    // 4. Создаём физическое тело — volume и mass уже финальные!
    PhysicsSystem::CreateRigidBody(registry, submarine);

    // 5. Кэшируем в SubmarineComponent
    subComp->cachedTotalMass   = totalMass;
    subComp->cachedTotalVolume = totalVolume;
    subComp->needsPhysicsUpdate = false;
    subComp->compartmentCountLastFrame = (int)subComp->allCompartments.size();

    OutputDebugStringA("[FinalizePhysics] ✅ Physics body created with correct volume!\n");
}