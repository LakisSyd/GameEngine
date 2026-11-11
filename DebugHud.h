#pragma once

// DebugHud is a tiny, deliberately separate overlay for temporary development
// messages. It is not gameplay UI and does not save anything to a scene.
//
// Typical use from code running on the main thread:
//   DebugHud::Get().Show("Aim: Shoulder");
//   DebugHud::Get().Show("Ray missed", DebugHudMessageKind::Warning, 3.0f);
//
// Disable all new and existing messages:
//   DebugHud::Get().SetEnabled(false);
//
// Show() replaces the previous message. This keeps the tool predictable while
// debugging fast state changes and prevents an unbounded message queue.

#include <algorithm>
#include <cmath>
#include <string>
#include <windows.h>

#include "src/Render/Text/DWriteTextSystem.h"

enum class DebugHudMessageKind
{
    Info,
    Success,
    Warning,
    Error
};

class DebugHud final
{
public:
    // One shared debug overlay is enough for the application. The function-local
    // static is created on first use and avoids a global initialization order.
    static DebugHud& Get()
    {
        static DebugHud instance;
        return instance;
    }

    void SetEnabled(bool enabled)
    {
        m_enabled = enabled;
        if (!m_enabled)
            Clear();
    }

    bool IsEnabled() const { return m_enabled; }
    void Toggle() { SetEnabled(!m_enabled); }

    // durationSeconds is real frame time because ApplicationClass calls Update
    // with dt. A new message intentionally replaces the old one.
    void Show(
        std::string message,
        DebugHudMessageKind kind = DebugHudMessageKind::Info,
        float durationSeconds = 2.0f)
    {
        if (!m_enabled || message.empty())
            return;

        m_message = std::move(message);
        m_kind = kind;
        m_duration = (std::max)(0.05f, durationSeconds);
        m_timeRemaining = m_duration;
    }

    void Clear()
    {
        m_message.clear();
        m_duration = 0.0f;
        m_timeRemaining = 0.0f;
    }

    void Update(float deltaTime)
    {
        if (!m_enabled || m_message.empty() || !std::isfinite(deltaTime))
            return;

        m_timeRemaining -= (std::max)(0.0f, deltaTime);
        if (m_timeRemaining <= 0.0f)
            Clear();
    }

    void Render(
        DWriteTextSystem& text,
        float screenWidth,
        float screenHeight) const
    {
        if (!m_enabled || m_message.empty() ||
            screenWidth <= 0.0f || screenHeight <= 0.0f)
            return;

        constexpr float panelWidth = 520.0f;
        constexpr float panelHeight = 38.0f;
        const float width = (std::min)(panelWidth, (std::max)(80.0f, screenWidth - 24.0f));
        const float x = (std::max)(12.0f, (screenWidth - width) * 0.5f);
        // Keep this below GameHud's regular notification, which is drawn at y=72.
        const float y = (std::min)(116.0f, (std::max)(12.0f, screenHeight - panelHeight - 12.0f));

        float opacity = 1.0f;
        constexpr float fadeSeconds = 0.25f;
        if (m_timeRemaining < fadeSeconds)
            opacity = std::clamp(m_timeRemaining / fadeSeconds, 0.0f, 1.0f);

        const D2D1_COLOR_F accent = MessageColor(m_kind, opacity);
        text.BeginBatch();
        text.DrawFilledRect(
            x, y, width, panelHeight,
            D2D1::ColorF(0.01f, 0.02f, 0.03f, 0.92f * opacity));
        text.DrawRectOutline(x, y, width, panelHeight, 1.0f, accent);
        text.DrawFilledRect(x, y, 4.0f, panelHeight, accent);

        const std::wstring wideMessage = Utf8ToWide(m_message);
        text.DrawSmallTextLineColor(
            wideMessage.c_str(), x + 14.0f, y + 8.0f, width - 26.0f, accent);
        text.EndBatch();
    }

private:
    DebugHud() = default;

    static D2D1_COLOR_F MessageColor(DebugHudMessageKind kind, float opacity)
    {
        switch (kind)
        {
        case DebugHudMessageKind::Success:
            return D2D1::ColorF(0.30f, 0.90f, 0.68f, opacity);
        case DebugHudMessageKind::Warning:
            return D2D1::ColorF(1.00f, 0.72f, 0.22f, opacity);
        case DebugHudMessageKind::Error:
            return D2D1::ColorF(1.00f, 0.35f, 0.30f, opacity);
        case DebugHudMessageKind::Info:
        default:
            return D2D1::ColorF(0.42f, 0.86f, 0.94f, opacity);
        }
    }

    static std::wstring Utf8ToWide(const std::string& value)
    {
        if (value.empty())
            return {};

        const int length = MultiByteToWideChar(
            CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
        if (length <= 0)
            return L"<invalid UTF-8 debug message>";

        std::wstring result(static_cast<std::size_t>(length), L'\0');
        MultiByteToWideChar(
            CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
            result.data(), length);
        return result;
    }

    bool m_enabled = true;
    std::string m_message;
    DebugHudMessageKind m_kind = DebugHudMessageKind::Info;
    float m_duration = 0.0f;
    float m_timeRemaining = 0.0f;
};
