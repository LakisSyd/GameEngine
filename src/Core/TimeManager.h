#include <winnt.h>
#include <profileapi.h>

enum class TimeState
{
    Running,
    Paused,
    StepMode
};

class TimeManager
{
public:
    // === Singleton Access ===
    static TimeManager& Instance()
    {
        static TimeManager instance; // Meyers' singleton (thread-safe в C++11+)
        return instance;
    }

    // === Public API ===
    void UpdateFrame()
    {
        LARGE_INTEGER currentTime;
        QueryPerformanceCounter(&currentTime);

        // Считаем реальное время (всегда, даже на паузе)
        float realDt = CalculateDelta(m_lastTime, currentTime);
        m_lastTime = currentTime;

        // Логика масштабирования времени
        switch (m_state)
        {
        case TimeState::Running:
            m_gameDt = realDt * m_timeScale;
            break;

        case TimeState::StepMode:
            // В StepMode время не течёт само, ждём вызова Step()
            m_gameDt = 0.0f;
            break;

        case TimeState::Paused:
        default:
            m_gameDt = 0.0f;
            break;
        }

        m_unscaledDt = realDt;
    }

    // Продвинуть игру на один фиксированный кадр (для пошаговой отладки)
    void StepFrame(float fixedDeltaTime = 1.0f / 60.0f)
    {
        if (m_state == TimeState::StepMode)
        {
            m_gameDt = fixedDeltaTime;
            m_wasStepped = true; // Флаг, что мы сделали шаг вручную
        }
    }

    // Сбросить дельту после шага (чтобы не применять её дважды)
    void ResetStep()
    {
        if (m_wasStepped)
        {
            m_gameDt = 0.0f;
            m_wasStepped = false;
        }
    }

    // === Геттеры ===
    static float DeltaTime() { return Instance().m_gameDt; }
    static float UnscaledDeltaTime() { return Instance().m_unscaledDt; }
    static void SetState(TimeState s) { Instance().m_state = s; }
    static TimeState GetState() { return Instance().m_state; }
    static void SetTimeScale(float s) { Instance().m_timeScale = s; }

private:
    // === Конструктор (инициализация) ===
    TimeManager()
        : m_gameDt(0.0f)
        , m_unscaledDt(0.0f)
        , m_timeScale(1.0f)
        , m_state(TimeState::Running)
        , m_wasStepped(false)
    {
        // Инициализируем lastTime текущим значением, 
        // чтобы первый кадр не дал огромную дельту
        QueryPerformanceCounter(&m_lastTime);

        // Кэшируем частоту для оптимизации
        QueryPerformanceFrequency(&m_frequency);
    }

    // Запрещаем копирование синглтона
    TimeManager(const TimeManager&) = delete;
    TimeManager& operator=(const TimeManager&) = delete;

    // === Вспомогательная функция (использует кэшированную частоту) ===
    inline float CalculateDelta(LARGE_INTEGER start, LARGE_INTEGER end) const
    {
        return static_cast<float>(end.QuadPart - start.QuadPart)
            / static_cast<float>(m_frequency.QuadPart);
    }

    // === Данные ===
    float m_gameDt;
    float m_unscaledDt;
    float m_timeScale;
    TimeState m_state;
    bool m_wasStepped; // Флаг для отслеживания ручного шага

    LARGE_INTEGER m_lastTime;
    mutable LARGE_INTEGER m_frequency; // mutable, т.к. используется в const-функции
};