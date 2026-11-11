#pragma once
#include <DirectXMath.h>
#include <vector>
#include <string>

enum class AnimationState
{
    Idle,
    Walk,
    Run,
    RifleIdle,
    RifleGun
};

struct AnimationBinding
{
    AnimationState state;
    std::string name;
    int index = -1;
};


struct AnimationComponent
{
    AnimationState currentState = AnimationState::Idle;

    int animationIndex = 0;
    float currentTime = 0.0f;
    bool isPlaying = true;

    std::vector<AnimationBinding> animations =
    {
        { AnimationState::Idle, "Idle" },
        { AnimationState::Walk, "Walk" },
        { AnimationState::Run,  "Run"  },
        { AnimationState::RifleIdle, "Rifle_Idle" },
        { AnimationState::RifleGun, "Rifle_Gun" }
    };

    std::vector<DirectX::XMFLOAT4X4> finalBoneMatrices;
};
