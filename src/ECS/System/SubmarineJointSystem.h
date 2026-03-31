// SubmarineJointSystem.h
#pragma once

#include <vector>
#include <btBulletDynamicsCommon.h>  // Bullet Physics: btVector3, btFixedConstraint и т.д.

// Forward declarations для уменьшения зависимостей
class ECSRegistry;
struct SubmarineJointComponent;
using Entity = unsigned int;  // Или uint32_t — уточни под свой проект

/**
 * @brief Система управления физическими соединениями между отсеками подлодки
 *
 * Отвечает за создание, обновление и удаление btFixedConstraint между
 * физическими телами отсеков, отслеживание нагрузки и разрушение соединений.
 */
class SubmarineJointSystem
{
public:
    /**
     * @brief Создаёт фиксированный джойнт между двумя объектами
     *
     * @param registry Ссылка на ECS-реестр
     * @param a Первый объект (отсек)
     * @param b Второй объект (отсек)
     * @param pivotA_local Точка крепления в локальном пространстве объекта A
     * @param pivotB_local Точка крепления в локальном пространстве объекта B
     * @param breakForce Пороговое значение импульса для разрушения джойнта
     */
    static void CreateJoint(ECSRegistry& registry, Entity a, Entity b,
        const btVector3& pivotA_local, const btVector3& pivotB_local, float breakForce);

    /**
     * @brief Обновляет состояние всех активных джойнтов
     *
     * Проверяет приложенный импульс и обрабатывает разрушение соединений.
     *
     * @param registry Ссылка на ECS-реестр
     * @param deltaTime Время между кадрами (может использоваться для интерполяции)
     */
    static void Update(ECSRegistry& registry, float deltaTime);

    /**
     * @brief Удаляет конкретный джойнт по его сущности
     *
     * @param registry Ссылка на ECS-реестр
     * @param jointEntity Сущность, содержащая SubmarineJointComponent
     */
    static void RemoveJoint(ECSRegistry& registry, Entity jointEntity);

    /**
     * @brief Удаляет все джойнты, связанные с указанной сущностью
     *
     * Полезно при уничтожении отсека — автоматически чистит все его соединения.
     *
     * @param registry Ссылка на ECS-реестр
     * @param entity Сущность отсека, для которой нужно удалить соединения
     */
    static void RemoveJointsForEntity(ECSRegistry& registry, Entity entity);

private:
    // Приватные вспомогательные методы можно добавить при необходимости
    // Например: валидация параметров, логирование, пулинг джойнтов
};