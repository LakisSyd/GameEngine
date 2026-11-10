#pragma once

#include "NPCComponents.h"

// =============================================================================
// NpcCommandSystem — единственная точка выдачи ПРИОРИТЕТНЫХ приказов NPC.
// =============================================================================
//
// Вызывающий код (debug UI, квестовый скрипт, консоль, сеть) не должен вручную
// менять brain.command и чистить старый маршрут. Он вызывает один из методов
// ниже. Так мы гарантируем одинаковое поведение команды независимо от источника.
//
// ПОЧЕМУ РУЧНАЯ КОМАНДА ПРИОРИТЕТНЕЕ АВТОМАТИЧЕСКОГО РЕМОНТА:
//
//   кнопка UI -> NpcCommandSystem::MoveToRoom(...)
//             -> brain.command.type становится MoveToRoom
//             -> NpcCommand::IsActive() возвращает true
//             -> NpcUtilitySystem продолжает считать scores для debug-панели,
//                но НЕ меняет выбранную задачу
//             -> StateSystem сначала проверяет IsActive() и исполняет команду
//
// ResumeAutomatic() делает type == None, и на следующем кадре Utility AI снова
// получает право выбрать пробоину.
//
// КАК ДОБАВИТЬ НОВУЮ КОМАНДУ:
//   1. Добавить значение в NpcCommandType.
//   2. Добавить публичный метод здесь, который вызывает Begin(...).
//   3. Добавить case в StateSystem::UpdateManualCommand().
//   4. При необходимости добавить поле цели в NpcCommand и кнопку в EditorUI.
// Utility scoring при этом менять не требуется.
class NpcCommandSystem
{
public:
    // Снять любой ручной приказ и вернуть NPC в автоматический режим.
    // ResetNavigation важен: маршрут старой команды нельзя повторно использовать
    // для автоматически выбранной пробоины.
    static void ResumeAutomatic(NpcBrainComponent& brain)
    {
        brain.command.Clear();
        ResetNavigation(brain);
        brain.status = "Automatic utility AI resumed";
    }

    // Постоянная команда. Stop не завершается сам: только Resume auto или новый
    // приказ снова разрешит NPC двигаться.
    static void Stop(NpcBrainComponent& brain)
    {
        Begin(brain, NpcCommandType::Stop);
    }

    // Постоянная команда брожения. Сбрасываем wanderTargetValid, чтобы первая
    // случайная точка была выбрана относительно актуальной позиции NPC.
    static void Wander(NpcBrainComponent& brain)
    {
        Begin(brain, NpcCommandType::Wander);
        brain.wanderTargetValid = false;
    }

    // Одноразовая команда. После достижения target StateSystem вызовет
    // CompleteManualCommand(), очистит command и вернёт автоматический режим.
    static void MoveToPosition(NpcBrainComponent& brain, const DirectX::XMFLOAT3& target)
    {
        Begin(brain, NpcCommandType::MoveToPosition);
        brain.command.worldPosition = target;
    }

    // room — Entity комнаты, не порядковый номер в UI. У Entity должны быть
    // RoomComponent и TransformComponent.
    static void MoveToRoom(NpcBrainComponent& brain, Entity room)
    {
        Begin(brain, NpcCommandType::MoveToRoom);
        brain.command.targetRoom = room;
    }

    // Постоянно следовать за Entity. Целевая позиция и комната читаются заново
    // каждый кадр, поэтому движущуюся цель можно сопровождать между комнатами.
    static void FollowEntity(NpcBrainComponent& brain, Entity target)
    {
        Begin(brain, NpcCommandType::FollowEntity);
        brain.command.targetEntity = target;
    }

    // Одноразово починить именно эту Entity. Сейчас корректной целью является
    // незапечатанная HullBreachComponent + RepairableComponent.
    static void RepairEntity(NpcBrainComponent& brain, Entity target)
    {
        Begin(brain, NpcCommandType::RepairEntity);
        brain.command.targetEntity = target;
    }

    // Ручная диагностическая команда: не меняет цель, но помечает кэш маршрута
    // грязным. Полезно после исправления roomA/roomB или состояния двери в Play.
    static void ForceRepath(NpcBrainComponent& brain)
    {
        // revision также инвалидирует кэш опционального NavMesh-слоя без прямой
        // зависимости NpcCommandSystem от Recast/Detour.
        ++brain.command.revision;
        ResetNavigation(brain);
        brain.status = "Route rebuild requested";
    }

private:
    // Общая подготовка нового приказа.
    //
    // Почему сначала очищаются обе Entity-цели: иначе после смены FollowEntity
    // на MoveToRoom в brain осталась бы старая targetEntity. Текущей логике это
    // не мешает, но сильно путает отладку и будущую сериализацию.
    static void Begin(NpcBrainComponent& brain, NpcCommandType type)
    {
        brain.debugDoorSnapshotCaptured = false;
        brain.debugDoorSnapshot.clear();
        brain.command.type = type;
        brain.command.targetEntity = INVALID_ENTITY;
        brain.command.targetRoom = INVALID_ENTITY;
        ++brain.command.revision;
        ResetNavigation(brain);
        brain.blockedReason.clear();
        brain.status = std::string("Manual command: ") + NpcCommandName(type);
    }

    // Сбрасывает только навигационную память. Настройки NPC, автоматическая цель
    // repairTarget и wanderAnchor здесь намеренно не уничтожаются.
    static void ResetNavigation(NpcBrainComponent& brain)
    {
        brain.doorRoute.clear();
        brain.doorDestinationRooms.clear();
        brain.doorRouteIndex = 0;
        brain.navigationTargetRoom = INVALID_ENTITY;
        brain.activeDoor = INVALID_ENTITY;
        brain.routeDirty = true;
        brain.repathTimer = 0.0f;
        brain.stuckTimer = 0.0f;
        brain.progressPositionInitialized = false;
    }
};
