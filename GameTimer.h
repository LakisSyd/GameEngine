#pragma once

// Игровые часы.
// Используются игровыми системами:
// AI, пожары, износ, задачи, события и т.д.
class GameTimer
{
public:

    void Update(float deltaTime)
    {
        if (!paused)
        {
            totalSeconds +=
                static_cast<double>(deltaTime * timeScale);
        }
    }

    double GetTotalSeconds() const
    {
        return totalSeconds;
    }

    float GetTimeScale() const
    {
        return timeScale;
    }

    void SetTimeScale(float scale)
    {
        timeScale = scale;
    }

    bool IsPaused() const
    {
        return paused;
    }

    void SetPaused(bool value)
    {
        paused = value;
    }

private:

    double totalSeconds = 0.0;
    float timeScale = 1.0f;
    bool paused = false;
};