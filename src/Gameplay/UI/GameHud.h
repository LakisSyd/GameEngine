#pragma once

#include <algorithm>
#include <cwchar>
#include <string>
#include <windows.h>

#include "DWriteTextSystem.h"
#include "Components.h"
#include "ECSRegistry.h"
#include "InputManager.h"
#include "PlayerComponents.h"

#include "Gameplay/Ship/RoomCullingSystem.h"
#include "Gameplay/Ship/ShipAlertSystem.h"

#include "../../WorldEventLog.h"

struct GameHudStats
{
    float frameMs = 0.0f;
    float renderCpuMs = 0.0f;

    int activeMeshObjects = 0;
    int mainRenderedObjects = 0;
    int shadowRenderedObjects = 0;
};

class GameHud
{
public:

    void Render(
        DWriteTextSystem& text,
        ECSRegistry& registry,
        Entity player,
        float deltaTime,
        float screenWidth,
        float screenHeight,
        const InputManager& input,
        const WorldEventLog& eventLog,
        const GameHudStats& stats = {})
    {
        UpdateFps(deltaTime);

        if (input.WasKeyPressed(VK_F3))
            m_showDebug = !m_showDebug;

        if (input.WasKeyPressed(VK_TAB))
            m_showEventLog = !m_showEventLog;

        text.BeginBatch();

        const auto* playerComp =
            player != INVALID_ENTITY
            ? registry.GetComponent<PlayerComponent>(player)
            : nullptr;

        const RoomComponent* playerRoom =
            FindPlayerRoom(registry, player);

        if (playerComp)
        {
            DrawPlayerVitals(
                text,
                *playerComp,
                screenWidth,
                screenHeight);
        }

        DrawShipAlert(
            text,
            screenWidth);

        if (m_showDebug)
        {
            DrawDebug(
                text,
                playerComp,
                playerRoom,
                stats);
        }

        if (m_showEventLog)
        {
            DrawEventLog(
                text,
                registry,
                eventLog,
                screenWidth,
                screenHeight);
        }

        text.EndBatch();
    }

private:

    static const RoomComponent* FindPlayerRoom(
        ECSRegistry& registry,
        Entity player)
    {
        if (player == INVALID_ENTITY)
            return nullptr;

        const RoomComponent* result = nullptr;

        registry.ForEach<RoomComponent>(
            [&](Entity, RoomComponent& room)
            {
                if (result)
                    return;

                for (Entity object : room.objectsInRoom)
                {
                    if (object == player)
                    {
                        result = &room;
                        return;
                    }
                }
            });

        return result;
    }

    static float Ratio(
        float value,
        float maximum)
    {
        return maximum > 0.0f
            ? std::clamp(value / maximum, 0.0f, 1.0f)
            : 0.0f;
    }

    static std::wstring FormatFixed1(float value)
    {
        wchar_t buffer[32]{};

        swprintf_s(
            buffer,
            L"%.1f",
            value);

        return buffer;
    }

    static void DrawPlayerVitals(
        DWriteTextSystem& text,
        const PlayerComponent& player,
        float screenWidth,
        float screenHeight)
    {
        constexpr float labelWidth = 66.0f;
        constexpr float barWidth = 128.0f;
        constexpr float barHeight = 6.0f;

        const float x =
            (std::max)(
                20.0f,
                screenWidth - labelWidth - barWidth - 28.0f);

        const float y =
            (std::max)(
                20.0f,
                screenHeight - 76.0f);

        const float healthRatio =
            Ratio(
                player.health,
                player.maxHealth);

        const float staminaRatio =
            Ratio(
                player.stamina,
                player.maxStamina);

        DrawVital(
            text,
            L"HEALTH",
            x,
            y,
            labelWidth,
            barWidth,
            barHeight,
            healthRatio,
            healthRatio > 0.35f
            ? D2D1::ColorF(
                0.86f,
                0.90f,
                0.88f,
                1.0f)
            : D2D1::ColorF(
                0.96f,
                0.24f,
                0.18f,
                1.0f));

        DrawVital(
            text,
            L"STAMINA",
            x,
            y + 30.0f,
            labelWidth,
            barWidth,
            barHeight,
            staminaRatio,
            staminaRatio > 0.20f
            ? D2D1::ColorF(
                0.28f,
                0.82f,
                0.65f,
                1.0f)
            : D2D1::ColorF(
                0.95f,
                0.61f,
                0.22f,
                1.0f));
    }

    static void DrawVital(
        DWriteTextSystem& text,
        const wchar_t* label,
        float x,
        float y,
        float labelWidth,
        float barWidth,
        float barHeight,
        float ratio,
        const D2D1_COLOR_F& color)
    {
        text.DrawSmallTextLineColor(
            label,
            x,
            y - 8.0f,
            labelWidth,
            D2D1::ColorF(
                0.72f,
                0.78f,
                0.78f,
                0.92f));

        text.DrawFilledRect(
            x + labelWidth,
            y,
            barWidth,
            barHeight,
            D2D1::ColorF(
                0.04f,
                0.06f,
                0.065f,
                0.78f));

        text.DrawFilledRect(
            x + labelWidth,
            y,
            barWidth * std::clamp(ratio, 0.0f, 1.0f),
            barHeight,
            color);
    }

    static void DrawShipAlert(
        DWriteTextSystem& text,
        float screenWidth)
    {
        const auto& alert =
            ShipAlertSystem::GetCurrentAlert();

        if (alert.level == ShipAlertLevel::None ||
            !alert.message ||
            !alert.message[0])
        {
            return;
        }

        constexpr float width = 286.0f;
        constexpr float height = 26.0f;

        const float x =
            (std::max)(
                18.0f,
                screenWidth * 0.5f - width * 0.5f);

        constexpr float y = 34.0f;

        D2D1_COLOR_F accent =
            D2D1::ColorF(
                0.86f,
                0.66f,
                0.24f,
                1.0f);

        if (alert.level == ShipAlertLevel::Critical)
        {
            accent =
                D2D1::ColorF(
                    0.96f,
                    0.22f,
                    0.17f,
                    1.0f);
        }
        else if (alert.level == ShipAlertLevel::Warning)
        {
            accent =
                D2D1::ColorF(
                    0.98f,
                    0.46f,
                    0.18f,
                    1.0f);
        }

        text.DrawFilledRect(
            x,
            y,
            width,
            height,
            D2D1::ColorF(
                0.025f,
                0.035f,
                0.04f,
                0.88f));

        text.DrawFilledRect(
            x,
            y,
            4.0f,
            height,
            accent);

        text.DrawSmallTextLineColor(
            alert.message,
            x + 14.0f,
            y + 3.0f,
            width - 20.0f,
            accent);
    }

    static const wchar_t* WorldEventTypeToString(WorldEventType type)
    {
        switch (type)
        {
        case WorldEventType::BreachOpened:
            return L"BREACH OPENED";

        case WorldEventType::BreachSealed:
            return L"BREACH SEALED";

        case WorldEventType::EquipmentFailed:
            return L"EQUIPMENT FAILED";

        case WorldEventType::EquipmentRepaired:
            return L"EQUIPMENT REPAIRED";

        case WorldEventType::EquipmentDestroyed:
            return L"EQUIPMENT DESTROYED";

        case WorldEventType::PowerBrownout:
            return L"POWER BROWNOUT";

        default:
            return L"UNKNOWN EVENT";
        }
    }

    static std::wstring GetRoomName(
        ECSRegistry& registry,
        Entity room)
    {
        if (room == INVALID_ENTITY)
            return L"UNKNOWN";

        if (const auto* name =
            registry.GetComponent<NameComponent>(room))
        {
            return std::wstring(
                name->name.begin(),
                name->name.end());
        }

        return L"ROOM " +
            std::to_wstring(
                static_cast<unsigned int>(room));
    }

    static std::wstring FormatWorldEvent(
        ECSRegistry& registry,
        const WorldEvent& event)
    {
        wchar_t timeBuffer[32]{};

        swprintf_s(
            timeBuffer,
            L"%7.1f",
            event.timeSeconds);

        std::wstring result;

        result += L"[";
        result += timeBuffer;
        result += L"]  ";

        result +=
            WorldEventTypeToString(event.type);

        result += L"  |  ";

        result +=
            GetRoomName(
                registry,
                event.room);

        return result;
    }

    static void DrawEventLog(
        DWriteTextSystem& text,
        ECSRegistry& registry,
        const WorldEventLog& eventLog,
        float screenWidth,
        float screenHeight)
    {
        constexpr float width = 680.0f;
        constexpr float height = 440.0f;

        const float x =
            screenWidth * 0.5f -
            width * 0.5f;

        const float y =
            screenHeight * 0.5f -
            height * 0.5f;

        text.DrawFilledRect(
            x,
            y,
            width,
            height,
            D2D1::ColorF(
                0.012f,
                0.018f,
                0.022f,
                0.96f));

        text.DrawFilledRect(
            x,
            y,
            width,
            3.0f,
            D2D1::ColorF(
                0.42f,
                0.80f,
                0.92f,
                1.0f));

        text.DrawSmallTextLineColor(
            L"SHIP EVENT LOG",
            x + 22.0f,
            y + 16.0f,
            width - 44.0f,
            D2D1::ColorF(
                0.42f,
                0.80f,
                0.92f,
                1.0f));

        text.DrawSmallTextLineColor(
            L"TAB - CLOSE",
            x + width - 118.0f,
            y + 16.0f,
            100.0f,
            D2D1::ColorF(
                0.50f,
                0.56f,
                0.58f,
                1.0f));

        text.DrawFilledRect(
            x + 18.0f,
            y + 45.0f,
            width - 36.0f,
            1.0f,
            D2D1::ColorF(
                0.18f,
                0.24f,
                0.26f,
                1.0f));

        const auto& events =
            eventLog.GetEvents();

        constexpr float rowHeight = 23.0f;
        constexpr int maxVisibleEvents = 15;

        int shown = 0;

        for (auto it = events.rbegin();
            it != events.rend() &&
            shown < maxVisibleEvents;
            ++it)
        {
            const WorldEvent& event = *it;

            const std::wstring line =
                FormatWorldEvent(
                    registry,
                    event);

            D2D1_COLOR_F color =
                D2D1::ColorF(
                    0.78f,
                    0.84f,
                    0.86f,
                    1.0f);

            switch (event.type)
            {
            case WorldEventType::FireStarted:
            case WorldEventType::BreachOpened:
            case WorldEventType::EquipmentDestroyed:
                color =
                    D2D1::ColorF(
                        0.96f,
                        0.34f,
                        0.20f,
                        1.0f);
                break;

            case WorldEventType::EquipmentFailed:
            case WorldEventType::PowerBrownout:
                color =
                    D2D1::ColorF(
                        0.96f,
                        0.68f,
                        0.22f,
                        1.0f);
                break;

            case WorldEventType::FireExtinguished:
            case WorldEventType::BreachSealed:
            case WorldEventType::EquipmentRepaired:
                color =
                    D2D1::ColorF(
                        0.30f,
                        0.84f,
                        0.62f,
                        1.0f);
                break;

            default:
                break;
            }

            text.DrawSmallTextLineColor(
                line.c_str(),
                x + 22.0f,
                y + 60.0f +
                rowHeight * static_cast<float>(shown),
                width - 44.0f,
                color);

            ++shown;
        }

        if (events.empty())
        {
            text.DrawSmallTextLineColor(
                L"NO EVENTS RECORDED",
                x + 22.0f,
                y + 72.0f,
                width - 44.0f,
                D2D1::ColorF(
                    0.50f,
                    0.56f,
                    0.58f,
                    1.0f));
        }
    }

    void DrawDebug(
        DWriteTextSystem& text,
        const PlayerComponent* player,
        const RoomComponent* room,
        const GameHudStats& stats) const
    {
        constexpr float x = 24.0f;
        constexpr float y = 22.0f;
        constexpr float width = 270.0f;
        constexpr float row = 20.0f;

        text.DrawFilledRect(
            x - 8.0f,
            y - 6.0f,
            width,
            228.0f,
            D2D1::ColorF(
                0.015f,
                0.02f,
                0.022f,
                0.78f));

        text.DrawSmallTextLineColor(
            L"F3  DEBUG TELEMETRY",
            x,
            y,
            width,
            D2D1::ColorF(
                0.42f,
                0.80f,
                0.92f,
                1.0f));

        text.DrawSmallTextLineColor(
            (L"FPS  " +
                std::to_wstring(m_displayFps)).c_str(),
            x,
            y + row,
            width,
            D2D1::ColorF(
                0.82f,
                0.88f,
                0.90f,
                1.0f));

        text.DrawSmallTextLineColor(
            (L"FRAME  " +
                FormatFixed1(stats.frameMs) +
                L" MS").c_str(),
            x,
            y + row * 2.0f,
            width,
            D2D1::ColorF(
                0.72f,
                0.78f,
                0.80f,
                1.0f));

        text.DrawSmallTextLineColor(
            (L"RENDER CPU  " +
                FormatFixed1(stats.renderCpuMs) +
                L" MS").c_str(),
            x,
            y + row * 3.0f,
            width,
            D2D1::ColorF(
                0.72f,
                0.78f,
                0.80f,
                1.0f));

        text.DrawSmallTextLineColor(
            (L"MESHES  " +
                std::to_wstring(
                    stats.activeMeshObjects)).c_str(),
            x,
            y + row * 4.0f,
            width,
            D2D1::ColorF(
                0.72f,
                0.78f,
                0.80f,
                1.0f));

        text.DrawSmallTextLineColor(
            (L"MAIN / SHADOW  " +
                std::to_wstring(
                    stats.mainRenderedObjects) +
                L" / " +
                std::to_wstring(
                    stats.shadowRenderedObjects)).c_str(),
            x,
            y + row * 5.0f,
            width,
            D2D1::ColorF(
                0.72f,
                0.78f,
                0.80f,
                1.0f));

        const auto& culling =
            RoomCullingSystem::GetDebugStats();

        text.DrawSmallTextLineColor(
            (L"ROOMS  " +
                std::to_wstring(
                    culling.visibleRoomCount) +
                L"  CULLED  " +
                std::to_wstring(
                    culling.hiddenEntities)).c_str(),
            x,
            y + row * 6.0f,
            width,
            D2D1::ColorF(
                0.72f,
                0.78f,
                0.80f,
                1.0f));

        if (player)
        {
            text.DrawSmallTextLineColor(
                (L"PLAYER O2  " +
                    std::to_wstring(
                        static_cast<int>(
                            player->oxygen))).c_str(),
                x,
                y + row * 7.0f,
                width,
                D2D1::ColorF(
                    0.28f,
                    0.82f,
                    0.94f,
                    1.0f));
        }

        if (room)
        {
            const float pressurePercent =
                std::clamp(
                    room->pressure,
                    0.0f,
                    1.0f) *
                100.0f;

            const D2D1_COLOR_F pressureColor =
                pressurePercent >= 65.0f
                ? D2D1::ColorF(
                    0.38f,
                    0.86f,
                    0.70f,
                    1.0f)
                : D2D1::ColorF(
                    0.98f,
                    0.46f,
                    0.18f,
                    1.0f);

            const D2D1_COLOR_F temperatureColor =
                room->temperature >= 5.0f
                ? D2D1::ColorF(
                    0.62f,
                    0.82f,
                    0.92f,
                    1.0f)
                : D2D1::ColorF(
                    0.36f,
                    0.66f,
                    1.0f,
                    1.0f);

            text.DrawSmallTextLineColor(
                (L"ROOM PRESSURE  " +
                    FormatFixed1(
                        pressurePercent) +
                    L" %").c_str(),
                x,
                y + row * 8.0f,
                width,
                pressureColor);

            text.DrawSmallTextLineColor(
                (L"ROOM TEMP  " +
                    FormatFixed1(
                        room->temperature) +
                    L" C").c_str(),
                x,
                y + row * 9.0f,
                width,
                temperatureColor);
        }
        else
        {
            text.DrawSmallTextLineColor(
                L"ROOM ATMOSPHERE  OUTSIDE / UNKNOWN",
                x,
                y + row * 8.0f,
                width,
                D2D1::ColorF(
                    0.98f,
                    0.46f,
                    0.18f,
                    1.0f));
        }
    }

    void UpdateFps(float deltaTime)
    {
        if (deltaTime <= 0.0f)
            return;

        m_fpsTimer += deltaTime;

        ++m_frameCounter;

        if (m_fpsTimer >= 0.25f)
        {
            m_displayFps =
                static_cast<int>(
                    m_frameCounter /
                    m_fpsTimer +
                    0.5f);

            m_fpsTimer = 0.0f;
            m_frameCounter = 0;
        }
    }

private:

    bool m_showDebug = false;
    bool m_showEventLog = false;

    float m_fpsTimer = 0.0f;

    int m_frameCounter = 0;
    int m_displayFps = 0;
};
