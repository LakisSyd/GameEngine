#pragma once
#include <DirectXMath.h>
using namespace DirectX;

// Простой namespace с константами (вместо сложного класса)
namespace Settings {
    // === Ввод / Камера ===
    constexpr float CAMERA_SENSITIVITY = 0.15f;
    constexpr float CAMERA_TURN_SPEED = 1.0f;
    constexpr float INPUT_MOVE_SPEED = 0.1f;
    constexpr float INPUT_MOVE_SPEED_SHIFT = 0.2f;
    constexpr float INPUT_MOVE_SPEED_CTRL = 0.05f;
    constexpr int DRAG_THRESHOLD_PX = 5;
    constexpr float DRAG_DISTANCE_DEFAULT = 2.0f;

    // === Пикинг ===
    constexpr float LIGHT_SPHERE_RADIUS = 0.3f;
    constexpr float OBJECT_PICK_RADIUS = 1.5f;
    constexpr float SPRITE_RADIUS_FACTOR = 0.5f;


    // === Рендер ===
    constexpr float GIZMO_CLIP_SPACE_SIZE = 0.3f;
    constexpr bool VSYNC_ENABLED = true;
    constexpr bool FULLSCREEN = false;
    constexpr float SCREEN_DEPTH = 1000.0f;
    constexpr float SCREEN_NEAR = 0.3f;

    struct FogSettings {
        float FOG_START = 5.0f;
        float FOG_END = 50.0f;
		XMFLOAT3 color = XMFLOAT3(0.5f, 0.5f, 0.5f);
		bool enabled = false;
    };
}

