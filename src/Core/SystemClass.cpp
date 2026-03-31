// ============================================================================
// SystemClass.cpp — «корневой» класс: создаёт окно, запускает игровой цикл
// ============================================================================

#include "systemclass.h"
#include <windowsx.h>
#include "backends/imgui_impl_win32.h"
#include "TimeManager.h"
// Объявляем обработчик ImGui, определённый в imgui_impl_win32.cpp
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ============================================================================
// Конструктор
// Обнуляем все указатели явно.
// Если этого не сделать и Initialize() упадёт на середине —
// Shutdown() попытается удалить мусорные адреса → краш.
// ============================================================================
SystemClass::SystemClass()
{
    m_Input = nullptr;
    m_Application = nullptr;
}

// Пустые: копирование не поддерживается, очистка делается явно в Shutdown()
SystemClass::SystemClass(const SystemClass& other) {}
SystemClass::~SystemClass() {}

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
    int screenWidth = 0, screenHeight = 0;

    // Шаг 1: создаём окно, screenWidth/screenHeight заполняются функцией
    InitializeWindows(screenWidth, screenHeight);

    // Шаг 2: подсистема ввода — хранит состояние всех клавиш и кнопок мыши
    m_Input = std::make_unique<InputClass>();
    m_Input->Initialize();

    // Шаг 3: графическая подсистема — Direct3D, шейдеры, сцена, редактор
    m_Application = new ApplicationClass();
    bool result = m_Application->Initialize(screenWidth, screenHeight, m_hwnd);
    if (!result)
        return false;

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
        m_Application->Shutdown();
        delete m_Application;
        m_Application = nullptr;
    }

    // unique_ptr очистится сам, но явно обнуляем для читаемости
    if (m_Input)
        m_Input = nullptr;

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
            // Обработка хоткеев отладки
            if (msg.message == WM_KEYDOWN)
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

    // Escape — запросить выход из приложения
    if (m_Input->IsKeyDown(VK_ESCAPE))
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
            // Режим захвата: считаем дельту относительно центра окна,
            // затем возвращаем курсор в центр (FPS-управление)
            RECT rect;
            GetClientRect(hwnd, &rect);
            POINT center = {
                rect.left + (rect.right - rect.left) / 2,
                rect.top + (rect.bottom - rect.top) / 2
            };

            int deltaX = mouseX - center.x;
            int deltaY = mouseY - center.y;

            m_Input->AddMouseDelta(deltaX, deltaY);

            // Возвращаем курсор в центр — следующее WM_MOUSEMOVE снова даст дельту
            ClientToScreen(hwnd, &center);
            SetCursorPos(center.x, center.y);
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
    ApplicationHandle = this;

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
    ShowCursor(true);

    if (FULL_SCREEN)
        ChangeDisplaySettings(nullptr, 0); // вернуть исходное разрешение дисплея

    DestroyWindow(m_hwnd);
    m_hwnd = nullptr;

    UnregisterClass(m_applicationName, m_hinstance);
    m_hinstance = nullptr;

    ApplicationHandle = nullptr; // обнуляем глобальный указатель
}

// ============================================================================
// WndProc — глобальная точка входа для всех сообщений Windows
//
// Windows требует обычную C-функцию. Мы перенаправляем вызовы через
// глобальный указатель ApplicationHandle в метод MessageHandler.
// ImGui получает сообщения первым, чтобы обрабатывать клики по своим окнам.
// ============================================================================
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    // Даём ImGui первым обработать сообщение (мышь, клавиатура)
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam))
        return true;

    switch (msg)
    {
    case WM_DESTROY:
    case WM_CLOSE:
        // Пользователь закрыл окно — WM_QUIT поместится в очередь и Run() завершится
        PostQuitMessage(0);
        return 0;

    default:
        return ApplicationHandle->MessageHandler(hwnd, msg, wparam, lparam);
    }
}