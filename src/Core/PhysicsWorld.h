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

        // ===============================================================
        // ?? REGISTER GImpact algorithm for dynamic concave mesh collision
        // Without this, btGImpactMeshShape won't detect collisions!
        // ===============================================================
        btGImpactCollisionAlgorithm::registerAlgorithm(m_dispatcher.get());

        // ===============================================================
        // ?? FIX: ”величиваем итерации солвера дл€ стабильных джойнтов.
        //    ѕо умолчанию Bullet использует 10 итераций Ч этого мало
        //    дл€ btFixedConstraint между т€жЄлыми телами.
        //    50 итераций дают стабильные соединени€ отсеков подлодки.
        // ===============================================================
        m_dynamicsWorld->getSolverInfo().m_numIterations = 50;

        // ?? FIX: Split impulse предотвращает "отскок" от penetration
        //    correction. Ѕез этого тела, проникшие друг в друга,
        //    получают огромный импульс, который рвЄт джойнты.
        m_dynamicsWorld->getSolverInfo().m_splitImpulse = true;
        m_dynamicsWorld->getSolverInfo().m_splitImpulsePenetrationThreshold = -0.04f;

        // ?? FIX: ERP (Error Reduction Parameter) Ч скорость коррекции
        //    ошибки позиционировани€. —лишком высокое значение = жЄсткие
        //    рывки. 0.2 Ч м€гка€ коррекци€, не рвЄт джойнты.
        m_dynamicsWorld->getSolverInfo().m_erp = 0.2f;
        m_dynamicsWorld->getSolverInfo().m_erp2 = 0.2f;

        return true;
    }

    void Update(float deltaTime)
    {
        static float accumulator = 0.0f;
        const float timestep = 1.0f / 60.0f;
        accumulator += deltaTime;

        // ?? FIX: ќграничиваем максимальный accumulator,
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
// ”далите или закомментируйте строку ниже, так как она не нужна и вызывает ошибку E0077
// btActionInterface* action;
