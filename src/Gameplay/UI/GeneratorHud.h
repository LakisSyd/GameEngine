#pragma once

#include <algorithm>
#include <cwchar>
#include <string>

#include "DWriteTextSystem.h"
#include "ECSRegistry.h"
#include "InputManager.h"
#include "Components.h"
#include "Gameplay/Power/GeneratorComponent.h"
#include "Gameplay/Power/PowerGridComponent.h"

class GeneratorHud
{
public:
    ~GeneratorHud()
    {
        if (m_panelBitmap)
            m_panelBitmap->Release();
    }

    void Render(
        DWriteTextSystem& text,
        ECSRegistry& registry,
        Entity openedEntity,
        float screenWidth,
        const InputManager& input)
    {
        auto* generator = registry.GetComponent<GeneratorComponent>(openedEntity);
        if (!generator)
            return;

        const auto* damage = registry.GetComponent<DamageableComponent>(openedEntity);
        const auto* grid = FindGrid(registry, openedEntity);

        const float outputRatio = Ratio(generator->currentOutputMw, generator->maxOutputMw);
        const float deuteriumRatio = Ratio(generator->deuteriumKg, generator->maxDeuteriumKg);
        const float tritiumRatio = Ratio(generator->tritiumKg, generator->maxTritiumKg);
        const float fuelRatio = (std::min)(deuteriumRatio, tritiumRatio);
        const float temperatureRatio = Ratio(generator->heat, generator->scramTemperature);
        const float gridLoadRatio = grid ? grid->loadRatio : 0.0f;
        const float batteryRatio = grid ? std::clamp(grid->BatteryRatio(), 0.0f, 1.0f) : 0.0f;

        constexpr float panelWidth = 560.0f;
        constexpr float panelHeight = 315.0f;
        constexpr float panelTop = 48.0f;
        constexpr float innerOffset = 42.0f;
        constexpr float innerWidth = 476.0f;
        const float panelLeft = (std::max)(20.0f, screenWidth - panelWidth - 24.0f);
        const float innerLeft = panelLeft + innerOffset;

        text.BeginBatch();
        EnsureTexture(text);

        if (m_panelBitmap)
            text.DrawBitmap(m_panelBitmap, panelLeft, panelTop, panelWidth, panelHeight);
        else
            text.DrawFilledRect(panelLeft, panelTop, panelWidth, panelHeight,
                D2D1::ColorF(0.025f, 0.045f, 0.060f, 0.96f));

        const ReactorState state = GetState(*generator, damage);
        text.DrawTextLineColor(
            L"FUSION REACTOR",
            innerLeft,
            panelTop + 43.0f,
            D2D1::ColorF(0.84f, 0.95f, 1.0f, 1.0f));
        text.DrawFilledRect(innerLeft, panelTop + 76.0f, 9.0f, 9.0f, GetStateColor(state));
        text.DrawSmallTextLineColor(
            GetStateText(state),
            innerLeft + 18.0f,
            panelTop + 69.0f,
            innerWidth - 18.0f,
            GetStateColor(state));

        DrawMetric(
            text, innerLeft, panelTop + 99.0f, innerWidth,
            L"REACTOR OUTPUT",
            FormatPair(generator->currentOutputMw, generator->maxOutputMw, L" MW"),
            outputRatio,
            D2D1::ColorF(0.16f, 0.73f, 1.0f, 1.0f));
        DrawMetric(
            text, innerLeft, panelTop + 130.0f, innerWidth,
            L"D-T FUEL RESERVE",
            FormatPercent(fuelRatio),
            fuelRatio,
            fuelRatio > 0.15f
                ? D2D1::ColorF(0.30f, 0.88f, 0.62f, 1.0f)
                : D2D1::ColorF(0.96f, 0.32f, 0.22f, 1.0f));
        DrawMetric(
            text, innerLeft, panelTop + 161.0f, innerWidth,
            L"CORE TEMPERATURE",
            FormatValue(generator->heat, L" C"),
            temperatureRatio,
            temperatureRatio < 0.75f
                ? D2D1::ColorF(0.95f, 0.67f, 0.22f, 1.0f)
                : D2D1::ColorF(0.98f, 0.24f, 0.18f, 1.0f));
        DrawMetric(
            text, innerLeft, panelTop + 192.0f, innerWidth,
            L"SHIP GRID LOAD",
            grid ? FormatPair(grid->demandMw, grid->generationMw + grid->batteryOutputMw, L" MW") : L"NO GRID",
            gridLoadRatio,
            grid && grid->brownout
                ? D2D1::ColorF(0.98f, 0.25f, 0.18f, 1.0f)
                : D2D1::ColorF(0.22f, 0.82f, 0.96f, 1.0f));
        DrawMetric(
            text, innerLeft, panelTop + 223.0f, innerWidth,
            L"EMERGENCY BATTERY",
            grid ? FormatPercent(batteryRatio) : L"NOT INSTALLED",
            batteryRatio,
            batteryRatio > 0.20f
                ? D2D1::ColorF(0.40f, 0.82f, 0.34f, 1.0f)
                : D2D1::ColorF(0.98f, 0.38f, 0.20f, 1.0f));

        DrawControls(text, input, *generator, damage, innerLeft, panelTop + 258.0f, innerWidth);
        text.EndBatch();
    }

private:
    enum class ReactorState
    {
        Online,
        Offline,
        Scrammed,
        FuelEmpty,
        Damaged
    };

    void EnsureTexture(DWriteTextSystem& text)
    {
        if (m_panelTextureLoadAttempted)
            return;

        m_panelTextureLoadAttempted = true;
        text.LoadBitmapFromFile(
            L"assets/texture/ui/fusion_generator_panel.png",
            &m_panelBitmap);
    }

    static PowerGridComponent* FindGrid(ECSRegistry& registry, Entity generatorEntity)
    {
        const auto* part = registry.GetComponent<ShipPartComponent>(generatorEntity);
        return part ? registry.GetComponent<PowerGridComponent>(part->root) : nullptr;
    }

    static float Ratio(float value, float maximum)
    {
        return maximum > 0.0f ? std::clamp(value / maximum, 0.0f, 1.0f) : 0.0f;
    }

    static std::wstring FormatValue(float value, const wchar_t* suffix)
    {
        wchar_t buffer[48]{};
        swprintf_s(buffer, L"%.1f%s", value, suffix);
        return buffer;
    }

    static std::wstring FormatPair(float value, float maximum, const wchar_t* suffix)
    {
        wchar_t buffer[64]{};
        swprintf_s(buffer, L"%.1f / %.1f%s", value, maximum, suffix);
        return buffer;
    }

    static std::wstring FormatPercent(float ratio)
    {
        wchar_t buffer[24]{};
        swprintf_s(buffer, L"%.0f%%", std::clamp(ratio, 0.0f, 1.0f) * 100.0f);
        return buffer;
    }

    static ReactorState GetState(
        const GeneratorComponent& generator,
        const DamageableComponent* damage)
    {
        if (damage && damage->isDestroyed)
            return ReactorState::Damaged;
        if (generator.isScrammed)
            return ReactorState::Scrammed;
        if (generator.deuteriumKg <= 0.0001f || generator.tritiumKg <= 0.0001f)
            return ReactorState::FuelEmpty;
        return generator.isOnline ? ReactorState::Online : ReactorState::Offline;
    }

    static const wchar_t* GetStateText(ReactorState state)
    {
        switch (state)
        {
        case ReactorState::Online:    return L"FUSION STABLE";
        case ReactorState::Offline:   return L"REACTOR STANDBY";
        case ReactorState::Scrammed:  return L"AUTOMATIC SCRAM - COOLING";
        case ReactorState::FuelEmpty: return L"FUSION FUEL DEPLETED";
        case ReactorState::Damaged:   return L"REACTOR DAMAGED";
        default:                      return L"REACTOR UNKNOWN";
        }
    }

    static D2D1_COLOR_F GetStateColor(ReactorState state)
    {
        switch (state)
        {
        case ReactorState::Online:    return D2D1::ColorF(0.20f, 0.86f, 1.0f, 1.0f);
        case ReactorState::Offline:   return D2D1::ColorF(0.90f, 0.64f, 0.25f, 1.0f);
        case ReactorState::Scrammed:  return D2D1::ColorF(1.0f, 0.24f, 0.16f, 1.0f);
        case ReactorState::FuelEmpty: return D2D1::ColorF(0.96f, 0.42f, 0.18f, 1.0f);
        case ReactorState::Damaged:   return D2D1::ColorF(0.96f, 0.22f, 0.18f, 1.0f);
        default:                      return D2D1::ColorF(D2D1::ColorF::White);
        }
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
            label, x, y, width * 0.62f,
            D2D1::ColorF(0.56f, 0.72f, 0.80f, 1.0f));
        text.DrawSmallTextLineColor(
            value.c_str(), x + width * 0.64f, y, width * 0.36f,
            D2D1::ColorF(0.82f, 0.92f, 0.96f, 1.0f));
        text.DrawFilledRect(
            x, y + 19.0f, width, 5.0f,
            D2D1::ColorF(0.04f, 0.11f, 0.15f, 0.96f));
        text.DrawFilledRect(
            x, y + 19.0f, width * std::clamp(ratio, 0.0f, 1.0f), 5.0f, color);
    }

    static bool IsHovered(
        const InputManager& input,
        float x,
        float y,
        float width,
        float height)
    {
        return input.GetMouseX() >= x && input.GetMouseX() <= x + width &&
            input.GetMouseY() >= y && input.GetMouseY() <= y + height;
    }

    static bool DrawButton(
        DWriteTextSystem& text,
        const InputManager& input,
        float x,
        float y,
        float width,
        float height,
        const wchar_t* label,
        bool enabled,
        bool primary = false)
    {
        const bool hovered = enabled && IsHovered(input, x, y, width, height);
        const D2D1_COLOR_F background = !enabled
            ? D2D1::ColorF(0.08f, 0.09f, 0.10f, 0.88f)
            : (hovered
                ? D2D1::ColorF(0.10f, 0.44f, 0.58f, 0.98f)
                : (primary
                    ? D2D1::ColorF(0.05f, 0.27f, 0.38f, 0.96f)
                    : D2D1::ColorF(0.04f, 0.16f, 0.22f, 0.94f)));

        text.DrawFilledRect(x, y, width, height, background);
        text.DrawSmallTextLineColor(
            label, x + 10.0f, y + 3.0f, width - 20.0f,
            enabled
                ? D2D1::ColorF(0.84f, 0.95f, 1.0f, 1.0f)
                : D2D1::ColorF(0.38f, 0.43f, 0.45f, 1.0f));
        return hovered && input.WasLeftPressed();
    }

    static void DrawControls(
        DWriteTextSystem& text,
        const InputManager& input,
        GeneratorComponent& generator,
        const DamageableComponent* damage,
        float x,
        float y,
        float width)
    {
        constexpr float height = 25.0f;
        const bool damaged = damage && damage->isDestroyed;
        const bool hasFuel = generator.deuteriumKg > 0.0001f && generator.tritiumKg > 0.0001f;
        const bool canStart = !damaged && !generator.isScrammed && hasFuel;

        if (DrawButton(text, input, x, y, 42.0f, height, L"-", true))
        {
            generator.targetOutputMw = (std::max)(
                0.0f,
                generator.targetOutputMw - generator.maxOutputMw * 0.10f);
        }

        const std::wstring targetText = L"TARGET " +
            FormatPercent(Ratio(generator.targetOutputMw, generator.maxOutputMw));
        DrawButton(text, input, x + 50.0f, y, 150.0f, height, targetText.c_str(), false);

        if (DrawButton(text, input, x + 208.0f, y, 42.0f, height, L"+", true))
        {
            generator.targetOutputMw = (std::min)(
                generator.maxOutputMw,
                generator.targetOutputMw + generator.maxOutputMw * 0.10f);
        }

        const bool powerEnabled = generator.isOnline || canStart;
        const wchar_t* powerText = damaged
            ? L"REPAIR REQUIRED"
            : (generator.isScrammed
                ? L"COOLING LOCK"
                : (generator.isOnline ? L"SHUT DOWN" : L"START REACTOR"));
        if (DrawButton(
            text, input, x + 260.0f, y, width - 260.0f, height,
            powerText, powerEnabled, true))
        {
            generator.isOnline = !generator.isOnline;
        }
    }

    ID2D1Bitmap* m_panelBitmap = nullptr;
    bool m_panelTextureLoadAttempted = false;
};
