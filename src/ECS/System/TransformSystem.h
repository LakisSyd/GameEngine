#pragma once
// Подсказка по структуре файла:
#include "ECSRegistry .h"
#include "gameobject.h"
class TransformSystem {
public:
    // Принимает реестр и вектор объектов
    // Итерирует все TransformComponent
    // Синхронизирует с GameObject
    static void Update(ECSRegistry& registry, std::vector<GameObject>& objects)
    {
        for (auto& [entity, transform] : registry.GetComponents<TransformComponent>())
        {
            if(entity < objects.size()) // Проверяем, что индекс объекта валиден
            {
                GameObject& obj = objects[entity];
                obj.SetPosition(transform.position);
                obj.SetRotation(transform.rotation);
                obj.SetScale(transform.scale);
			}
        }
    }
};