#pragma once
#include "ECSRegistry.h"
#include "Components.h"

class JointConnectionSystem
{
public:
    // Вызывать каждый кадр — создаёт/удаляет джойнты по состоянию компонентов
    static void Update(ECSRegistry& registry);

    // Создать физический джойнт из JointConnectionComponent
    static void CreateJoint(ECSRegistry& registry, Entity entity, JointConnectionComponent& jointComp);

    // Удалить физический джойнт (не удаляет компонент, только constraint)
    static void RemoveJoint(ECSRegistry& registry, JointConnectionComponent& jointComp);

    // Удалить все джойнты, связанные с данным entity
    static void RemoveJointsForEntity(ECSRegistry& registry, Entity entity);
};
