#pragma once  // 💡 Современная замена #ifndef/#define — предотвращает повторное включение

// 📌 Директива для ускорения компиляции:
//    Исключает редко используемые WinAPI-функции из заголовков Windows
#define WIN32_LEAN_AND_MEAN

// ⚙️ Стандартные заголовки Windows для работы с окнами и сообщениями
#include <windows.h>

// 📌 Зависимости фреймворка:
//    InputClass  — обработка клавиатуры
//    ApplicationClass — будущая графика (DirectX)
#include "InputClass.h"
#include "ApplicationClass.h"
#include <memory>  // Для std::unique_ptr



//////////////////////////////////////////////////////////////////////////////// 
// 📌 SystemClass — "корневой" класс всего приложения
// 💡 Паттерн: один владеющий экземпляр на приложение.
// 🔜 В будущем: будет владеть устройством DirectX (ID3D11Device), swap chain и другими ресурсами
//////////////////////////////////////////////////////////////////////////////// 
class SystemClass
{
public:
    // 📌 Конструктор/деструктор
    SystemClass();
    SystemClass(const SystemClass&) = delete;
    SystemClass& operator=(const SystemClass&) = delete;
    ~SystemClass();

    // 📌 Публичный интерфейс для WinMain
    // Создаёт окно, инициализирует ввод и графику.
    // Возвращает false если хоть что-то не удалось инициализировать.
    bool Initialize();

    // Освобождает все ресурсы в безопасном порядке (графика → ввод → окно).
    void Shutdown();

    // Запускает главный цикл: обрабатывает сообщения Windows и вызывает Frame().
    // Работает до получения WM_QUIT или пока Frame() не вернёт false.
    void Run();

    // 📌 Обработчик сообщений Windows — маршрутизирует события в класс
    // 💡 Вызывается из глобальной функции WndProc (см. ниже)
    LRESULT CALLBACK MessageHandler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

private:
    // 📌 Внутренние методы
    // Обрабатывает один кадр: проверяет Escape и вызывает Application::Frame().
    bool Frame();

    // Регистрирует класс окна и создаёт окно нужного размера.
    // Записывает реальные размеры экрана в screenWidth / screenHeight.
    void InitializeWindows(int& screenWidth, int& screenHeight);

    // Уничтожает окно и снимает регистрацию класса окна.
    void ShutdownWindows();

    // ── Данные окна ──────────────────────────────────────────────────────────
    LPCWSTR   m_applicationName = nullptr;
    HINSTANCE m_hinstance = nullptr;
    HWND      m_hwnd = nullptr;

    // ── Подсистемы ──────────────────────────────────────────────────────────
    // SystemClass владеет InputClass через unique_ptr — автоочистка
    std::unique_ptr<InputClass> m_Input;

    std::unique_ptr<ApplicationClass> m_Application;
};
