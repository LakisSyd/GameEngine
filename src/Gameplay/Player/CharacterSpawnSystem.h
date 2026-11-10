#pragma once
#include <algorithm>
#include <ECSRegistry.h>
#include "PhysicsWorld.h"

class CharacterSpawnSystem {
public:

    static void Destroy(
        ECSRegistry& registry,
        Entity entity
    );

    static void Spawn(ECSRegistry& registry, Entity entity, const XMFLOAT3& startPosition)
    {
        auto* controller = registry.GetComponent<CharacterController>(entity);
        auto* tf = registry.GetComponent<TransformComponent>(entity);
        if (!controller || !tf) {
            OutputDebugStringA("[CharacterSpawn] Missing components!\n");
            return;
        }

        

        if (controller->controller || controller->ghostObj || controller->capsuleShape)
            Destroy(registry, entity);

        // -- 1. Капсула -----------------------------------------------------
        controller->capsuleRadius = (std::max)(controller->capsuleRadius, 0.05f);
        controller->capsuleHeight = (std::max)(controller->capsuleHeight, 0.1f);
        controller->capsuleCenterOffsetY = controller->capsuleHeight * 0.5f + controller->capsuleRadius;
        controller->capsuleShape = new btCapsuleShape(controller->capsuleRadius, controller->capsuleHeight);
        controller->capsuleShape->setMargin(0.04f);

        // -- 2. Ghost object ------------------------------------------------
        controller->ghostObj = new btPairCachingGhostObject();

        btTransform startTransform;
        startTransform.setIdentity();
        startTransform.setOrigin(btVector3(startPosition.x, startPosition.y, startPosition.z));

        controller->ghostObj->setWorldTransform(startTransform);
        controller->ghostObj->setCollisionShape(controller->capsuleShape);
        controller->ghostObj->setUserIndex(static_cast<int>(entity));
        controller->ghostObj->setCollisionFlags(
            controller->ghostObj->getCollisionFlags() | btCollisionObject::CF_CHARACTER_OBJECT
        );

        // -- 3. Контроллер (3 аргумента для совместимости!) -----------------
        controller->controller = new btKinematicCharacterController(
            controller->ghostObj,
            controller->capsuleShape,
            controller->stepHeight
            // ? Нет 4-го аргумента upAxis в старых версиях!
        );

       

        controller->controller->setGravity(btVector3(0, -controller->gravity, 0));
        controller->controller->setMaxSlope(controller->maxSlope * 0.01745329252f);
        controller->controller->setJumpSpeed(controller->jumpSpeed);
        controller->controller->setUseGhostSweepTest(false);
        controller->controller->setMaxPenetrationDepth(0.08f);

       
        startTransform.setIdentity();

        // Bullet хранит центр капсулы, а Transform игрока считается визуальной точкой у ног.
        startTransform.setOrigin(btVector3(
            startPosition.x,
            startPosition.y + controller->capsuleCenterOffsetY,
            startPosition.z
        ));

        controller->ghostObj->setWorldTransform(startTransform);
        if (controller->controller)
            controller->controller->warp(startTransform.getOrigin());
        controller->lastSyncedPosition = startPosition;
        controller->lastSyncedRotation = tf->rotation;

        
        // -- 5. Регистрация в мире ------------------------------------------
        auto* world = PhysicsWorld::Get().GetWorld();
        if (!world)
        {
            OutputDebugStringA("[CharacterSpawn] Physics world is null!\n");
            Destroy(registry, entity);
            return;
        }

        world->addCollisionObject(
            controller->ghostObj,
            btBroadphaseProxy::CharacterFilter,
            btBroadphaseProxy::DefaultFilter |
            btBroadphaseProxy::StaticFilter |
            btBroadphaseProxy::KinematicFilter
        );


        // ?? Критично: регистрируем как action
        world->addAction(controller->controller);

        // -- 6. Отладочный вывод (без getNumActions) ------------------------
        OutputDebugStringA(
            ("[Character] Spawned: pos=(" +
                std::to_string(startPosition.x) + "," +
                std::to_string(startPosition.y) + "," +
                std::to_string(startPosition.z) + ")\n").c_str()
        );

        OutputDebugStringA("[Character] Settings applied:\n");
       
        btVector3 g = controller->controller->getGravity();
        OutputDebugStringA(("  Gravity: (" +
            std::to_string(g.getX()) + "," +
            std::to_string(g.getY()) + "," +
            std::to_string(g.getZ()) + ")\n").c_str());

        // В CharacterSyncSystem, при первом обновлении:
        static bool firstSync = true;
        if (firstSync) {
            OutputDebugStringA("[CharacterSync] First sync called\n");
            firstSync = false;
        }



    }

};

