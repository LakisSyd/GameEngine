#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

// Автоматические части боя включаются независимо друг от друга. Физическое
// движение, поворот модели и анимация сюда намеренно не входят: это исполнители,
// которыми пользуются и обычные команды NPC, и Combat Lab.
enum class NpcCombatModule : std::uint32_t
{
    Perception           = 1u << 0,
    Engagement           = 1u << 1,
    Pursuit              = 1u << 2,
    StationaryFire       = 1u << 3,
    Search               = 1u << 4,
    AutomaticTactics     = 1u << 5,
    CoverSelection       = 1u << 6,
    PeekCycle            = 1u << 7,
    CoverReposition      = 1u << 8,
    DynamicCover         = 1u << 9,
    MovingFire           = 1u << 10,
    IncomingFireReaction = 1u << 11,
    SquadTactics         = 1u << 12
};

enum class NpcCombatModulePreset
{
    ManualSandbox,
    MinimalDuel,
    CoverAndPeek,
    FullIndividual,
    Legacy
};

constexpr std::uint32_t NpcCombatModuleBit(NpcCombatModule module)
{
    return static_cast<std::uint32_t>(module);
}

// Сохраняемая конфигурация модулей одного NPC. Новые NPC начинают с простого
// дуэлянта: увидеть -> приблизиться -> остановиться -> выстрелить. Продвинутая
// тактика подключается из Combat Lab по одному слою.
struct NpcCombatModulesComponent
{
    bool perception = true;
    bool engagement = true;
    bool pursuit = true;
    bool stationaryFire = true;

    bool search = false;
    bool automaticTactics = false;
    bool coverSelection = false;
    bool peekCycle = false;
    bool coverReposition = false;
    bool dynamicCover = false;
    bool movingFire = false;
    bool incomingFireReaction = false;
    bool squadTactics = false;

    // Эти два поля принадлежат runtime. Они нужны, чтобы очистить старое
    // состояние ровно в момент выключения модуля, но не мешать принудительной
    // команде Combat Lab проверить отключённую автоматику.
    std::uint32_t runtimePreviousMask = 0;
    bool runtimeMaskInitialized = false;
    // Combat Lab может временно открыть только нужный исполнитель, не включая
    // соответствующую автоматику и не сохраняя override в сцену.
    std::uint32_t runtimeForcedMask = 0;
    float runtimeForcedTime = 0.0f;

    std::uint32_t EnabledMask() const
    {
        std::uint32_t result = 0;
        const auto add = [&](bool enabled, NpcCombatModule module)
        {
            if (enabled)
                result |= NpcCombatModuleBit(module);
        };

        add(perception, NpcCombatModule::Perception);
        add(engagement, NpcCombatModule::Engagement);
        add(pursuit, NpcCombatModule::Pursuit);
        add(stationaryFire, NpcCombatModule::StationaryFire);
        add(search, NpcCombatModule::Search);
        add(automaticTactics, NpcCombatModule::AutomaticTactics);
        add(coverSelection, NpcCombatModule::CoverSelection);
        add(peekCycle, NpcCombatModule::PeekCycle);
        add(coverReposition, NpcCombatModule::CoverReposition);
        add(dynamicCover, NpcCombatModule::DynamicCover);
        add(movingFire, NpcCombatModule::MovingFire);
        add(incomingFireReaction, NpcCombatModule::IncomingFireReaction);
        add(squadTactics, NpcCombatModule::SquadTactics);
        return result;
    }

    bool IsEnabled(NpcCombatModule module) const
    {
        return (EnabledMask() & NpcCombatModuleBit(module)) != 0;
    }

    bool IsOperational(NpcCombatModule module) const
    {
        const std::uint32_t bit = NpcCombatModuleBit(module);
        return IsEnabled(module) ||
            (runtimeForcedTime > 0.0f && (runtimeForcedMask & bit) != 0);
    }

    void ForceForDebug(std::uint32_t moduleMask, float duration)
    {
        if (moduleMask == 0 || !std::isfinite(duration) || duration <= 0.0f)
            return;

        runtimeForcedMask |= moduleMask;
        runtimeForcedTime = (std::max)(
            runtimeForcedTime,
            duration);
    }

    void TickDebugOverride(float deltaTime)
    {
        if (runtimeForcedTime <= 0.0f)
            return;

        const float safeDeltaTime = std::isfinite(deltaTime)
            ? (std::max)(0.0f, deltaTime)
            : 0.0f;
        runtimeForcedTime = (std::max)(
            0.0f,
            runtimeForcedTime - safeDeltaTime);
        if (runtimeForcedTime <= 0.0f)
            runtimeForcedMask = 0;
    }

    void ClearDebugOverride()
    {
        runtimeForcedMask = 0;
        runtimeForcedTime = 0.0f;
    }

    void ApplyPreset(NpcCombatModulePreset preset)
    {
        // Смена профиля возвращает чистый воспроизводимый baseline. Временное
        // разрешение, выданное кнопкой Combat Lab, не должно пережить профиль.
        ClearDebugOverride();

        perception = false;
        engagement = false;
        pursuit = false;
        stationaryFire = false;
        search = false;
        automaticTactics = false;
        coverSelection = false;
        peekCycle = false;
        coverReposition = false;
        dynamicCover = false;
        movingFire = false;
        incomingFireReaction = false;
        squadTactics = false;

        if (preset == NpcCombatModulePreset::ManualSandbox)
            return;

        perception = true;
        engagement = true;
        pursuit = true;
        stationaryFire = true;

        if (preset == NpcCombatModulePreset::MinimalDuel)
            return;

        coverSelection = true;
        peekCycle = true;
        incomingFireReaction = true;

        if (preset == NpcCombatModulePreset::CoverAndPeek)
            return;

        search = true;
        automaticTactics = true;
        coverReposition = true;
        dynamicCover = true;
        movingFire = true;

        // Групповой координатор пока намеренно не входит в FullIndividual.
        // Legacy отражает прежний набор доступных возможностей, включая squad.
        squadTactics = preset == NpcCombatModulePreset::Legacy;
    }
};

inline bool NpcCombatModuleOperational(
    const NpcCombatModulesComponent* modules,
    NpcCombatModule module)
{
    // Отсутствие нового компонента означает старое (legacy) поведение.
    return !modules || modules->IsOperational(module);
}

inline const char* NpcCombatModulePresetName(NpcCombatModulePreset preset)
{
    switch (preset)
    {
    case NpcCombatModulePreset::ManualSandbox: return "Manual sandbox";
    case NpcCombatModulePreset::MinimalDuel: return "Minimal duel";
    case NpcCombatModulePreset::CoverAndPeek: return "Cover + peek";
    case NpcCombatModulePreset::FullIndividual: return "Full individual";
    case NpcCombatModulePreset::Legacy: return "Legacy";
    default: return "Unknown";
    }
}
