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
    // ✅ КРИТИЧНО: Добавляем физику ПОСЛЕ всех отсеков — volume уже известно!
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
            totalMass   += 500.0f;
        }
    }
    totalVolume += 15.0f;

    char dbg[256];
    sprintf_s(dbg,
        "[FinalizePhysics] Compartments: %zu | TotalVolume: %.2f m³ | TotalMass: %.0f kg\n",
        subComp->allCompartments.size(), totalVolume, totalMass);
    OutputDebugStringA(dbg);

    // 2. Читаем конфигурацию корпуса (или используем дефолт)
    SubmarineHullConfig hullCfg{};
    if (registry.HasComponent<SubmarineHullConfig>(submarine))
        hullCfg = *registry.GetComponent<SubmarineHullConfig>(submarine);
    else
        registry.AddComponent<SubmarineHullConfig>(submarine, hullCfg);

    // 3. RigidBodyComponent — GImpactMesh вместо Compound
    if (!registry.HasComponent<RigidBodyComponent>(submarine))
    {
        RigidBodyComponent rb{};
        rb.mass           = hullCfg.isKinematic ? 0.0f : totalMass;
        rb.isKinematic    = hullCfg.isKinematic;
        rb.linearDamping  = 0.5f;
        rb.angularDamping = 0.8f;
        rb.friction       = 0.1f;

        // ═══════════════════════════════════════════════════════════════════
        // 🔥 ИЗМЕНЕНИЕ: GImpactMesh вместо Compound из боксов
        //    Коллизия берётся напрямую из 3D-модели — корпус, перегородки,
        //    палубы — всё что есть в меше, всё становится коллизией.
        //    Больше не нужно вручную описывать каждый бокс!
        // ═══════════════════════════════════════════════════════════════════
        rb.isCompound     = false;
        rb.shapeType      = RigidBodyComponent::GImpactMesh;

        registry.AddComponent<RigidBodyComponent>(submarine, std::move(rb));
    }
    else
    {
        auto* rb = registry.GetComponent<RigidBodyComponent>(submarine);
        rb->mass       = hullCfg.isKinematic ? 0.0f : totalMass;
        rb->isCompound = false;
        rb->shapeType  = RigidBodyComponent::GImpactMesh;
        rb->compoundElements.clear();
    }

    // 4. BuoyancyComponent
    if (!registry.HasComponent<BuoyancyComponent>(submarine))
    {
        BuoyancyComponent buoy{};
        buoy.volume               = totalVolume;
        buoy.lastCalculatedVolume = totalVolume;
        buoy.fluidDensity         = 1025.0f;
        registry.AddComponent<BuoyancyComponent>(submarine, buoy);
    }
    else
    {
        auto* buoy = registry.GetComponent<BuoyancyComponent>(submarine);
        buoy->volume               = totalVolume;
        buoy->lastCalculatedVolume = totalVolume;
        buoy->fluidDensity         = 1025.0f;
    }

    // 5. Создаём физическое тело
    //    CreateRigidBody увидит shapeType == GImpactMesh и вызовет
    //    CreateGImpactMeshShape(), который возьмёт треугольники из
    //    MeshRendererComponent->model
    PhysicsSystem::CreateRigidBody(registry, submarine);

    // 6. Если кинематический — настраиваем флаги Bullet
    auto* rb = registry.GetComponent<RigidBodyComponent>(submarine);
    if (rb && rb->body && hullCfg.isKinematic)
    {
        rb->body->setCollisionFlags(
            rb->body->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
        rb->body->setActivationState(DISABLE_DEACTIVATION);
        OutputDebugStringA("[FinalizePhysics] Submarine set as KINEMATIC object\n");
    }

    // 7. Кэшируем
    subComp->cachedTotalMass              = totalMass;
    subComp->cachedTotalVolume            = totalVolume;
    subComp->needsPhysicsUpdate           = false;
    subComp->compartmentCountLastFrame    = (int)subComp->allCompartments.size();

    OutputDebugStringA("[FinalizePhysics] ✅ GImpactMesh hull created from model geometry!\n");
}

// ══════════════════════════════════════════════════════════════════════════════
// Построить 6 стен корпуса как CompoundElements
// Каждая стена — btBoxShape с толщиной wallThickness
// ══════════════════════════════════════════════════════════════════════════════
void SubmarineBuilder::BuildHullCompoundElements(
    const SubmarineHullConfig& cfg, RigidBodyComponent& rb)
{
    const float hx = cfg.hullExtents.x * 0.5f; // полуширина  (X)
    const float hy = cfg.hullExtents.y * 0.5f; // полувысота  (Y)
    const float hz = cfg.hullExtents.z * 0.5f; // полудлина   (Z)
    const float t = cfg.wallThickness;

    // ══════════════════════════════════════════════════════════════════════
    // Корпус строится из 6 ТОЛСТЫХ ПЛИТ (btBoxShape), образующих полый ящик.
    // 
    // ВАЖНО: Внутри корпуса НЕТ коллизии — Bullet не блокирует объекты
    // внутри compound shape снаружи-внуть. Это нормально для подлодки:
    //   - Игрок/объекты ВНУТРИ — управляются логикой (отсеки, давление)
    //   - Внешние объекты (дно, торпеды) — сталкиваются со стенами снаружи
    //
    // Схема (вид сбоку, XZ-плоскость):
    //
    //   ┌──[Потолок  Y+]──┐
    //   │                 │
    // [Лев X-]  ПУСТО  [Прав X+]
    //   │                 │
    //   └──[Дно     Y-]──┘
    //   [Нос Z+]     [Корма Z-]
    //
    // Каждая плита: halfExt задаёт её размер, pos — центр плиты.
    // ══════════════════════════════════════════════════════════════════════

    // debugColorIndex используется PhysicsDebugRenderer для цветовой маркировки
    // 0=дно(красный), 1=потолок(синий), 2=левая(зелёный),
    // 3=правая(жёлтый), 4=нос(оранжевый), 5=корма(фиолетовый)
    int colorIdx = 0;

    auto addWall = [&](XMFLOAT3 pos, XMFLOAT3 halfExt, const char* label)
        {
            RigidBodyComponent::CompoundElement wall{};
            wall.type = RigidBodyComponent::CompoundElement::Box;
            wall.localPosition = pos;
            wall.localRotation = { 0, 0, 0, 1 };
            wall.boxOffsetMin = { -halfExt.x, -halfExt.y, -halfExt.z };
            wall.boxOffsetMax = { halfExt.x,  halfExt.y,  halfExt.z };
            wall.size = 1.0f;
            wall.debugWeight = (float)colorIdx; // ← кодируем индекс цвета
            rb.compoundElements.push_back(wall);

            char buf[128];
            sprintf_s(buf, "  [Wall %d] %-10s pos=(%.1f,%.1f,%.1f) half=(%.2f,%.2f,%.2f)\n",
                colorIdx, label, pos.x, pos.y, pos.z,
                halfExt.x, halfExt.y, halfExt.z);
            OutputDebugStringA(buf);

            ++colorIdx;
        };

    OutputDebugStringA("[BuildHull] Building 6-wall compound hull:\n");

    // ── 0: Дно (нижняя стена, Y-) ──────────────────────────────────────────
    // Центр: сдвинут к низу на (hy - t/2), толщина t по Y
    addWall({ 0.0f, -hy + t * 0.5f, 0.0f }, { hx, t * 0.5f, hz }, "Bottom");

    // ── 1: Потолок (верхняя стена, Y+) ────────────────────────────────────
    addWall({ 0.0f,  hy - t * 0.5f, 0.0f }, { hx, t * 0.5f, hz }, "Top");

    // ── 2: Левая стена (X-) ────────────────────────────────────────────────
    addWall({ -hx + t * 0.5f, 0.0f, 0.0f }, { t * 0.5f, hy, hz }, "Left");

    // ── 3: Правая стена (X+) ───────────────────────────────────────────────
    addWall({ hx - t * 0.5f, 0.0f, 0.0f }, { t * 0.5f, hy, hz }, "Right");

    // ── 4: Нос (передняя стена, Z+) ───────────────────────────────────────
    addWall({ 0.0f, 0.0f,  hz - t * 0.5f }, { hx, hy, t * 0.5f }, "Bow");

    // ── 5: Корма (задняя стена, Z-) ────────────────────────────────────────
    addWall({ 0.0f, 0.0f, -hz + t * 0.5f }, { hx, hy, t * 0.5f }, "Stern");

    char buf[256];
    sprintf_s(buf,
        "[BuildHull] Done. HullExtents=(%.1f x %.1f x %.1f), WallThickness=%.2f\n"
        "[BuildHull] NOTE: Interior is HOLLOW — objects inside are handled by compartment logic\n",
        cfg.hullExtents.x, cfg.hullExtents.y, cfg.hullExtents.z, t);
    OutputDebugStringA(buf);
}