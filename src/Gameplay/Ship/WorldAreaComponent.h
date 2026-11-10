#pragma once
#include <DirectXMath.h>
#include <cstdint>
using namespace DirectX;

enum class WorldAreaType
{
    Grass,
    NoGrass,
    Forest,
    Spawn,
    Trigger
};

struct WorldAreaComponent
{
    WorldAreaType type = WorldAreaType::Grass;

    XMFLOAT3 center = { 0, 0, 0 };
    XMFLOAT3 size = { 10, 1, 10 };

    float density = 0.12f;
    uint32_t seed = 1337;

    bool enabled = true;
};

