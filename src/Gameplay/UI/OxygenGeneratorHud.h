#pragma once

#include <algorithm>
#include <cwchar>
#include <string>

#include "DWriteTextSystem.h"
#include "ECSRegistry.h"
#include "Components.h"
#include "Gameplay/Power/OxygenGeneratorComponent.h"
#include "Gameplay/Power/PowerConsumerComponent.h"

class OxygenGeneratorHud
{
public:
    ~OxygenGeneratorHud()
    {
        if (m_panelBitmap)
            m_panelBitmap->Release();
    }

    void Render(
        DWriteTextSystem& text,
        ECSRegistry& registry,
        Entity openedEntity,
        float screenWidth)
    {
        const auto* generator = registry.GetComponent<OxygenGeneratorComponent>(openedEntity);
        if (!generator)
            return;

        const auto* consumer = registry.GetComponent<PowerConsumerComponent>(openedEntity);
        const auto* damage = registry.GetComponent<DamageableComponent>(openedEntity);
        const DeviceState state = GetState(*generator, consumer, damage);

        const float powerRatio = consumer
            ? std::clamp(consumer->powerRatio, 0.0f, 1.0f)
            : (generator->hasPower ? 1.0f : 0.0f);
        const float integrity = damage && damage->maxHealth > 0.0f
            ? std::clamp(damage->health / damage->maxHealth, 0.0f, 1.0f)
            : 1.0f;
        const float atmosphere = CalculateAverageAtmosphere(registry);
        const float outputRatio = state == DeviceState::Online ? 1.0f : 0.0f;

        constexpr float panelWidth = 500.0f;
        constexpr float panelHeight = 281.0f;
        constexpr float panelTop = 60.0f;
        constexpr float innerOffset = 36.0f;
        constexpr float innerWidth = 428.0f;
        const float panelLeft = (std::max)(20.0f, screenWidth - panelWidth - 24.0f);
        const float innerLeft = panelLeft + innerOffset;

        text.BeginBatch();
        EnsureTexture(text);

        if (m_panelBitmap)
            text.DrawBitmap(m_panelBitmap, panelLeft, panelTop, panelWidth, panelHeight);
        else
            text.DrawFilledRect(panelLeft, panelTop, panelWidth, panelHeight,
                D2D1::ColorF(0.025f, 0.045f, 0.060f, 0.96f));

        text.DrawTextLineColor(
            L"OXYGEN PROCESSOR",
            innerLeft,
            panelTop + 34.0f,
            D2D1::ColorF(0.82f, 0.95f, 1.0f, 1.0f));
        text.DrawFilledRect(innerLeft, panelTop + 68.0f, 9.0f, 9.0f, GetStateColor(state));
        text.DrawSmallTextLineColor(
            GetStateText(state),
            innerLeft + 18.0f,
            panelTop + 61.0f,
            innerWidth - 18.0f,
            GetStateColor(state));

        DrawMetric(
            text, innerLeft, panelTop + 93.0f, innerWidth,
            L"GRID SUPPLY", FormatPercent(powerRatio), powerRatio,
            D2D1::ColorF(0.20f, 0.78f, 0.98f, 1.0f));
        DrawMetric(
            text, innerLeft, panelTop + 129.0f, innerWidth,
            L"OXYGEN PRODUCTION", FormatRate(generator->oxygenPerSecond), outputRatio,
            D2D1::ColorF(0.20f, 0.90f, 0.62f, 1.0f));
        DrawMetric(
            text, innerLeft, panelTop + 165.0f, innerWidth,
            L"SHIP ATMOSPHERE", FormatPercent(atmosphere), atmosphere,
            atmosphere > 0.40f
                ? D2D1::ColorF(0.28f, 0.84f, 0.94f, 1.0f)
                : D2D1::ColorF(0.98f, 0.34f, 0.20f, 1.0f));
        DrawMetric(
            text, innerLeft, panelTop + 201.0f, innerWidth,
            L"SYSTEM INTEGRITY", FormatPercent(integrity), integrity,
            integrity > 0.35f
                ? D2D1::ColorF(0.94f, 0.69f, 0.24f, 1.0f)
                : D2D1::ColorF(0.96f, 0.25f, 0.20f, 1.0f));

        text.DrawSmallTextLineColor(
            L"AUTOMATIC CONTINUOUS OPERATION",
            innerLeft,
            panelTop + 244.0f,
            innerWidth,
            D2D1::ColorF(0.42f, 0.62f, 0.68f, 1.0f));

        text.EndBatch();
    }

private:
    enum class DeviceState
    {
        Online,
        Disabled,
        NoPower,
        Damaged
    };

    void EnsureTexture(DWriteTextSystem& text)
    {
        if (m_panelTextureLoadAttempted)
            return;

        m_panelTextureLoadAttempted = true;
        text.LoadBitmapFromFile(
            L"assets/texture/ui/oxygen_generator_panel.png",
            &m_panelBitmap);
    }

    static float CalculateAverageAtmosphere(ECSRegistry& registry)
    {
        float total = 0.0f;
        int count = 0;
        registry.ForEach<RoomComponent>(
            [&](Entity, RoomComponent& room)
            {
                if (!room.isActive)
                    return;
                total += std::clamp(room.oxygen, 0.0f, 1.0f);
                ++count;
            });
        return count > 0 ? total / static_cast<float>(count) : 0.0f;
    }

    static DeviceState GetState(
        const OxygenGeneratorComponent& generator,
        const PowerConsumerComponent* consumer,
        const DamageableComponent* damage)
    {
        if ((damage && damage->isDestroyed) || generator.broken)
            return DeviceState::Damaged;
        if (!generator.enabled)
            return DeviceState::Disabled;
        if (!generator.hasPower || (consumer && !consumer->hasPower))
            return DeviceState::NoPower;
        return DeviceState::Online;
    }

    static const wchar_t* GetStateText(DeviceState state)
    {
        switch (state)
        {
        case DeviceState::Online:   return L"LIFE SUPPORT ONLINE";
        case DeviceState::Disabled: return L"PROCESSOR STANDBY";
        case DeviceState::NoPower:  return L"POWER UNAVAILABLE";
        case DeviceState::Damaged:  return L"MAINTENANCE REQUIRED";
        default:                    return L"SYSTEM UNKNOWN";
        }
    }

    static D2D1_COLOR_F GetStateColor(DeviceState state)
    {
        switch (state)
        {
        case DeviceState::Online:   return D2D1::ColorF(0.22f, 0.90f, 0.62f, 1.0f);
        case DeviceState::Disabled: return D2D1::ColorF(0.78f, 0.69f, 0.31f, 1.0f);
        case DeviceState::NoPower:  return D2D1::ColorF(0.98f, 0.45f, 0.20f, 1.0f);
        case DeviceState::Damaged:  return D2D1::ColorF(0.96f, 0.25f, 0.20f, 1.0f);
        default:                    return D2D1::ColorF(D2D1::ColorF::White);
        }
    }

    static std::wstring FormatPercent(float ratio)
    {
        wchar_t buffer[24]{};
        swprintf_s(buffer, L"%.0f%%", std::clamp(ratio, 0.0f, 1.0f) * 100.0f);
        return buffer;
    }

    static std::wstring FormatRate(float rate)
    {
        wchar_t buffer[32]{};
        swprintf_s(buffer, L"%.2f / S", rate);
        return buffer;
    }

    static void DrawMetric(
        DWriteTextSystem& text,
        float x,
        float y,
        float width,
        const wchar_t* label,
        const std::wstring& value,
        float ratio,
        const D2D1_COLOR_F& color)
    {
        text.DrawSmallTextLineColor(
            label, x, y, width * 0.66f,
            D2D1::ColorF(0.55f, 0.72f, 0.78f, 1.0f));
        text.DrawSmallTextLineColor(
            value.c_str(), x + width * 0.68f, y, width * 0.30f,
            D2D1::ColorF(0.82f, 0.94f, 0.96f, 1.0f));
        text.DrawFilledRect(
            x, y + 21.0f, width, 6.0f,
            D2D1::ColorF(0.05f, 0.12f, 0.15f, 0.96f));
        text.DrawFilledRect(
            x, y + 21.0f, width * std::clamp(ratio, 0.0f, 1.0f), 6.0f, color);
    }

    ID2D1Bitmap* m_panelBitmap = nullptr;
    bool m_panelTextureLoadAttempted = false;
};
