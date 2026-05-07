#pragma once
#include <ECSRegistry .h>
#include "PhysicsWorld.h"

class CharacterSpawnSystem {
public:
    static void Spawn(ECSRegistry& registry, Entity entity, const XMFLOAT3& startPosition)
    {
        auto* controller = registry.GetComponent<CharacterController>(entity);
        auto* tf = registry.GetComponent<TransformComponent>(entity);
        if (!controller || !tf) {
            OutputDebugStringA("[CharacterSpawn] Missing components!\n");
            return;
        }

        // ── 1. Капсула ─────────────────────────────────────────────────────
        controller->capsuleShape = new btCapsuleShape(0.4f, 1.4f);
        controller->capsuleShape->setMargin(0.04f);

        // ── 2. Ghost object ────────────────────────────────────────────────
        controller->ghostObj = new btPairCachingGhostObject();

        btTransform startTransform;
        startTransform.setIdentity();
        startTransform.setOrigin(btVector3(startPosition.x, startPosition.y, startPosition.z));

        controller->ghostObj->setWorldTransform(startTransform);
        controller->ghostObj->setCollisionShape(controller->capsuleShape);
        controller->ghostObj->setCollisionFlags(
            controller->ghostObj->getCollisionFlags() | btCollisionObject::CF_CHARACTER_OBJECT
        );

        // ── 3. Контроллер (3 аргумента для совместимости!) ─────────────────
        controller->controller = new btKinematicCharacterController(
            controller->ghostObj,
            controller->capsuleShape,
            controller->stepHeight
            // ❌ Нет 4-го аргумента upAxis в старых версиях!
        );

        // ── 4. Настройки (без setUpAxis!) ──────────────────────────────────
        // ❌ controller->controller->setUpAxis(1);  // ← Убери эту строку!

        controller->controller->setGravity(btVector3(0, -controller->gravity, 0));
        controller->controller->setMaxSlope(controller->maxSlope);
        controller->controller->setJumpSpeed(controller->jumpSpeed);
        controller->controller->setUseGhostSweepTest(true);

       
        startTransform.setIdentity();

        // 🔥 Сдвиг на 3 метра вверх — персонаж должен спавниться НАД полом
        startTransform.setOrigin(btVector3(
            startPosition.x,
            startPosition.y + 3.0f,  // ← КРИТИЧНО!
            startPosition.z
        ));

        controller->ghostObj->setWorldTransform(startTransform);

        // ── 5. Регистрация в мире ──────────────────────────────────────────
        auto* world = PhysicsWorld::Get().GetWorld();

        world->addCollisionObject(
            controller->ghostObj,
            btBroadphaseProxy::CharacterFilter,
            btBroadphaseProxy::AllFilter  // 🔥 Коллизирует со ВСЕМ (только для тестов!)
        );


        // 🔥 Критично: регистрируем как action
        world->addAction(controller->controller);

        // ── 6. Отладочный вывод (без getNumActions) ────────────────────────
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

#ifdef _DEBUG
        OutputDebugStringA(
            ("[CharDebug] entity=" + std::to_string(entity) +
                " | onGround=" + std::to_string(controller->controller->onGround()) +
                " | posY=" + std::to_string(startPosition.y) + "\n").c_str()
        );
#endif


#ifdef _DEBUG
        auto* ghost = controller->ghostObj;
        char buf[256];

        sprintf_s(buf,
            "[CharCollision] ghost group=0x%X, mask=0x%X\n",
            ghost->getBroadphaseHandle()->m_collisionFilterGroup,
            ghost->getBroadphaseHandle()->m_collisionFilterMask
        );
        OutputDebugStringA(buf);

        // Тест: создаём луч вниз и проверяем, детектируется ли пол
        btCollisionWorld::ClosestRayResultCallback rayCallback(
            btVector3(startPosition.x, startPosition.y + 1.0f, startPosition.z),
            btVector3(startPosition.x, startPosition.y - 10.0f, startPosition.z)
        );
        rayCallback.m_collisionFilterGroup = btBroadphaseProxy::CharacterFilter;
        rayCallback.m_collisionFilterMask = btBroadphaseProxy::StaticFilter;

         world->rayTest(rayCallback.m_rayFromWorld, rayCallback.m_rayToWorld, rayCallback);

        if (rayCallback.hasHit())
        {
            OutputDebugStringA("[CharCollision] ✅ Ray hit ground — masks OK\n");
        }
        else
        {
            OutputDebugStringA("[CharCollision] ❌ Ray missed — check floor masks!\n");
        }
#endif

    }

};

