#pragma once

#include "cameraclass.h"
#include "InputManager.h"
#include "inputclass.h"

class EditorUI;   // forward declare, чтобы не тянуть тяжёлый заголовок

class CameraController
{
public:
    CameraController(CameraClass* camera,
        InputManager* inputManager,
        InputClass* input,
        EditorUI* editorUI);

    /// Вызывать каждый кадр из ApplicationClass::Frame
    void Update(float dt);

private:
    CameraClass* m_camera;
    InputManager* m_inputManager;
    InputClass* m_input;
    EditorUI* m_editorUI;
};
