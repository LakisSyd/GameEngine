#pragma once
#include <DirectXMath.h>
#include <vector>

struct AnimationComponent {
    int  animationIndex = 0;
    float currentTime = 0.0f;
    bool isPlaying = true;

    // Матрицы костей, вычисленные для ТЕКУЩЕГО КАДРА.
    // Размер будет совпадать с количеством костей в модели.
    std::vector<DirectX::XMFLOAT4X4> finalBoneMatrices;
};