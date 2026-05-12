#pragma once
#include <btBulletDynamicsCommon.h>
#include <BulletCollision/Gimpact/btGImpactCollisionAlgorithm.h>
#include <BulletCollision/Gimpact/btGImpactShape.h>
#include <memory>

class PhysicsWorld
{
public:
    static PhysicsWorld& Get()
    {
        static PhysicsWorld instance;
        return instance;
    }

    bool Initialize()
    {
        m_collisionConfig = std::make_unique<btDefaultCollisionConfiguration>();
        m_dispatcher = std::make_unique<btCollisionDispatcher>(m_collisionConfig.get());
        m_broadphase = std::make_unique<btDbvtBroadphase>();
        m_solver = std::make_unique<btSequentialImpulseConstraintSolver>();
        m_dynamicsWorld = std::make_unique<btDiscreteDynamicsWorld>(
            m_dispatcher.get(),
            m_broadphase.get(),
            m_solver.get(),
            m_collisionConfig.get()
        );

        m_dynamicsWorld->setGravity(btVector3(0, -9.81f, 0));
        m_ghostCallback = std::make_unique<btGhostPairCallback>();
        m_dynamicsWorld->getPairCache()->setInternalGhostPairCallback(m_ghostCallback.get());

        // ═══════════════════════════════════════════════════════════════
        // 🔥 REGISTER GImpact algorithm for dynamic concave mesh collision
        // Without this, btGImpactMeshShape won't detect collisions!
        // ═══════════════════════════════════════════════════════════════
        btGImpactCollisionAlgorithm::registerAlgorithm(m_dispatcher.get());

        // ═══════════════════════════════════════════════════════════════
        // 🔥 FIX: Увеличиваем итерации солвера для стабильных джойнтов.
        //    По умолчанию Bullet использует 10 итераций — этого мало
        //    для btFixedConstraint между тяжёлыми телами.
        //    50 итераций дают стабильные соединения отсеков подлодки.
        // ═══════════════════════════════════════════════════════════════
        m_dynamicsWorld->getSolverInfo().m_numIterations = 50;

        // 🔥 FIX: Split impulse предотвращает "отскок" от penetration
        //    correction. Без этого тела, проникшие друг в друга,
        //    получают огромный импульс, который рвёт джойнты.
        m_dynamicsWorld->getSolverInfo().m_splitImpulse = true;
        m_dynamicsWorld->getSolverInfo().m_splitImpulsePenetrationThreshold = -0.04f;

        // 🔥 FIX: ERP (Error Reduction Parameter) — скорость коррекции
        //    ошибки позиционирования. Слишком высокое значение = жёсткие
        //    рывки. 0.2 — мягкая коррекция, не рвёт джойнты.
        m_dynamicsWorld->getSolverInfo().m_erp = 0.2f;
        m_dynamicsWorld->getSolverInfo().m_erp2 = 0.2f;

        return true;
    }

    void Update(float deltaTime)
    {
        static float accumulator = 0.0f;
        const float timestep = 1.0f / 60.0f;
        accumulator += deltaTime;

        // 🔥 FIX: Ограничиваем максимальный accumulator,
        //    чтобы не было "spiral of death" при лагах
        if (accumulator > 0.25f)
            accumulator = 0.25f;

        while (accumulator >= timestep)
        {
            m_dynamicsWorld->stepSimulation(timestep, 1, timestep);
            accumulator -= timestep;
           
            
        }
    }

    btDiscreteDynamicsWorld* GetWorld() { return m_dynamicsWorld.get(); }

    void Shutdown()
    {
        OutputDebugStringA("[PhysicsWorld] Shutdown() called\n");  // ← Проверьте, появляется ли это

        // Порядок важен! Обратный порядку создания:
        if (m_dynamicsWorld) {
            // 1. Сначала удалить все тела из мира (если ещё не удалены через ECS)
            for (int i = m_dynamicsWorld->getNumCollisionObjects() - 1; i >= 0; --i) {
                btCollisionObject* obj = m_dynamicsWorld->getCollisionObjectArray()[i];
                btRigidBody* body = btRigidBody::upcast(obj);
                if (body && body->getMotionState()) {
                    delete body->getMotionState();
                }
                m_dynamicsWorld->removeCollisionObject(obj);
                delete obj;  // shape удалится автоматически, если не compound
            }
        }

        m_dynamicsWorld.reset();  // ~btDiscreteDynamicsWorld
        m_solver.reset();         // ~btSequentialImpulseConstraintSolver
        m_broadphase.reset();     // ~btDbvtBroadphase
        m_dispatcher.reset();     // ~btCollisionDispatcher
        m_collisionConfig.reset(); // ~btDefaultCollisionConfiguration ← освобождает пулы!

        OutputDebugStringA("[PhysicsWorld] Shutdown() complete\n");
    }

private:
    PhysicsWorld() = default;
    ~PhysicsWorld() = default;
    std::unique_ptr<btGhostPairCallback> m_ghostCallback;
    std::unique_ptr<btDefaultCollisionConfiguration> m_collisionConfig;
    std::unique_ptr<btCollisionDispatcher> m_dispatcher;
    std::unique_ptr<btBroadphaseInterface> m_broadphase;
    std::unique_ptr<btSequentialImpulseConstraintSolver> m_solver;
    std::unique_ptr<btDiscreteDynamicsWorld> m_dynamicsWorld;

    btActionInterface* action;
};
