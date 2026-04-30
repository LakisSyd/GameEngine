#include "InputClass.h"
#include <windows.h>  // Для SetCursorPos, GetClientRect
#include <stdio.h>

InputClass::InputClass()
{
    for (int i = 0; i < 256; ++i) m_keys[i] = false;
}

void InputClass::Initialize()
{
    for (int i = 0; i < 256; ++i) m_keys[i] = false;
    m_mouseX = m_mouseY = 0;
    m_mouseDeltaX = m_mouseDeltaY = 0;
    m_leftButtonDown = m_rightButtonDown = false;
    m_mouseCaptured = false;
}

// ... KeyDown/KeyUp/IsKeyDown без изменений ...

// 🔹 Методы мыши
void InputClass::SetMousePosition(int x, int y)
{
    m_mouseX = x;
    m_mouseY = y;
}

void InputClass::GetMousePosition(int& x, int& y) const
{
    x = m_mouseX;
    y = m_mouseY;
}

void InputClass::AddMouseDelta(int dx, int dy)
{
    m_mouseDeltaX += dx;
    m_mouseDeltaY += dy;
}

void InputClass::GetMouseDelta(int& dx, int& dy)
{
    dx = m_mouseDeltaX;
    dy = m_mouseDeltaY;
}

void InputClass::ResetMouseDelta()
{
    m_mouseDeltaX = 0;
    m_mouseDeltaY = 0;
}

void InputClass::DumpKeyState(unsigned int key) const
{
    
        char buf[256];
        sprintf_s(buf, "Key %c (0x%X) = %d\n", key, key, (key < 256) ? m_keys[key] : 0);
        OutputDebugStringA(buf);
    
}

// ===== InputClass.cpp =====

void InputClass::SetMouseCapture(bool capture)
{
    m_mouseCaptured = capture;

    if (capture) {
        // 🔹 СКРЫВАЕМ курсор при захвате
        while (ShowCursor(FALSE) >= 0); // Цикл гарантирует скрытие

        // Центрируем мышь
        RECT rect;
        GetClientRect(GetActiveWindow(), &rect);
        POINT center = {
            rect.left + (rect.right - rect.left) / 2,
            rect.top + (rect.bottom - rect.top) / 2
        };
        ClientToScreen(GetActiveWindow(), &center);
        SetCursorPos(center.x, center.y);

        ResetMouseDelta();
        m_mouseX = center.x;
        m_mouseY = center.y;
    }
    else {
        // 🔹 ПОКАЗЫВАЕМ курсор при освобождении
        while (ShowCursor(TRUE) < 0); // Цикл гарантирует появление
    }
}


void InputClass::KeyDown(unsigned int key)
{
    if (key < 256)
    {
        m_keys[key] = true;
        char buf[256];
        sprintf_s(buf, "[INPUT] KeyDown: %c (0x%X) from thread %d\n", key, key, GetCurrentThreadId());
        OutputDebugStringA(buf);
    }
}

void InputClass::KeyUp(unsigned int key)
{
    if (key < 256)
    {
        m_keys[key] = false;
        char buf[256];
        sprintf_s(buf, "[INPUT] KeyUp: %c (0x%X) from thread %d\n", key, key, GetCurrentThreadId());
        OutputDebugStringA(buf);
    }
}

bool InputClass::IsKeyDown(unsigned int key)
{
    return (key < 256) ? m_keys[key] : false;
}