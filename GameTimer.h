#pragma once

#include <algorithm>
#include <cmath>

// Игровые часы.
// Используются игровыми системами:
// AI, пожары, износ, задачи, события и т.д.
class GameTimer
{
public:

    float Update(float deltaTime)
    {
        const float safeDelta = std::isfinite(deltaTime)
            ? (std::max)(0.0f, deltaTime)
            : 0.0f;
        scaledDeltaSeconds = paused ? 0.0f : safeDelta * timeScale;
        totalSeconds += static_cast<double>(scaledDeltaSeconds);
        return scaledDeltaSeconds;
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
        timeScale = std::isfinite(scale)
            ? std::clamp(scale, 0.0f, 16.0f)
            : 1.0f;
    }

    bool IsPaused() const
    {
        return paused;
    }

    void SetPaused(bool value)
    {
        paused = value;
        if (paused)
            scaledDeltaSeconds = 0.0f;
    }

    float GetDeltaSeconds() const
    {
        return scaledDeltaSeconds;
    }

    void Reset(double startSeconds = 0.0)
    {
        totalSeconds = std::isfinite(startSeconds)
            ? (std::max)(0.0, startSeconds)
            : 0.0;
        scaledDeltaSeconds = 0.0f;
        timeScale = 1.0f;
        paused = false;
    }

private:

    double totalSeconds = 0.0;
    float scaledDeltaSeconds = 0.0f;
    float timeScale = 1.0f;
    bool paused = false;
};
