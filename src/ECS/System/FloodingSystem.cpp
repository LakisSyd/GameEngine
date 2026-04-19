#include "FloodingSystem.h"
#include "Components.h"
#include <algorithm>

void FloodingSystem::Update(ECSRegistry& registry, float deltaTime, float globalWaterLevel)
{
    auto& compartmentPool = registry.GetComponents<CompartmentComponent>();
    auto& transformPool = registry.GetComponents<TransformComponent>();
    auto& subPool = registry.GetComponents<SubmarineComponent>();

    for (auto& [entity, comp] : compartmentPool)
    {
        // ✅ Проверяем, это отсек подлодки (не корень!)
        auto* subLink = subPool.Get(entity);
        if (!subLink || subLink->submarineRoot == INVALID_ENTITY) continue;
        if (entity == subLink->submarineRoot) continue;  // ← Пропускаем корень!

        // 🔥 КРИТИЧЕСКИ ВАЖНО: Используем трансформ ОТСЕКА, а не подлодки!
        auto* compartmentTransform = transformPool.Get(entity);
        if (!compartmentTransform) continue;

        XMMATRIX compartmentWorldMatrix = compartmentTransform->GetWorldMatrix();

        // ══════════════════════════════════════════════════════════════════════
        // 1. РАССЧИТЫВАЕМ ПРИТОК ВОДЫ ЧЕРЕЗ ПРОБОИНЫ
        // ══════════════════════════════════════════════════════════════════════
        float totalInflow = 0.0f;

        for (auto& breach : comp.breaches)
        {
            if (breach.isSealed) continue;

            // 🔥 Преобразуем локальную позицию пробоины в мировую
            // breach.localPosition - это в координатах самого отсека!
            XMVECTOR localPos = XMLoadFloat3(&breach.localPosition);
            XMVECTOR worldPos = XMVector3TransformCoord(localPos, compartmentWorldMatrix);
            XMFLOAT3 breachWorldPos;
            XMStoreFloat3(&breachWorldPos, worldPos);

            // 🔥 Проверяем, под ли водой пробоина
            float breachDepth = globalWaterLevel - breachWorldPos.y;

            if (breachDepth > 0.0f)
            {
                float pressure = breachDepth * 9.81f;
                breach.flowRate = CalculateFlowRate(breach, pressure);
                totalInflow += breach.flowRate;

                // 📊 DEBUG: Выводим информацию о течи
#ifdef _DEBUG
                char buf[256];
                sprintf_s(buf, "[Flooding] Breach #%p: depth=%.2f, pressure=%.1f, flowRate=%.3f m³/s\n",
                    &breach, breachDepth, pressure, breach.flowRate);
                OutputDebugStringA(buf);
#endif
            }
            else
            {
                breach.flowRate = 0.0f;
            }
        }

        // 🔥 Проверка: НИЖНЯЯ граница отсека под водой?
        // Это критично для определения, может ли вода течь внутрь
        XMFLOAT3 localBottomCenter = {
            (comp.localMin.x + comp.localMax.x) * 0.5f,
            comp.localMin.y,  // 🔥 НИЖНЯЯ точка!
            (comp.localMin.z + comp.localMax.z) * 0.5f
        };

        XMVECTOR worldBottomV = XMVector3TransformCoord(
            XMLoadFloat3(&localBottomCenter),
            compartmentWorldMatrix);
        float worldBottomY = XMVectorGetY(worldBottomV);
        float bottomDepth = globalWaterLevel - worldBottomY;

        // 🔥 ОТЛАДКА: Выводим состояние отсека
#ifdef _DEBUG
        {
            char buf[256];
            sprintf_s(buf, "[Compartment %u] Bottom Y: world=%.2f, depth=%.2f, water=%.1f%%, breaches=%zu\n",
                entity,
                worldBottomY,
                bottomDepth,
                (comp.volume > 0) ? (comp.waterVolume / comp.volume) * 100.f : 0.f,
                comp.breaches.size());
            OutputDebugStringA(buf);
        }
#endif



        // ══════════════════════════════════════════════════════════════════════
        // 2. ОТКАЧКА НАСОСАМИ (если есть питание)
        // ══════════════════════════════════════════════════════════════════════
        float totalOutflow = 0.0f;
        if (comp.hasPower && comp.waterVolume > 0.0f)
        {
            totalOutflow = comp.pumpRate;  // м³/с
        }

      

        // ══════════════════════════════════════════════════════════════════════
        // 4. ОБНОВЛЯЕМ ОБЪЕМ ВОДЫ
        // ══════════════════════════════════════════════════════════════════════
        float netFlow = (totalInflow - totalOutflow) * deltaTime;
        comp.waterVolume += netFlow;
        comp.waterVolume = fmaxf(0.0f, fminf(comp.waterVolume, comp.volume));

        // ══════════════════════════════════════════════════════════════════════
        // 5. ВЫЧИСЛЯЕМ УРОВЕНЬ ВОДЫ (от дна отсека)
        // ══════════════════════════════════════════════════════════════════════
        float height = comp.localMax.y - comp.localMin.y;
        if (height > 0.0f)
        {
            comp.waterLevel = (comp.waterVolume / comp.volume) * height;
        }
        else
        {
            comp.waterLevel = 0.0f;
        }

        // ── Синхронизируем DamageDecalComponent с пробоинами ─────────────────
        if (!registry.HasComponent<DamageDecalComponent>(entity))
            registry.AddComponent<DamageDecalComponent>(entity, DamageDecalComponent{});

        auto* decal = registry.GetComponent<DamageDecalComponent>(entity);
        decal->SyncFromBreaches(comp);
    }

    auto& doorPool = registry.GetComponents<DoorComponent>();

    for (auto& [doorEntity, door] : doorPool)
    {
        // Анимируем openProgress
        if (door.isOpen || door.isBroken)
            door.openProgress = min(1.0f, door.openProgress + door.openSpeed * deltaTime);
        else
            door.openProgress = max(0.0f, door.openProgress - door.openSpeed * deltaTime);

        // Если дверь закрыта — пропускаем
        if (door.openProgress <= 0.0f) continue;

        // Берём оба отсека
        auto* compA = registry.GetComponent<CompartmentComponent>(door.compartmentA);
        auto* compB = registry.GetComponent<CompartmentComponent>(door.compartmentB);
        if (!compA || !compB) continue;

        // Сообщающиеся сосуды
        float levelDiff = compA->waterLevel - compB->waterLevel;
        if (std::fabs(levelDiff) < 0.001f) continue;

        float flow = door.GetEffectiveFlowRate() * levelDiff * deltaTime;

        // Защита от переполнения и отрицательных значений
        flow = std::clamp(flow, 0.0f, compA->waterVolume);
        flow = std::min(flow, compB->volume - compB->waterVolume);

        compA->waterVolume -= flow;
        compB->waterVolume += flow;
    }

    // ══════════════════════════════════════════════════════════════════════
   // 6. ОБНОВЛЯЕМ ПЛАВУЧЕСТЬ КОРНЯ ПОДЛОДКИ
   //    Вода в отсеках вытесняет воздух → уменьшается выталкивающая сила
   //    buoy.volume = базовый объём - суммарный объём воды в отсеках
   // ══════════════════════════════════════════════════════════════════════
    auto& subRootPool = registry.GetComponents<SubmarineComponent>();

    for (auto& [entity, subComp] : subRootPool)
    {
        // Берём только корни подлодок
        if (subComp.submarineRoot != entity) continue;

        auto* buoy = registry.GetComponent<BuoyancyComponent>(entity);
        if (!buoy) continue;

        // Суммируем воду во всех отсеках
        float totalWaterVolume = 0.0f;
        for (Entity compEntity : subComp.allCompartments)
        {
            auto* comp = registry.GetComponent<CompartmentComponent>(compEntity);
            if (comp)
                totalWaterVolume += comp->waterVolume;
        }

        // Базовый объём кэшируем при первом кадре
        if (buoy->lastCalculatedVolume > 0.0f)
        {
            // Эффективный объём = базовый - вода (не меньше 0)
            buoy->volume = fmaxf(0.0f, buoy->lastCalculatedVolume - totalWaterVolume);
        }

#ifdef _DEBUG
        char buf[256];
        sprintf_s(buf, "[Buoyancy] Root=%u | Water=%.2f m³ | EffVolume=%.2f m³\n",
            entity, totalWaterVolume, buoy->volume);
        OutputDebugStringA(buf);
#endif
    }
}

// ══════════════════════════════════════════════════════════════════════════════
// Создание пробоины (в локальных координатах отсека!)
// ══════════════════════════════════════════════════════════════════════════════
void FloodingSystem::CreateBreach(ECSRegistry& registry, Entity compartment,
    const XMFLOAT3& localPosition, float size)
{
    auto* comp = registry.GetComponent<CompartmentComponent>(compartment);
    auto* transform = registry.GetComponent<TransformComponent>(compartment);
    if (!comp) return;

    // 🔥 Проверяем, что пробоина в пределах отсека!
    if (localPosition.x < comp->localMin.x || localPosition.x > comp->localMax.x ||
        localPosition.y < comp->localMin.y || localPosition.y > comp->localMax.y ||
        localPosition.z < comp->localMin.z || localPosition.z > comp->localMax.z)
    {
        OutputDebugStringA("❌ WARNING: Breach created OUTSIDE compartment bounds!\n");
    }

    BreachData breach;
    breach.localPosition = localPosition;
    breach.size = size;
    breach.flowRate = 0.0f;
    breach.isSealed = false;

    comp->breaches.push_back(breach);

    // 📊 DEBUG: Вычисляем мировую позицию для вывода
    XMFLOAT3 worldPos = { 0, 0, 0 };
    if (transform)
    {
        XMVECTOR localPosV = XMLoadFloat3(&localPosition);
        XMVECTOR worldPosV = XMVector3TransformCoord(localPosV, transform->GetWorldMatrix());
        XMStoreFloat3(&worldPos, worldPosV);
    }

    char buf[256];
    sprintf_s(buf,
        "[Breach] Entity=%u | Local=(%.2f,%.2f,%.2f) | World=(%.2f,%.2f,%.2f) | Size=%.2f\n"
        "         Comp bounds: [%.1f..%.1f, %.1f..%.1f, %.1f..%.1f]\n",
        compartment,
        localPosition.x, localPosition.y, localPosition.z,
        worldPos.x, worldPos.y, worldPos.z,
        size,
        comp->localMin.x, comp->localMax.x,
        comp->localMin.y, comp->localMax.y,
        comp->localMin.z, comp->localMax.z);
    OutputDebugStringA(buf);
}

// ══════════════════════════════════════════════════════════════════════════════
// Создание пробоины по мировой позиции (для удобства)
// ══════════════════════════════════════════════════════════════════════════════
void FloodingSystem::CreateBreachAtWorldPosition(ECSRegistry& registry, Entity compartment,
    const XMFLOAT3& worldPosition, float size)
{
    auto* transform = registry.GetComponent<TransformComponent>(compartment);
    if (!transform)
    {
        CreateBreach(registry, compartment, worldPosition, size);
        return;
    }

    // 🔥 Конвертируем world -> local через матрицу ОТСЕКА
    XMVECTOR worldV = XMLoadFloat3(&worldPosition);
    XMMATRIX invWorld = XMMatrixInverse(nullptr, transform->GetWorldMatrix());
    XMVECTOR localV = XMVector3TransformCoord(worldV, invWorld);
    XMFLOAT3 localPos;
    XMStoreFloat3(&localPos, localV);

    CreateBreach(registry, compartment, localPos, size);
}

// ══════════════════════════════════════════════════════════════════════════════
// Ремонт пробоины
// ══════════════════════════════════════════════════════════════════════════════
void FloodingSystem::SealBreach(ECSRegistry& registry, Entity compartment, size_t breachIndex)
{
    auto* comp = registry.GetComponent<CompartmentComponent>(compartment);
    if (!comp || breachIndex >= comp->breaches.size()) return;

    comp->breaches[breachIndex].isSealed = true;
    OutputDebugStringA("[Flooding] Breach sealed\n");
}

// ══════════════════════════════════════════════════════════════════════════════
// Поиск отсека, содержащего мировую позицию
// ══════════════════════════════════════════════════════════════════════════════
Entity FloodingSystem::FindCompartmentForObject(ECSRegistry& registry, const XMFLOAT3& worldPos)
{
    auto& compartmentPool = registry.GetComponents<CompartmentComponent>();
    auto& transformPool = registry.GetComponents<TransformComponent>();

    for (auto& [entity, comp] : compartmentPool)
    {
        auto* transform = transformPool.Get(entity);
        if (comp.ContainsPoint(worldPos, transform))
            return entity;
    }

    return INVALID_ENTITY;
}

// ══════════════════════════════════════════════════════════════════════════════
// Расчет скорости потока через пробоину (закон Торричелли)
// ══════════════════════════════════════════════════════════════════════════════
float FloodingSystem::CalculateFlowRate(const BreachData& breach, float waterPressure)
{
    // Площадь пробоины (круглая)
    float area = 3.14159f * breach.size * breach.size;

    // Скорость потока: v = √(2·P/ρ)
    // P - давление, ρ - плотность воды 1000 кг/м³
    float velocity = sqrtf(2.0f * waterPressure / 1000.0f);

    // Расход: Q = A × v (м³/с)
    return area * velocity;
}
