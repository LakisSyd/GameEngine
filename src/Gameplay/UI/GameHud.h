#pragma once

#include <algorithm>
#include <cmath>
#include <cwchar>
#include <string>
#include <windows.h>

#include "DWriteTextSystem.h"
#include "Components.h"
#include "ECSRegistry.h"
#include "Gameplay/Player/GameplayInputSystem.h"
#include "PlayerComponents.h"

#include "Gameplay/Combat/WeaponComponent.h"
#include "Gameplay/Inventory/InventoryComponent.h"
#include "Gameplay/AI/CrewTaskCoordinator.h"
#include "Gameplay/Ship/RoomCullingSystem.h"
#include "Gameplay/Ship/ShipAlertSystem.h"

#include "../../WorldEventLog.h"
#include "../../../NPCComponents.h"

struct GameHudStats
{
    float frameMs = 0.0f;
    float renderCpuMs = 0.0f;

    int activeMeshObjects = 0;
    int mainRenderedObjects = 0;
    int shadowRenderedObjects = 0;
    int availableCrew = 0;
    int busyCrew = 0;
    int openCrewTasks = 0;
    bool simulationPaused = false;
    float timeScale = 1.0f;
};

// Optional per-frame presentation data that is owned by gameplay systems rather
// than ECS. Keeping it separate lets the existing Render call remain valid.
struct GameHudContext
{
    std::string interactionPrompt;
    std::wstring interactionBinding = L"E";
    Entity interactionTarget = INVALID_ENTITY;
    bool showCrosshair = true;
    bool useWeaponCrosshair = false;
    float weaponCrosshairX = 0.0f;
    float weaponCrosshairY = 0.0f;
    bool showInventory = true;
    std::string notification;
    bool notificationSuccess = true;
    Entity openedNpc = INVALID_ENTITY;
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
        const GameInputFrame& input,
        const WorldEventLog& eventLog,
        const GameHudStats& stats = {})
    {
        Render(
            text,
            registry,
            player,
            deltaTime,
            screenWidth,
            screenHeight,
            input,
            eventLog,
            stats,
            GameHudContext{});
    }

    void Render(
        DWriteTextSystem& text,
        ECSRegistry& registry,
        Entity player,
        float deltaTime,
        float screenWidth,
        float screenHeight,
        const GameInputFrame& input,
        const WorldEventLog& eventLog,
        const GameHudStats& stats,
        const GameHudContext& context)
    {
        UpdateFps(deltaTime);

        if (input.Pressed(GameAction::ToggleDebugHud))
            m_showDebug = !m_showDebug;

        if (input.Pressed(GameAction::ToggleEventLog))
            m_showEventLog = !m_showEventLog;

        text.BeginBatch();

        const auto* playerVitals =
            player != INVALID_ENTITY
            ? registry.GetComponent<CharacterVitalsComponent>(player)
            : nullptr;

        const RoomComponent* playerRoom =
            FindPlayerRoom(registry, player);

        if (context.showCrosshair && !m_showEventLog)
        {
            const auto* equipped = registry.GetComponent<EquippedWeaponComponent>(player);
            const auto* weapon = equipped ? registry.GetComponent<WeaponComponent>(equipped->weapon) : nullptr;
            DrawCrosshair(
                text,
                context.useWeaponCrosshair ? false :
                    context.interactionTarget != INVALID_ENTITY,
                context.useWeaponCrosshair ? context.weaponCrosshairX : screenWidth * 0.5f,
                context.useWeaponCrosshair ? context.weaponCrosshairY : screenHeight * 0.5f,
                weapon && weapon->shotFlashTime > 0.0f);
        }

        if (playerVitals)
        {
            DrawPlayerVitals(
                text,
                *playerVitals,
                screenHeight);
        }

        DrawEnvironmentStatus(
            text,
            playerRoom,
            screenWidth);

        if (context.showInventory && !m_showEventLog)
        {
            DrawInventory(
                text,
                registry,
                player,
                screenWidth,
                screenHeight);
        }

        if (!m_showEventLog &&
            !context.interactionPrompt.empty())
        {
            DrawInteractionPrompt(
                text,
                context.interactionPrompt,
                context.interactionBinding,
                screenWidth,
                screenHeight);
        }

        DrawShipAlert(
            text,
            screenWidth);

        DrawCrewStatus(text, stats);

        if (!context.notification.empty() && !m_showEventLog)
        {
            DrawNotification(
                text,
                context.notification,
                context.notificationSuccess,
                screenWidth);
        }

        if (!m_showEventLog && context.openedNpc != INVALID_ENTITY)
        {
            DrawNpcInteractionPanel(
                text,
                registry,
                context.openedNpc,
                context.interactionBinding,
                screenWidth,
                screenHeight);
        }

        if (m_showDebug)
        {
            DrawDebug(
                text,
                playerVitals,
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
        const CharacterVitalsComponent& vitals,
        float screenHeight)
    {
        constexpr float panelWidth = 276.0f;
        constexpr float panelHeight = 116.0f;
        constexpr float labelWidth = 72.0f;
        constexpr float barWidth = 136.0f;
        constexpr float barHeight = 7.0f;

        constexpr float x = 22.0f;
        const float y =
            (std::max)(18.0f, screenHeight - panelHeight - 22.0f);

        text.DrawFilledRect(
            x,
            y,
            panelWidth,
            panelHeight,
            D2D1::ColorF(0.012f, 0.026f, 0.034f, 0.84f));

        text.DrawRectOutline(
            x,
            y,
            panelWidth,
            panelHeight,
            1.0f,
            D2D1::ColorF(0.12f, 0.31f, 0.38f, 0.86f));

        text.DrawFilledRect(
            x,
            y,
            4.0f,
            panelHeight,
            D2D1::ColorF(0.22f, 0.82f, 0.94f, 1.0f));

        text.DrawSmallTextLineColor(
            L"EVA SUIT // LIFE SUPPORT",
            x + 14.0f,
            y + 7.0f,
            panelWidth - 24.0f,
            D2D1::ColorF(0.42f, 0.86f, 0.94f, 0.96f));

        const float healthRatio =
            Ratio(
                vitals.health,
                vitals.maxHealth);

        const float staminaRatio =
            Ratio(
                vitals.stamina,
                vitals.maxStamina);

        const float oxygenRatio =
            Ratio(
                vitals.oxygen,
                vitals.maxOxygen);

        DrawVital(
            text,
            L"VITALS",
            x + 14.0f,
            y + 38.0f,
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
            L"MOTION",
            x + 14.0f,
            y + 65.0f,
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

        DrawVital(
            text,
            L"OXYGEN",
            x + 14.0f,
            y + 92.0f,
            labelWidth,
            barWidth,
            barHeight,
            oxygenRatio,
            oxygenRatio > 0.25f
            ? D2D1::ColorF(0.22f, 0.72f, 0.98f, 1.0f)
            : D2D1::ColorF(0.98f, 0.34f, 0.18f, 1.0f));
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
            y - 9.0f,
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

        const std::wstring percent =
            std::to_wstring(
                static_cast<int>(ratio * 100.0f + 0.5f)) +
            L"%";

        text.DrawSmallTextLineColor(
            percent.c_str(),
            x + labelWidth + barWidth + 8.0f,
            y - 9.0f,
            48.0f,
            color);
    }

    static std::wstring Utf8ToWide(const std::string& value)
    {
        if (value.empty())
            return {};

        const int count = MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            value.data(),
            static_cast<int>(value.size()),
            nullptr,
            0);

        if (count <= 0)
            return std::wstring(value.begin(), value.end());

        std::wstring result(
            static_cast<std::size_t>(count),
            L'\0');

        MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            value.data(),
            static_cast<int>(value.size()),
            result.data(),
            count);

        return result;
    }

    static std::wstring GetInventoryItemName(
        ECSRegistry& registry,
        Entity item)
    {
        if (item == INVALID_ENTITY || !registry.IsAlive(item))
            return L"EMPTY";

        if (const auto* name =
            registry.GetComponent<NameComponent>(item))
        {
            if (!name->name.empty() && name->name != "Entity")
                return Utf8ToWide(name->name);
        }

        const auto* weapon =
            registry.GetComponent<WeaponComponent>(item);

        if (!weapon)
            return L"ITEM";

        std::string fileName = weapon->visualModelPath;
        const std::size_t slash = fileName.find_last_of("/\\");
        if (slash != std::string::npos)
            fileName.erase(0, slash + 1);

        const std::size_t extension = fileName.find_last_of('.');
        if (extension != std::string::npos)
            fileName.erase(extension);

        return fileName.empty()
            ? L"RIFLE"
            : Utf8ToWide(fileName);
    }

    static void DrawNpcInteractionPanel(
        DWriteTextSystem& text,
        ECSRegistry& registry,
        Entity npcEntity,
        const std::wstring& interactionBinding,
        float screenWidth,
        float screenHeight)
    {
        const auto* npc = registry.GetComponent<NPCComponent>(npcEntity);
        if (!npc)
            return;

        const auto* brain = registry.GetComponent<NpcBrainComponent>(npcEntity);
        const auto* crew = registry.GetComponent<CrewMemberComponent>(npcEntity);
        constexpr float width = 372.0f;
        constexpr float height = 166.0f;
        const float x = (std::max)(14.0f, screenWidth - width - 24.0f);
        const float y = (std::max)(92.0f, screenHeight * 0.27f);
        const D2D1_COLOR_F accent = npc->isHostile
            ? D2D1::ColorF(0.98f, 0.30f, 0.20f, 1.0f)
            : D2D1::ColorF(0.27f, 0.84f, 0.95f, 1.0f);

        text.DrawFilledRect(
            x, y, width, height,
            D2D1::ColorF(0.008f, 0.022f, 0.030f, 0.94f));
        text.DrawRectOutline(x, y, width, height, 1.0f, accent);
        text.DrawFilledRect(x, y, 4.0f, height, accent);
        text.DrawSmallTextLineColor(
            L"CREW LINK // LOCAL CHANNEL",
            x + 15.0f, y + 8.0f, width - 28.0f, accent);

        const std::wstring name = Utf8ToWide(
            npc->name.empty() ? std::string("Unnamed crew member") : npc->name);
        text.DrawSmallTextLineColor(
            name.c_str(), x + 15.0f, y + 34.0f, width - 28.0f,
            D2D1::ColorF(0.92f, 0.96f, 0.97f, 1.0f));

        const std::string roleText = crew
            ? CrewRoleName(crew->role)
            : "Unassigned crew";
        const std::string stateText = brain
            ? NpcStateName(brain->state)
            : "No AI brain";
        const std::wstring dutyLine = Utf8ToWide(
            roleText + "   //   " + stateText);
        text.DrawSmallTextLineColor(
            dutyLine.c_str(), x + 15.0f, y + 57.0f, width - 28.0f,
            D2D1::ColorF(0.53f, 0.76f, 0.80f, 0.96f));

        const std::wstring description = Utf8ToWide(
            npc->description.empty()
                ? std::string("No personnel record available.")
                : npc->description);
        text.DrawSmallTextLineColor(
            description.c_str(), x + 15.0f, y + 83.0f, width - 28.0f,
            D2D1::ColorF(0.72f, 0.80f, 0.82f, 0.96f));

        std::string assignment = "Duty: available";
        if (crew && crew->assignedTask != INVALID_ENTITY)
        {
            assignment = "Duty: autonomous ship task #" +
                std::to_string(static_cast<unsigned int>(crew->assignedTask));
        }
        else if (brain && !brain->status.empty())
        {
            assignment = brain->status;
        }
        const std::wstring assignmentLine = Utf8ToWide(assignment);
        text.DrawSmallTextLineColor(
            assignmentLine.c_str(), x + 15.0f, y + 111.0f, width - 28.0f,
            crew && crew->assignedTask != INVALID_ENTITY
                ? D2D1::ColorF(0.98f, 0.67f, 0.20f, 1.0f)
                : D2D1::ColorF(0.38f, 0.86f, 0.65f, 1.0f));

        const std::wstring closeHint =
            interactionBinding + L" / ESC  CLOSE CHANNEL";
        text.DrawSmallTextLineColor(
            closeHint.c_str(),
            x + 15.0f, y + 140.0f, width - 28.0f,
            D2D1::ColorF(0.42f, 0.58f, 0.62f, 0.90f));
    }

    static void DrawCrewStatus(
        DWriteTextSystem& text,
        const GameHudStats& stats)
    {
        constexpr float x = 22.0f;
        constexpr float y = 22.0f;
        constexpr float width = 254.0f;
        constexpr float height = 48.0f;

        text.DrawFilledRect(
            x, y, width, height,
            D2D1::ColorF(0.01f, 0.024f, 0.032f, 0.80f));
        text.DrawRectOutline(
            x, y, width, height, 1.0f,
            D2D1::ColorF(0.10f, 0.30f, 0.38f, 0.86f));
        text.DrawFilledRect(
            x, y, 3.0f, height,
            stats.openCrewTasks > stats.busyCrew
                ? D2D1::ColorF(0.96f, 0.55f, 0.18f, 1.0f)
                : D2D1::ColorF(0.24f, 0.80f, 0.91f, 1.0f));

        const std::wstring crewLine =
            L"CREW NETWORK   " +
            std::to_wstring(stats.busyCrew) + L" / " +
            std::to_wstring(stats.availableCrew) + L" ASSIGNED";
        text.DrawSmallTextLineColor(
            crewLine.c_str(), x + 12.0f, y + 4.0f, width - 18.0f,
            D2D1::ColorF(0.46f, 0.84f, 0.91f, 0.96f));

        std::wstring statusLine =
            L"OPEN TASKS  " + std::to_wstring(stats.openCrewTasks);
        if (stats.simulationPaused)
            statusLine += L"   //   SIM PAUSED";
        else if (std::abs(stats.timeScale - 1.0f) > 0.001f)
            statusLine += L"   //   TIME x" + FormatFixed1(stats.timeScale);
        text.DrawSmallTextLineColor(
            statusLine.c_str(), x + 12.0f, y + 24.0f, width - 18.0f,
            D2D1::ColorF(0.70f, 0.78f, 0.80f, 0.94f));
    }

    static void DrawNotification(
        DWriteTextSystem& text,
        const std::string& message,
        bool success,
        float screenWidth)
    {
        constexpr float width = 460.0f;
        constexpr float height = 34.0f;
        const float x = (std::max)(12.0f, screenWidth * 0.5f - width * 0.5f);
        constexpr float y = 72.0f;
        const D2D1_COLOR_F accent = success
            ? D2D1::ColorF(0.30f, 0.90f, 0.68f, 1.0f)
            : D2D1::ColorF(0.98f, 0.42f, 0.20f, 1.0f);

        text.DrawFilledRect(
            x, y, width, height,
            D2D1::ColorF(0.01f, 0.025f, 0.032f, 0.92f));
        text.DrawRectOutline(x, y, width, height, 1.0f, accent);
        text.DrawFilledRect(x, y, 4.0f, height, accent);
        const std::wstring wide = Utf8ToWide(message);
        text.DrawSmallTextLineColor(
            wide.c_str(), x + 14.0f, y + 7.0f, width - 26.0f, accent);
    }

    static void DrawCrosshair(
        DWriteTextSystem& text,
        bool canInteract, float x, float y, bool shotFlash)
    {
        const D2D1_COLOR_F color = shotFlash ? D2D1::ColorF(1.0f, 0.70f, 0.18f, 1.0f) : canInteract
            ? D2D1::ColorF(0.30f, 0.94f, 0.72f, 0.95f)
            : D2D1::ColorF(0.66f, 0.86f, 0.91f, 0.78f);

        constexpr float gap = 5.0f;
        constexpr float length = 7.0f;
        constexpr float thickness = 1.5f;

        text.DrawFilledRect(
            x - gap - length,
            y - thickness * 0.5f,
            length,
            thickness,
            color);
        text.DrawFilledRect(
            x + gap,
            y - thickness * 0.5f,
            length,
            thickness,
            color);
        text.DrawFilledRect(
            x - thickness * 0.5f,
            y - gap - length,
            thickness,
            length,
            color);
        text.DrawFilledRect(
            x - thickness * 0.5f,
            y + gap,
            thickness,
            length,
            color);
        text.DrawFilledRect(
            x - 1.0f,
            y - 1.0f,
            2.0f,
            2.0f,
            color);
    }

    static void DrawInteractionPrompt(
        DWriteTextSystem& text,
        const std::string& prompt,
        const std::wstring& interactionBinding,
        float screenWidth,
        float screenHeight)
    {
        constexpr float width = 410.0f;
        constexpr float height = 45.0f;
        const float x =
            (std::max)(12.0f, screenWidth * 0.5f - width * 0.5f);
        const float y =
            (std::max)(80.0f, screenHeight * 0.62f);
        const D2D1_COLOR_F accent =
            D2D1::ColorF(0.30f, 0.94f, 0.72f, 1.0f);

        text.DrawFilledRect(
            x,
            y,
            width,
            height,
            D2D1::ColorF(0.01f, 0.025f, 0.032f, 0.90f));
        text.DrawRectOutline(
            x,
            y,
            width,
            height,
            1.0f,
            D2D1::ColorF(0.14f, 0.42f, 0.40f, 0.95f));
        text.DrawFilledRect(
            x,
            y,
            3.0f,
            height,
            accent);

        const float badgeWidth = (std::max)(29.0f,
            13.0f + static_cast<float>(interactionBinding.size()) * 7.0f);
        const float contentX = x + badgeWidth + 23.0f;
        text.DrawFilledRect(
            x + 11.0f,
            y + 8.0f,
            badgeWidth,
            29.0f,
            D2D1::ColorF(0.08f, 0.20f, 0.21f, 0.96f));
        text.DrawRectOutline(
            x + 11.0f,
            y + 8.0f,
            badgeWidth,
            29.0f,
            1.0f,
            accent);
        text.DrawSmallTextLineColor(
            interactionBinding.c_str(),
            x + 18.0f,
            y + 11.0f,
            badgeWidth - 8.0f,
            accent);

        text.DrawSmallTextLineColor(
            L"INTERACT",
            contentX,
            y + 3.0f,
            86.0f,
            D2D1::ColorF(0.40f, 0.65f, 0.68f, 0.92f));

        const std::wstring widePrompt = Utf8ToWide(prompt);
        text.DrawSmallTextLineColor(
            widePrompt.c_str(),
            contentX,
            y + 21.0f,
            width - (contentX - x) - 12.0f,
            D2D1::ColorF(0.88f, 0.95f, 0.95f, 1.0f));
    }

    static void DrawInventory(
        DWriteTextSystem& text,
        ECSRegistry& registry,
        Entity player,
        float screenWidth,
        float screenHeight)
    {
        constexpr float slotWidth = 108.0f;
        constexpr float slotHeight = 55.0f;
        constexpr float gap = 7.0f;
        constexpr float panelPadding = 9.0f;
        constexpr float headerHeight = 21.0f;
        constexpr float panelWidth =
            slotWidth * static_cast<float>(InventoryComponent::SlotCount) +
            gap * static_cast<float>(InventoryComponent::SlotCount - 1) +
            panelPadding * 2.0f;
        constexpr float panelHeight =
            headerHeight + slotHeight + panelPadding * 2.0f;

        const float panelX =
            (std::max)(10.0f, screenWidth * 0.5f - panelWidth * 0.5f);
        const float panelY =
            (std::max)(12.0f, screenHeight - panelHeight - 18.0f);

        text.DrawFilledRect(
            panelX,
            panelY,
            panelWidth,
            panelHeight,
            D2D1::ColorF(0.009f, 0.022f, 0.030f, 0.86f));
        text.DrawRectOutline(
            panelX,
            panelY,
            panelWidth,
            panelHeight,
            1.0f,
            D2D1::ColorF(0.10f, 0.28f, 0.36f, 0.90f));
        text.DrawSmallTextLineColor(
            L"QUICK ACCESS // 1-3",
            panelX + panelPadding,
            panelY + 2.0f,
            panelWidth - panelPadding * 2.0f,
            D2D1::ColorF(0.36f, 0.69f, 0.76f, 0.94f));

        const auto* inventory = player != INVALID_ENTITY
            ? registry.GetComponent<InventoryComponent>(player)
            : nullptr;

        Entity legacyWeapon = INVALID_ENTITY;
        if (!inventory && player != INVALID_ENTITY)
        {
            if (const auto* equipped =
                registry.GetComponent<EquippedWeaponComponent>(player))
            {
                legacyWeapon = equipped->weapon;
            }
        }

        for (std::size_t slot = 0;
            slot < InventoryComponent::SlotCount;
            ++slot)
        {
            const bool selected = inventory
                ? inventory->activeSlot == slot
                : slot == 0;
            const Entity item = inventory
                ? inventory->GetItem(slot)
                : (slot == 0 ? legacyWeapon : INVALID_ENTITY);
            const bool validItem =
                item != INVALID_ENTITY &&
                registry.IsAlive(item) &&
                registry.HasComponent<WeaponComponent>(item);

            const float x =
                panelX + panelPadding +
                static_cast<float>(slot) * (slotWidth + gap);
            const float y = panelY + panelPadding + headerHeight;
            const D2D1_COLOR_F accent = selected
                ? D2D1::ColorF(0.26f, 0.84f, 0.96f, 1.0f)
                : D2D1::ColorF(0.18f, 0.34f, 0.40f, 0.88f);

            text.DrawFilledRect(
                x,
                y,
                slotWidth,
                slotHeight,
                selected
                ? D2D1::ColorF(0.035f, 0.12f, 0.15f, 0.94f)
                : D2D1::ColorF(0.018f, 0.045f, 0.056f, 0.90f));
            text.DrawRectOutline(
                x,
                y,
                slotWidth,
                slotHeight,
                selected ? 2.0f : 1.0f,
                accent);
            text.DrawFilledRect(
                x,
                y,
                selected ? slotWidth : 3.0f,
                selected ? 3.0f : slotHeight,
                accent);

            const std::wstring key =
                std::to_wstring(static_cast<unsigned int>(slot + 1));
            text.DrawSmallTextLineColor(
                key.c_str(),
                x + 7.0f,
                y + 5.0f,
                16.0f,
                accent);

            const std::wstring itemName = validItem
                ? GetInventoryItemName(registry, item)
                : L"EMPTY";
            text.DrawSmallTextLineColor(
                itemName.c_str(),
                x + 25.0f,
                y + 5.0f,
                slotWidth - 31.0f,
                validItem
                ? D2D1::ColorF(0.86f, 0.94f, 0.95f, 1.0f)
                : D2D1::ColorF(0.34f, 0.43f, 0.46f, 0.86f));

            if (validItem)
            {
                const auto* weapon =
                    registry.GetComponent<WeaponComponent>(item);
                const std::wstring details =
                    weapon->isReloading
                    ? L"RELOAD " + FormatFixed1(weapon->reloadTimer) + L"s"
                    : L"DMG " + FormatFixed1(weapon->damage) +
                        L"  A " + std::to_wstring(weapon->ammoInMagazine) +
                        L"/" + std::to_wstring(weapon->reserveAmmo);

                text.DrawSmallTextLineColor(
                    details.c_str(),
                    x + 7.0f,
                    y + 29.0f,
                    slotWidth - 14.0f,
                    D2D1::ColorF(0.44f, 0.66f, 0.69f, 0.92f));
            }
        }
    }

    static void DrawEnvironmentStatus(
        DWriteTextSystem& text,
        const RoomComponent* room,
        float screenWidth)
    {
        constexpr float width = 272.0f;
        constexpr float height = 71.0f;
        const float x =
            (std::max)(16.0f, screenWidth - width - 22.0f);
        constexpr float y = 22.0f;

        text.DrawFilledRect(
            x,
            y,
            width,
            height,
            D2D1::ColorF(0.01f, 0.024f, 0.032f, 0.82f));
        text.DrawRectOutline(
            x,
            y,
            width,
            height,
            1.0f,
            D2D1::ColorF(0.10f, 0.29f, 0.36f, 0.88f));
        text.DrawFilledRect(
            x,
            y,
            width,
            3.0f,
            D2D1::ColorF(0.25f, 0.75f, 0.88f, 0.94f));

        const std::wstring roomName = room
            ? Utf8ToWide(room->roomName)
            : L"EXTERIOR / UNKNOWN";
        text.DrawSmallTextLineColor(
            (L"LOCAL ATMOSPHERE // " + roomName).c_str(),
            x + 10.0f,
            y + 7.0f,
            width - 20.0f,
            room
            ? D2D1::ColorF(0.55f, 0.82f, 0.87f, 0.96f)
            : D2D1::ColorF(0.96f, 0.43f, 0.20f, 1.0f));

        if (!room)
        {
            text.DrawSmallTextLineColor(
                L"NO PRESSURE TELEMETRY",
                x + 10.0f,
                y + 37.0f,
                width - 20.0f,
                D2D1::ColorF(0.82f, 0.48f, 0.32f, 0.94f));
            return;
        }

        const float pressurePercent =
            std::clamp(room->pressure, 0.0f, 1.0f) * 100.0f;
        const float oxygenPercent =
            std::clamp(room->oxygen, 0.0f, 1.0f) * 100.0f;
        const D2D1_COLOR_F pressureColor = pressurePercent >= 65.0f
            ? D2D1::ColorF(0.35f, 0.88f, 0.68f, 1.0f)
            : D2D1::ColorF(0.98f, 0.42f, 0.18f, 1.0f);

        text.DrawSmallTextLineColor(
            (L"P " + FormatFixed1(pressurePercent) + L"%").c_str(),
            x + 10.0f,
            y + 35.0f,
            76.0f,
            pressureColor);
        text.DrawSmallTextLineColor(
            (L"O2 " + FormatFixed1(oxygenPercent) + L"%").c_str(),
            x + 92.0f,
            y + 35.0f,
            86.0f,
            oxygenPercent >= 18.0f
            ? D2D1::ColorF(0.28f, 0.74f, 0.98f, 1.0f)
            : D2D1::ColorF(0.98f, 0.38f, 0.20f, 1.0f));
        text.DrawSmallTextLineColor(
            (FormatFixed1(room->temperature) + L" C").c_str(),
            x + 188.0f,
            y + 35.0f,
            72.0f,
            D2D1::ColorF(0.66f, 0.80f, 0.88f, 1.0f));
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

        case WorldEventType::FireStarted:
            return L"FIRE DETECTED";

        case WorldEventType::FireExtinguished:
            return L"FIRE EXTINGUISHED";

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
        const CharacterVitalsComponent* vitals,
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

        if (vitals)
        {
            text.DrawSmallTextLineColor(
                (L"PLAYER O2  " +
                    std::to_wstring(
                        static_cast<int>(
                            vitals->oxygen))).c_str(),
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
