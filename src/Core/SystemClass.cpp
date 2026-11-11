// ============================================================================
// SystemClass.cpp — «корневой» класс: создаёт окно, запускает игровой цикл
// ============================================================================

#include "systemclass.h"
#include <windowsx.h>
#include "backends/imgui_impl_win32.h"
#include "TimeManager.h"
// Объявляем обработчик ImGui, определённый в imgui_impl_win32.cpp
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace
{
    SystemClass* g_applicationHandle = nullptr;
}
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

// ============================================================================
// Конструктор
// Обнуляем все указатели явно.
// Если этого не сделать и Initialize() упадёт на середине —
// Shutdown() попытается удалить мусорные адреса → краш.
// ============================================================================
SystemClass::SystemClass() = default;
SystemClass::~SystemClass() { Shutdown(); }

// ============================================================================
// Initialize — запуск всего приложения
//
// Порядок инициализации:
//   1. Создать окно Windows и узнать его размеры
//   2. Создать подсистему ввода
//   3. Создать и инициализировать графическую подсистему (ApplicationClass)
//   4. Передать указатель на InputClass в Application
// ============================================================================
bool SystemClass::Initialize()
{
    // Make retrying initialization deterministic after either a successful
    // run or a partial failure.
    Shutdown();

    int screenWidth = 0, screenHeight = 0;

    // Шаг 1: создаём окно, screenWidth/screenHeight заполняются функцией
    InitializeWindows(screenWidth, screenHeight);
    if (!m_hwnd)
    {
        Shutdown();
        return false;
    }

    // Шаг 2: подсистема ввода — хранит состояние всех клавиш и кнопок мыши
    m_Input = std::make_unique<InputClass>();
    m_Input->Initialize();

    // Шаг 3: графическая подсистема — Direct3D, шейдеры, сцена, редактор
    m_Application = std::make_unique<ApplicationClass>();
    bool result = m_Application->Initialize(screenWidth, screenHeight, m_hwnd);
    if (!result)
    {
        Shutdown();
        return false;
    }

    // Шаг 4: ApplicationClass не владеет InputClass — передаём не-владеющий указатель
    m_Application->SetInput(m_Input.get());

    return true;
}

// ============================================================================
// Shutdown — безопасная очистка в обратном порядке
//
// Порядок важен:
//   1. Сначала Application (зависит от окна и ввода)
//   2. Потом Input
//   3. В конце само окно
//
// Почему не в деструкторе?
    // ExitProcess() не вызывает деструкторы → утечки памяти
//   - Явный порядок освобождения легче контролировать
// ============================================================================
void SystemClass::Shutdown()
{
    if (m_Application)
    {
        m_Application.reset();
    }

    // unique_ptr очистится сам, но явно обнуляем для читаемости
    m_Input.reset();

    ShutdownWindows();
}

// ============================================================================
// Run — главный игровой цикл
//
// Используем PeekMessage (неблокирующую проверку сообщений), а не GetMessage,
// которая блокирует поток до следующего события — в игре нам нужен
// непрерывный цикл рендеринга.
//
// Алгоритм каждой итерации:
//   1. Обработать все накопившиеся сообщения Windows
//   2. Проверить WM_QUIT → завершить цикл
//   3. Иначе → обработать один кадр игры
// ============================================================================
void SystemClass::Run()
{
    MSG msg = {};
    bool done = false;

    while (!done)
    {
        // 1. Обновляем время (всегда!)
        TimeManager::Instance().UpdateFrame();

        // 2. Обработка сообщений Windows
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            // Editor-only time controls. In Play, Space belongs to Jump.
            if (msg.message == WM_KEYDOWN &&
                m_Application && m_Application->IsEditorMode())
            {
                switch (msg.wParam)
                {
                case VK_SPACE: // Пауза / Продолжить
                    if (TimeManager::GetState() == TimeState::Running)
                        TimeManager::SetState(TimeState::Paused);
                    else
                        TimeManager::SetState(TimeState::Running);
                    break;

                case 'K': // Шаг кадра (только в паузе/степ-моде)
                    if (TimeManager::GetState() != TimeState::Running)
                    {
                        TimeManager::SetState(TimeState::StepMode);
                        TimeManager::Instance().StepFrame();
                    }
                    break;
                }
            }

            if (msg.message == WM_QUIT) done = true;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // 3. Игровой цикл
        if (!Frame()) done = true;

        // 4. Сброс флага шага после кадра
        TimeManager::Instance().ResetStep();
    }
}


// ============================================================================
// Frame — логика одного кадра
//
// Сначала проверяем ввод, потом передаём управление графической подсистеме.
// ApplicationClass::Frame() внутри себя вызывает рендер.
// ============================================================================
bool SystemClass::Frame()
{
    float dt = TimeManager::DeltaTime(); // Получаем "игровое" время

    // In Play, Escape closes an interaction panel through GameInputFrame.
    // Window close still exits from either mode.
    if (m_Application && m_Application->IsEditorMode() &&
        m_Input->IsKeyDown(VK_ESCAPE))
        return false;

    // Обновление и рендер одного кадра в ApplicationClass
    return m_Application->Frame(dt);
}

// ============================================================================
// MessageHandler — маршрутизация сообщений Windows в объектную модель
//
// Вызывается из глобальной WndProc через указатель ApplicationHandle.
// Здесь обрабатываем только то, что нужно нашему движку:
// клавиатура и мышь. Всё остальное уходит в DefWindowProc.
// ============================================================================
LRESULT CALLBACK SystemClass::MessageHandler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg)
    {
        // ── Клавиатура ────────────────────────────────────────────────────────────
    case WM_KEYDOWN:
        m_Input->KeyDown(static_cast<unsigned int>(wparam));
        return 0;

    case WM_KEYUP:
        m_Input->KeyUp(static_cast<unsigned int>(wparam));
        return 0;

        // ── Мышь ─────────────────────────────────────────────────────────────────
    case WM_MOUSEMOVE:
    {
        int mouseX = GET_X_LPARAM(lparam);
        int mouseY = GET_Y_LPARAM(lparam);

        if (m_Input->IsMouseCaptured())
        {
            // Captured mouse is sampled once per frame by InputClass. Re-centering
            // from WM_MOUSEMOVE creates extra WM_MOUSEMOVE messages and can flood
            // the message queue while rotating and moving the camera.
            return 0;
        }
        else
        {
            // Обычный режим: просто запоминаем координаты для UI / picking
            m_Input->SetMousePosition(mouseX, mouseY);
        }
        return 0;
    }

    case WM_LBUTTONDOWN: m_Input->MouseLeftDown();  return 0;
    case WM_LBUTTONUP:   m_Input->MouseLeftUp();    return 0;
    case WM_RBUTTONDOWN: m_Input->MouseRightDown(); return 0;
    case WM_RBUTTONUP:   m_Input->MouseRightUp();   return 0;
    case WM_MBUTTONDOWN: m_Input->MouseMiddleDown(); return 0;
    case WM_MBUTTONUP:   m_Input->MouseMiddleUp();   return 0;

    default:
        return DefWindowProc(hwnd, msg, wparam, lparam);
    }
}

// ============================================================================
// InitializeWindows — создание окна WinAPI
//
// Регистрирует класс окна, создаёт окно нужного размера (полный экран / оконный)
// и показывает его. Стиль WS_POPUP убирает рамку и заголовок.
// ============================================================================
void SystemClass::InitializeWindows(int& screenWidth, int& screenHeight)
{
    WNDCLASSEX wc;
    DEVMODE    dmSettings;
    int posX, posY;

    // Сохраняем указатель на себя в глобальной переменной —
    // это «мост» между WinAPI (C-функции) и ООП (методы класса)
    g_applicationHandle = this;

    m_hinstance = GetModuleHandle(nullptr);
    m_applicationName = L"Engine";

    // Настройка класса окна
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    // CS_OWNDC: каждое окно получает свой Device Context — критично для DirectX
    wc.lpfnWndProc = WndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = m_hinstance;
    wc.hIcon = LoadIcon(nullptr, IDI_WINLOGO);
    wc.hIconSm = wc.hIcon;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.lpszMenuName = nullptr;
    wc.lpszClassName = m_applicationName;
    wc.cbSize = sizeof(WNDCLASSEX);

    RegisterClassEx(&wc);

    int desktopWidth = GetSystemMetrics(SM_CXSCREEN);
    int desktopHeight = GetSystemMetrics(SM_CYSCREEN);

    if (FULL_SCREEN)
    {
        // Полноэкранный режим: меняем разрешение дисплея и занимаем весь экран
        screenWidth = desktopWidth;
        screenHeight = desktopHeight;

        ZeroMemory(&dmSettings, sizeof(dmSettings));
        dmSettings.dmSize = sizeof(dmSettings);
        dmSettings.dmPelsWidth = static_cast<DWORD>(screenWidth);
        dmSettings.dmPelsHeight = static_cast<DWORD>(screenHeight);
        dmSettings.dmBitsPerPel = 32;
        dmSettings.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;

        ChangeDisplaySettings(&dmSettings, CDS_FULLSCREEN);
        posX = posY = 0;
    }
    else
    {
        // Оконный режим: ограничиваем желаемый размер размером рабочего стола
        screenWidth = 1920;
        screenHeight = 1280;

        if (screenWidth > desktopWidth)  screenWidth = desktopWidth;
        if (screenHeight > desktopHeight) screenHeight = desktopHeight;

        // Центрируем окно (но не уходим за границы экрана)
        posX = (desktopWidth - screenWidth) / 2;
        posY = (desktopHeight - screenHeight) / 2;
        if (posX < 0) posX = 0;
        if (posY < 0) posY = 0;
    }

    // WS_POPUP: окно без рамки и заголовка — чистый холст для рендера
    m_hwnd = CreateWindowEx(
        WS_EX_APPWINDOW,
        m_applicationName,
        m_applicationName,
        WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_POPUP,
        posX, posY,
        screenWidth, screenHeight,
        nullptr, nullptr,
        m_hinstance,
        nullptr
    );

    ShowWindow(m_hwnd, SW_SHOW);
    SetForegroundWindow(m_hwnd);
    SetFocus(m_hwnd);

    ShowCursor(true); // системный курсор; в будущем можно заменить кастомным
}

// ============================================================================
// ShutdownWindows — обратные действия к InitializeWindows
// ============================================================================
void SystemClass::ShutdownWindows()
{
    if (!m_hwnd && !m_hinstance && !m_applicationName)
    {
        if (g_applicationHandle == this)
            g_applicationHandle = nullptr;
        return;
    }

    ShowCursor(true);

    if (FULL_SCREEN)
        ChangeDisplaySettings(nullptr, 0); // вернуть исходное разрешение дисплея

    if (m_hwnd)
    {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }

    if (m_applicationName && m_hinstance)
        UnregisterClass(m_applicationName, m_hinstance);
    m_applicationName = nullptr;
    m_hinstance = nullptr;

    if (g_applicationHandle == this)
        g_applicationHandle = nullptr;
}

// ============================================================================
// WndProc — глобальная точка входа для всех сообщений Windows
//
// Windows требует обычную C-функцию. Мы перенаправляем вызовы через
// глобальный указатель ApplicationHandle в метод MessageHandler.
// ImGui получает сообщения первым, чтобы обрабатывать клики по своим окнам.
// ============================================================================
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    // ImGui must see every message, but keyboard state must also reach the
    // gameplay input layer. Otherwise Shift/WASD can remain invisible while
    // a runtime ImGui window is active.
    const bool imguiHandled =
        ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam) != 0;
    const bool keyboardMessage =
        msg == WM_KEYDOWN || msg == WM_KEYUP;
    if (keyboardMessage && g_applicationHandle)
        return g_applicationHandle->MessageHandler(hwnd, msg, wparam, lparam);

    if (imguiHandled)
        return true;

    switch (msg)
    {
    case WM_DESTROY:
    case WM_CLOSE:
        // Пользователь закрыл окно — WM_QUIT поместится в очередь и Run() завершится
        PostQuitMessage(0);
        return 0;

    default:
        return g_applicationHandle
            ? g_applicationHandle->MessageHandler(hwnd, msg, wparam, lparam)
            : DefWindowProc(hwnd, msg, wparam, lparam);
    }
}
