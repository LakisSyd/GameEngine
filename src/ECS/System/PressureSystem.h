#pragma once

////////////////////////////////////////////////////////////////////////////////
// Система: PressureSystem
// Назначение: Расчёт давления воды и урона корпусу
// Вызов: В ApplicationClass::Frame, после PhysicsSystem::Update
////////////////////////////////////////////////////////////////////////////////

#include "ECSRegistry.h"

class PressureSystem
{
public:
    // Обновление системы
    // registry — реестр ECS
    // deltaTime — время между кадрами
    // waterLevel — глобальный уровень воды (Y-координата поверхности)
    static void Update(ECSRegistry& registry, float deltaTime, float waterLevel);
};