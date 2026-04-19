#pragma once
#include "ECSRegistry .h"
#include <vector>
#include <string>

struct SubmarineBlueprint
{
    std::string name = "New Submarine";
    std::vector<Entity> compartments;  // Список всех отсеков
    Entity rootEntity = INVALID_ENTITY; // Корневая сущность (центр подлодки)
};

// В SubmarineBuilder.h:

struct CompartmentGeometry
{
    XMFLOAT3 size;      // Полные размеры (width, height, length)
    XMFLOAT3 center;    // Центр меша
    float volume;

    // 🔹 Helper: получить pivot для соединения
    btVector3 GetPivotRight() const { return btVector3(size.x * 0.5f, 0, 0); }
    btVector3 GetPivotLeft()  const { return btVector3(-size.x * 0.5f, 0, 0); }
    btVector3 GetPivotFront() const { return btVector3(0, 0, size.z * 0.5f); }
    btVector3 GetPivotBack()  const { return btVector3(0, 0, -size.z * 0.5f); }
};

class SubmarineBuilder
{
public:
    // ══════════════════════════════════════════════════════════════════════════
    // 🔹 СОЗДАНИЕ ПОДЛОДКИ
    // ══════════════════════════════════════════════════════════════════════════

    // Создать новую подлодку (пустой контейнер)
    static Entity CreateSubmarine(ECSRegistry& registry, const std::string& name,
        const XMFLOAT3& position = { 0, 0, 0 });

    // Добавить отсек к подлодке
    static Entity AddCompartment(ECSRegistry& registry,
        Entity submarine,
        AssimpModelClass* model,
        const XMFLOAT3& localPosition,
        const XMFLOAT3& localSize,
        float mass = 100.0f);

    static void FinalizeSubmarinePhysics(ECSRegistry& registry, Entity submarine);

   static void BuildHullCompoundElements(const SubmarineHullConfig& cfg, RigidBodyComponent& rb);

    // Получить геометрию модели
    static CompartmentGeometry CalculateGeometryFromModel(AssimpModelClass* model);

    static  void ConnectCompartmentsAuto(ECSRegistry& registry,
        Entity compartmentA,
        Entity compartmentB,
        float breakForce = 150000.0f,
        float connectionMargin = 0.1f); // небольшой зазор

    // Соединить два отсека
    static void ConnectCompartments(ECSRegistry& registry,
        Entity compartmentA, Entity compartmentB,
        const btVector3& pivotA, const btVector3& pivotB,
        float breakForce = 100000.0f);

    // ══════════════════════════════════════════════════════════════════════════
    // 🔹 ГОТОВЫЕ ПРЕФАБЫ
    // ══════════════════════════════════════════════════════════════════════════

    // Простая подлодка (3 отсека: носовой, центральный, кормовой)
    static Entity CreateBasicSubmarine(ECSRegistry& registry,
        AssimpModelClass* compartmentModel,
        const XMFLOAT3& spawnPos = { 0, 0, 0 });

    // Большая подлодка (5 отсеков + вертикальные уровни)
    static Entity CreateLargeSubmarine(ECSRegistry& registry,
        AssimpModelClass* compartmentModel,
        const XMFLOAT3& spawnPos = { 0, 0, 0 });

    // ══════════════════════════════════════════════════════════════════════════
    // 🔹 ОТДЕЛЕНИЕ ОТСЕКА (при поломке джойнта)
    // ══════════════════════════════════════════════════════════════════════════

    static void DetachCompartment(ECSRegistry& registry, Entity compartment);

    // ══════════════════════════════════════════════════════════════════════════
    // 🔹 УТИЛИТЫ
    // ══════════════════════════════════════════════════════════════════════════

    // Получить все отсеки подлодки
    static std::vector<Entity> GetAllCompartments(ECSRegistry& registry, Entity submarine);

    // Проверить целостность подлодки (все ли отсеки соединены)
  
};
