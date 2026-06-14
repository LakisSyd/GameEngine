#include "CharacterSpawnSystem.h"

void CharacterSpawnSystem::Destroy(ECSRegistry& registry, Entity entity)
{
    auto* character =
        registry.GetComponent<CharacterController>(entity);

    if (!character)
        return;

    auto* world = PhysicsWorld::Get().GetWorld();

    if (world)
    {
        if (character->controller)
            world->removeAction(character->controller);

        if (character->ghostObj)
            world->removeCollisionObject(character->ghostObj);
    }
}
