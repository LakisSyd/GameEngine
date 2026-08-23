#pragma once
#include <btBulletDynamicsCommon.h>
#include <BulletCollision/Gimpact/btGImpactCollisionAlgorithm.h>
#include <BulletCollision/Gimpact/btGImpactShape.h>
#include <algorithm>
#include <cstdint>
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
        m_accumulator = 0.0f;
        m_interpolationAlpha = 0.0f;
        m_stepIndex = 0;
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

        // ===============================================================
        // ?? REGISTER GImpact algorithm for dynamic concave mesh collision
        // Without this, btGImpactMeshShape won't detect collisions!
        // ===============================================================
        btGImpactCollisionAlgorithm::registerAlgorithm(m_dispatcher.get());

        // ===============================================================
        // ?? FIX: Увеличиваем итерации солвера для стабильных джойнтов.
        //    По умолчанию Bullet использует 10 итераций — этого мало
        //    для btFixedConstraint между тяжёлыми телами.
        //    50 итераций дают стабильные соединения отсеков подлодки.
        // ===============================================================
        m_dynamicsWorld->getSolverInfo().m_numIterations = 50;

        // ?? FIX: Split impulse предотвращает "отскок" от penetration
        //    correction. Без этого тела, проникшие друг в друга,
        //    получают огромный импульс, который рвёт джойнты.
        m_dynamicsWorld->getSolverInfo().m_splitImpulse = true;
        m_dynamicsWorld->getSolverInfo().m_splitImpulsePenetrationThreshold = -0.04f;

        // ?? FIX: ERP (Error Reduction Parameter) — скорость коррекции
        //    ошибки позиционирования. Слишком высокое значение = жёсткие
        //    рывки. 0.2 — мягкая коррекция, не рвёт джойнты.
        m_dynamicsWorld->getSolverInfo().m_erp = 0.2f;
        m_dynamicsWorld->getSolverInfo().m_erp2 = 0.2f;

        return true;
    }

    void Update(float deltaTime)
    {
        constexpr float timestep = 1.0f / 60.0f;
        m_accumulator += (std::max)(0.0f, deltaTime);

        // ?? FIX: Ограничиваем максимальный accumulator,
        //    чтобы не было "spiral of death" при лагах
        if (m_accumulator > 0.25f)
            m_accumulator = 0.25f;

        while (m_accumulator >= timestep)
        {
            m_dynamicsWorld->stepSimulation(timestep, 1, timestep);
            m_accumulator -= timestep;
            ++m_stepIndex;
           
            
        }

        // Rendering interpolates between the previous and current fixed state.
        // Without this, a 120/144 Hz renderer displays the same 60 Hz character
        // transform for several frames and movement looks like tiny stops even
        // though both the renderer and the physics simulation are healthy.
        m_interpolationAlpha = std::clamp(m_accumulator / timestep, 0.0f, 1.0f);
    }

    btDiscreteDynamicsWorld* GetWorld() { return m_dynamicsWorld.get(); }
    float GetInterpolationAlpha() const { return m_interpolationAlpha; }
    std::uint64_t GetStepIndex() const { return m_stepIndex; }

    void Shutdown()
    {
        if (m_dynamicsWorld)
        {
            // ECS components own constraints, bodies and collision shapes.
            // The world only keeps non-owning registrations for them, so detach those before
            // the registry destroys the actual resources.
            for (int i = m_dynamicsWorld->getNumConstraints() - 1; i >= 0; --i)
                m_dynamicsWorld->removeConstraint(m_dynamicsWorld->getConstraint(i));
            for (int i = m_dynamicsWorld->getNumCollisionObjects() - 1; i >= 0; --i)
            {
                btCollisionObject* object = m_dynamicsWorld->getCollisionObjectArray()[i];
                m_dynamicsWorld->removeCollisionObject(object);
            }
        }
        m_dynamicsWorld.reset();
        m_ghostCallback.reset();
        m_solver.reset();
        m_broadphase.reset();
        m_dispatcher.reset();
        m_collisionConfig.reset();
        m_accumulator = 0.0f;
        m_interpolationAlpha = 0.0f;
        m_stepIndex = 0;
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
    float m_accumulator = 0.0f;
    float m_interpolationAlpha = 0.0f;
    std::uint64_t m_stepIndex = 0;

    btActionInterface* action;
};
// Удалите или закомментируйте строку ниже, так как она не нужна и вызывает ошибку E0077
// btActionInterface* action;
