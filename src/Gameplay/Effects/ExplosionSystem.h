#pragma once

#include <DirectXMath.h>

#include <ECSRegistry.h>

class ParticleSystem;

struct ExplosionSettings
{
    float radius = 6.0f;
    float impulse = 22.0f;
    float upwardBias = 0.15f;
    float particleScale = 1.25f;
};

class ExplosionSystem
{
public:
    // Создаёт визуальный эффект и раздаёт физические импульсы всем динамическим
    // RigidBody внутри радиуса. Тяжёлые предметы естественно разгоняются слабее.
    static int Emit(
        ECSRegistry& registry,
        ParticleSystem& particleSystem,
        const DirectX::XMFLOAT3& position,
        const ExplosionSettings& settings = {});
};
