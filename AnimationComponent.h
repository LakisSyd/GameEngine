#pragma once
#include <DirectXMath.h>
#include <vector>

struct AnimationComponent {
    int  animationIndex = 0;
    float currentTime = 0.0f;
    bool isPlaying = true;

    // Ìàòðèöû êîñòåé, âû÷èñëåííûå äëÿ ÒÅÊÓÙÅÃÎ ÊÀÄÐÀ.
    // Ðàçìåð áóäåò ñîâïàäàòü ñ êîëè÷åñòâîì êîñòåé â ìîäåëè.
    std::vector<DirectX::XMFLOAT4X4> finalBoneMatrices;
};