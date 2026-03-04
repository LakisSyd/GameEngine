#include "SystemClass.h"
#include <windows.h>

// 📌 WinMain — стандартная точка входа для оконных приложений Windows
// ⚙️ Параметры:
//   hInstance     — дескриптор текущего экземпляра приложения
//   hPrevInstance — всегда NULL в современных Windows (устаревший параметр)
//   pCmdLine      — командная строка запуска
//   nCmdShow      — как показать окно (свернутое/развёрнутое)

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR pCmdLine, int nCmdShow)
{
    // 💡 DPI Awareness — отключаем автоматическое масштабирование Windows
    //    Без этого окно может "поплыть" на мониторах с высоким DPI (4K и т.д.)
    //    Важно для точного контроля над размерами рендера в DirectX
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);

    // 📌 Создаём основной объект системы — он управляет ВСЕМ приложением
    // ⚙️ Используем стековый объект (не через new), чтобы избежать утечек памяти
    //    даже при аварийном завершении (исключения, аварийные выходы)
    SystemClass system;

    // 📌 Шаг 1: Инициализация — создаёт окно, подсистемы ввода и графики
    bool result = system.Initialize();
    if (result)
    {
        // 📌 Шаг 2: Главный цикл приложения — работает пока пользователь не выйдет
        system.Run();
        
    }

    // 📌 Шаг 3: Очистка ресурсов — освобождает окно, устройства DirectX, память
    // 💡 ВАЖНО: очистка вызывается ЯВНО, а не в деструкторе (см. systemclass.cpp)
    system.Shutdown();

    return 0;
}