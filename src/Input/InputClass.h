#pragma once

class InputClass
{
public:
    InputClass();
    InputClass(const InputClass&) = default;
    ~InputClass() = default;

    void Initialize();
    void KeyDown(unsigned int);
    void KeyUp(unsigned int);
    bool IsKeyDown(unsigned int);

    // 🔹 Мышь: состояние кнопок
    void MouseLeftDown() { m_leftButtonDown = true; }
    void MouseLeftUp() { m_leftButtonDown = false; }
    void MouseRightDown() { m_rightButtonDown = true; }
    void MouseRightUp() { m_rightButtonDown = false; }

    bool IsLeftButtonDown() const { return m_leftButtonDown; }
    bool IsRightButtonDown() const { return m_rightButtonDown; }

    // 🔹 Мышь: позиция и дельта
    void SetMousePosition(int x, int y);
    void GetMousePosition(int& x, int& y) const;
    void AddMouseDelta(int dx, int dy);
    void GetMouseDelta(int& dx, int& dy);
    void ResetMouseDelta();

    void DumpKeyState(unsigned int key) const;
   

    // 🔹 Захват мыши
    void SetMouseCapture(bool capture);
    bool IsMouseCaptured() const { return m_mouseCaptured; }

private:
    bool m_keys[256];
    
    // 🔹 Состояние мыши
    int m_mouseX = 0, m_mouseY = 0;
    int m_mouseDeltaX = 0, m_mouseDeltaY = 0;
    bool m_leftButtonDown = false;
    bool m_rightButtonDown = false;
    bool m_mouseCaptured = false;
};