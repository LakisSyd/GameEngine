#include "systemclass.h"
#include <windowsx.h>

#include "backends/imgui_impl_win32.h"

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

//////////////////////////////////////////////////////////////////////////////// 
// 📌 Конструктор — инициализация указателей нулём
// 💡 ВАЖНО: если указатель не обнулить, а инициализация упадёт — в Shutdown()
//    произойдёт попытка удалить "мусорный" указатель → крах приложения.
//////////////////////////////////////////////////////////////////////////////// 
SystemClass::SystemClass()
{
    m_Input = nullptr;        // 💡 nullptr вместо 0 — современный C++ стиль
    m_Application = nullptr;
}

// 📌 Пустые копирующий конструктор и деструктор
// 💡 Почему деструктор пустой? См. Shutdown() ниже — очистка делается ЯВНО
SystemClass::SystemClass(const SystemClass& other) {}
SystemClass::~SystemClass() {}

//////////////////////////////////////////////////////////////////////////////// 
// 📌 Initialize() — запуск всего приложения
// 🔜 Порядок будущих этапов:
//    1. Создать окно Windows
//    2. Инициализировать ввод (клавиатура)
//    3. Инициализировать DirectX (урок 3+)
//    4. Загрузить ресурсы (шейдеры, текстуры — уроки 5+)
//////////////////////////////////////////////////////////////////////////////// 
bool SystemClass::Initialize()
{
    int screenWidth = 0, screenHeight = 0;
    bool result;

    // 📌 Шаг 1: Создать окно Windows и получить его размеры
    // ⚙️ Параметры передаются по ссылке — функция запишет туда реальные размеры
    InitializeWindows(screenWidth, screenHeight);

    // 📌 Шаг 2: Инициализировать подсистему ввода
    m_Input = new InputClass();
    m_Input->Initialize();  // Обнулить массив клавиш

    // 📌 Шаг 3: Инициализировать графическую подсистему
    // 💡 Пока пустая заглушка — в уроке 3 сюда приедет инициализация DirectX
    m_Application = new ApplicationClass();
    result = m_Application->Initialize(screenWidth, screenHeight, m_hwnd);
    if (!result)
    {
        return false;  // 🔜 Например, если не удалось создать устройство DirectX
    }

    m_Application->SetInput(m_Input);

    return true;
}

//////////////////////////////////////////////////////////////////////////////// 
// 📌 Shutdown() — безопасная очистка ресурсов
// 💡 Почему НЕ в деструкторе?
//    - Функции вроде ExitProcess() НЕ вызывают деструкторы → утечки
//    - Явный контроль порядка освобождения (сначала графика, потом окно)
//    - Возможность перезапуска подсистем без пересоздания объекта
//////////////////////////////////////////////////////////////////////////////// 
void SystemClass::Shutdown()
{
    // 📌 Порядок освобождения ВАЖЕН:
    //    1. Сначала графика (зависит от окна)
    //    2. Потом ввод
    //    3. В конце — само окно

    if (m_Application)
    {
        m_Application->Shutdown();  // 🔜 Освободит ресурсы DirectX (шейдеры, буферы)
        delete m_Application;
        m_Application = nullptr;
    }

    if (m_Input)
    {
        delete m_Input;  // Просто удаляем — внутри нет тяжёлых ресурсов
        m_Input = nullptr;
    }

    ShutdownWindows();  // Уничтожить окно последним
}

//////////////////////////////////////////////////////////////////////////////// 
// 📌 Run() — главный игровой цикл
// 💡 Паттерн "игровой цикл с фиксированным шагом" (упрощённая версия)
// ⚙️ Алгоритм:
//    Пока не выход:
//      1. Обработать системные сообщения Windows (мышь, фокус, закрытие)
//      2. Если не выход — выполнить логику кадра (ввод + рендеринг)
//////////////////////////////////////////////////////////////////////////////// 
void SystemClass::Run()
{
    MSG msg;      // Структура сообщения Windows
    bool done = false;

    // 📌 Обнулить структуру сообщения — хорошая практика безопасности
    ZeroMemory(&msg, sizeof(MSG));

    while (!done)
    {
        // 📌 PeekMessage — НЕБЛОКИРУЮЩАЯ проверка сообщений
        // 💡 Важно: GetMessage() БЛОКИРУЕТ поток до получения сообщения → игра "зависнет"
        //    Для игр нужен непрерывный цикл рендеринга → используем PeekMessage
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);   // Преобразовать виртуальные коды клавиш
            DispatchMessage(&msg);    // Отправить сообщение в WndProc
        }

        // 📌 Проверка выхода из приложения
        if (msg.message == WM_QUIT)
        {
            done = true;
        }
        else
        {
            // 📌 Выполнить логику одного кадра
            // 💡 Возвращает false если пользователь нажал Escape или произошла ошибка
            bool result = Frame();
            if (!result)
            {
                done = true;
            }
        }
    }
    Shutdown();
}

//////////////////////////////////////////////////////////////////////////////// 
// 📌 Frame() — обработка одного кадра
// 🔜 В будущем здесь будет:
//    - Обновление позиций объектов
//    - Проверка коллизий
//    - Вызов рендерера
//////////////////////////////////////////////////////////////////////////////// 
bool SystemClass::Frame()
{
    // 📌 Шаг 1: Проверить ввод пользователя
    // 💡 Паттерн "опрос состояния" (polling) вместо событийной модели
    //    Удобно для игр: в любой момент можно спросить "нажата ли клавиша?"
    if (m_Input->IsKeyDown(VK_ESCAPE))
    {
        return false;  // Пользователь хочет выйти
    }

    // 📌 Шаг 2: Передать управление графической подсистеме
    // 💡 ApplicationClass::Frame() вызовет Render() внутри себя
    return m_Application->Frame();
}

//////////////////////////////////////////////////////////////////////////////// 
// 📌 MessageHandler() — маршрутизация сообщений Windows в объектную модель
// 💡 Вызывается из глобальной WndProc через ApplicationHandle
//////////////////////////////////////////////////////////////////////////////// 
LRESULT CALLBACK SystemClass::MessageHandler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg)
    {
    case WM_KEYDOWN:
        m_Input->KeyDown(static_cast<unsigned int>(wparam));
        return 0;

    case WM_KEYUP:
        m_Input->KeyUp(static_cast<unsigned int>(wparam));
        return 0;

        // 🔹 Обработка мыши
    case WM_MOUSEMOVE:
    {
        int mouseX = GET_X_LPARAM(lparam);
        int mouseY = GET_Y_LPARAM(lparam);

        if (m_Input->IsMouseCaptured()) {
            // При захвате: считаем дельту относительно центра
            RECT rect;
            GetClientRect(hwnd, &rect);
            POINT center = {
                rect.left + (rect.right - rect.left) / 2,
                rect.top + (rect.bottom - rect.top) / 2
            };

            int deltaX = mouseX - center.x;
            int deltaY = mouseY - center.y;

            m_Input->AddMouseDelta(deltaX, deltaY);

            // Возвращаем курсор в центр
            ClientToScreen(hwnd, &center);
            SetCursorPos(center.x, center.y);
        }
        else {
            // Свободная мышь: просто обновляем позицию
            m_Input->SetMousePosition(mouseX, mouseY);
        }
        return 0;
    }

    case WM_LBUTTONDOWN:
        m_Input->MouseLeftDown();
        return 0;

    case WM_LBUTTONUP:
        m_Input->MouseLeftUp();
        return 0;

    case WM_RBUTTONDOWN:
        m_Input->MouseRightDown();
        return 0;

    case WM_RBUTTONUP:
        m_Input->MouseRightUp();
        return 0;

    default:
        return DefWindowProc(hwnd, msg, wparam, lparam);
    }
}

//////////////////////////////////////////////////////////////////////////////// 
// 📌 InitializeWindows() — создание окна через WinAPI
// 💡 Подводные камни:
//    - Стиль WS_POPUP убирает рамки — чистый холст для рендеринга
//    - BLACK_BRUSH — фон окна (временно, пока нет DirectX)
//    - ShowCursor(false) — скрываем системный курсор (в играх свой)
//////////////////////////////////////////////////////////////////////////////// 
void SystemClass::InitializeWindows(int& screenWidth, int& screenHeight)
{
    WNDCLASSEX wc;        // Описание класса окна
    DEVMODE dmSettings;   // Настройки дисплея (для полноэкранного режима)
    int posX, posY;       // Позиция окна на экране

    // 📌 Сохранить указатель на текущий объект для маршрутизации сообщений
    // 💡 Это "мост" между процедурным стилем WinAPI и ООП
    ApplicationHandle = this;

    // 📌 Получить дескриптор текущего экземпляра приложения
    m_hinstance = GetModuleHandle(nullptr);

    // 📌 Имя приложения — используется и как заголовок окна, и как имя класса
    m_applicationName = L"Engine";  // L — префикс для wide-string (Unicode)

    // 📌 Настроить класс окна
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    // 💡 CS_OWNDC — каждое окно получает СОБСТВЕННЫЙ контекст устройства (DC)
    //    Критически важно для DirectX — иначе могут быть артефакты при рендеринге

    wc.lpfnWndProc = WndProc;          // 📌 Глобальная функция-обработчик
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = m_hinstance;
    wc.hIcon = LoadIcon(nullptr, IDI_WINLOGO);  // Стандартная иконка Windows
    wc.hIconSm = wc.hIcon;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)); // Чёрный фон
    wc.lpszMenuName = nullptr;
    wc.lpszClassName = m_applicationName;
    wc.cbSize = sizeof(WNDCLASSEX);

    // 📌 Зарегистрировать класс окна в системе
    RegisterClassEx(&wc);

    // 📌 Определить разрешение РАБОЧЕГО СТОЛА пользователя
    screenWidth = GetSystemMetrics(SM_CXSCREEN);
    screenHeight = GetSystemMetrics(SM_CYSCREEN);

    // 📌 Настроить окно в зависимости от режима (полноэкранный / оконный)
    if (FULL_SCREEN)
    {
        // 📌 Полноэкранный режим — меняем настройки дисплея системы
        ZeroMemory(&dmSettings, sizeof(dmSettings));
        dmSettings.dmSize = sizeof(dmSettings);
        dmSettings.dmPelsWidth = static_cast<DWORD>(screenWidth);
        dmSettings.dmPelsHeight = static_cast<DWORD>(screenHeight);
        dmSettings.dmBitsPerPel = 32;  // 32-битный цвет (стандарт)
        dmSettings.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;

        ChangeDisplaySettings(&dmSettings, CDS_FULLSCREEN);
        posX = posY = 0;  // Верхний левый угол
    }
    else
    {
        // 📌 Оконный режим — фиксированный размер 800x600 по центру экрана
        screenWidth = 800;
        screenHeight = 600;
        posX = (GetSystemMetrics(SM_CXSCREEN) - screenWidth) / 2;
        posY = (GetSystemMetrics(SM_CYSCREEN) - screenHeight) / 2;
    }

    // 📌 Создать окно
    // 💡 Стили:
    //    WS_EX_APPWINDOW — окно появится в панели задач
    //    WS_CLIPSIBLINGS | WS_CLIPCHILDREN — предотвращают артефакты наложения
    //    WS_POPUP — без рамок и заголовка (чистый холст для рендера)
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

    // 📌 Показать окно и активировать его
    ShowWindow(m_hwnd, SW_SHOW);
    SetForegroundWindow(m_hwnd);
    SetFocus(m_hwnd);

    // 📌 Скрыть системный курсор — в играх обычно используется кастомный
    ShowCursor(true);
}

//////////////////////////////////////////////////////////////////////////////// 
// 📌 ShutdownWindows() — обратные действия к InitializeWindows()
//////////////////////////////////////////////////////////////////////////////// 
void SystemClass::ShutdownWindows()
{
    ShowCursor(true);  // Вернуть курсор

    if (FULL_SCREEN)
    {
        // 📌 Вернуть настройки дисплея к стандартным
        ChangeDisplaySettings(nullptr, 0);
    }

    DestroyWindow(m_hwnd);
    m_hwnd = nullptr;

    UnregisterClass(m_applicationName, m_hinstance);
    m_hinstance = nullptr;

    ApplicationHandle = nullptr;  // 📌 Обнулить глобальный указатель — безопасность
}

//////////////////////////////////////////////////////////////////////////////// 
// 📌 WndProc — глобальная функция, вызываемая Windows напрямую
// 💡 Это "точка входа" для всех сообщений окна. Отсюда мы маршрутизируем
//    в объектно-ориентированный MessageHandler через глобальный указатель.
//////////////////////////////////////////////////////////////////////////////// 
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
   

    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam))
        return true;

    switch (msg)
    {
    case WM_DESTROY:
    case WM_CLOSE:
        // 📌 Пользователь закрыл окно — запросить выход из приложения
        PostQuitMessage(0);  // Поместит WM_QUIT в очередь сообщений
        return 0;

    default:
        // 📌 Маршрутизация в объектную модель
        // 💡 ApplicationHandle должен быть установлен в InitializeWindows()
        return ApplicationHandle->MessageHandler(hwnd, msg, wparam, lparam);
    }
}