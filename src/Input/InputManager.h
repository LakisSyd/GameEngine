
#pragma once
#include <functional>
#include "InputClass.h"
#include <memory>
#include <array>


class InputManager {
public:
    // События (подписка через lambda)
    std::function<void(int, int)> OnLeftClick;
    std::function<void(int, int)> OnLeftDragStart;
    std::function<void(int, int, int, int)> OnLeftDrag;
    std::function<void()> OnLeftDragEnd;

    void Update(InputClass* input); // изменено: не передаём владение

    // Состояние
    bool IsDragging() const { return m_isDragging; }
    bool IsCameraMode() const { return m_isRightDown; }
    int GetMouseX() const { return m_mouseX; }
    int GetMouseY() const { return m_mouseY; }
    int GetDragStartX() const { return m_clickStartX; }
    int GetDragStartY() const { return m_clickStartY; }

    

    void SetDragging(bool value) { m_isDragging = value; }
    bool IsLeftDown() const { return m_isLeftDown; }  // ← геттер для HandleDragDrop
    bool WasLeftPressed() const { return m_leftPressed; }

    void SetMouseCapture(InputClass* input, bool capture);

	//Get DELTA MOUSE position since last update (for camera rotation)

	void GetMouseDelta(int& dx, int& dy) const;

    void ResetMouseDelta(InputClass* input);

    // Проверить, захвачена ли мышь
    bool IsMouseCaptured(InputClass* input) const;

    // ===== 🔥 НОВОЕ: Запросы клавиш (для WASD/RF движения) =====
    bool IsKeyDown(InputClass* input, unsigned int key) const;

    bool IsKeyPressed(InputClass* input, unsigned int key) const;
    bool WasKeyPressed(unsigned int key) const
    {
        return key < m_pressedKeys.size() && m_pressedKeys[key];
    }

    // Сброс состояния при выходе из фокуса окна
    void Reset();

 
        
	

private:
    // Сырое состояние
    bool m_isLeftDown = false;
    bool m_isRightDown = false;
    bool m_wasLeftDown = false;
    bool m_leftPressed = false;
    bool m_isDragging = false;
    bool m_potentialDrag = false;

    std::array<bool, 256> m_previousKeys{};
    std::array<bool, 256> m_pressedKeys{};

    int m_mouseX = 0, m_mouseY = 0;
    int m_clickStartX = 0, m_clickStartY = 0;
};;
