#pragma once

#include <algorithm>
#include "NPCComponents.h"
#include "ECSRegistry.h"
#include "Gameplay/AI/NpcNavMeshSystem.h"
#include "Gameplay/AI/NpcCombatDecisionRules.h"
#include "Gameplay/AI/NpcCoverLifecycle.h"
#include "NpcCoverQuery.h"
#include "NpcTacticalDecisionSystem.h"
#include <cmath>

// Координатор ЖИЗНЕННОГО ЦИКЛА индивидуального укрытия.
//
// Этот файл связывает три независимых слоя:
//   * NpcTacticalDecisionSystem решает, НУЖНО ли укрытие;
//   * NpcCoverQuery решает, ГДЕ находится лучшая защищённая позиция;
//   * StateSystem физически исполняет записанный сюда moveTarget.
//
// Внутри остаются только фазы поведения:
//   выбор → движение → ожидание за стеной → peek → возврат/смена.
// Проверки кандидатов намеренно находятся в NpcCoverQuery.h, чтобы новый способ
// поиска можно было подключить как ещё один поставщик результата.
//
// reservedCover — это БРОНЬ ОТ ДВОЙНОГО НАЗНАЧЕНИЯ: пока Entity хранится в
// NpcTacticalComponent одного бойца, координатор не должен отдавать ту же точку
// другому. rejectedCover + rejectedCoverTimer — короткий чёрный список, чтобы
// застрявший NPC не выбирал одну и ту же плохую точку каждый кадр.
//
// ВАЖНО: flankTarget — не Cover Point. Это временная точка на NavMesh для
// обхода сбоку; она не обещает физическую защиту и не резервируется как укрытие.
class NpcCoverSystem
{
public:
    // Один кадр управления фазами индивидуального укрытия.
    // registry хранит состояния, deltaTime обновляет runtime-таймеры, world
    // передаётся NpcCoverQuery для проверки физической защиты.
    static void Update(
        ECSRegistry& registry,
        float deltaTime,
        btDynamicsWorld* world)
    {
        // Для raycast нужен физический мир. Некорректное время также нельзя
        // использовать для обновления таймеров тактического компонента.
        if (!world || !std::isfinite(deltaTime) || deltaTime <= 0.0f)
        {
            return;
        }

        // NPCCombatComponent означает, что сущность участвует в боевом цикле.
        registry.ForEach<NPCCombatComponent>(
            [&](Entity npcEntity, NPCCombatComponent& combat)
            {
                // Получаем все данные, необходимые именно для выбора укрытия.
                auto* npcBrain =
                    registry.GetComponent<NpcBrainComponent>(npcEntity);

                auto* npcTactical =
                    registry.GetComponent<NpcTacticalComponent>(npcEntity);

                const auto* npcFaction =
                    registry.GetComponent<FactionComponent>(npcEntity);

                const auto* npcTransform =
                    registry.GetComponent<TransformComponent>(npcEntity);

                const auto* npc =
                    registry.GetComponent<NPCComponent>(npcEntity);

                const auto* vitals =
                    registry.GetComponent<CharacterVitalsComponent>(npcEntity);

                // Сущность без полного набора индивидуального AI пропускаем.
                if (!npc ||
                    !npcBrain ||
                    !npcTactical ||
                    !npcFaction ||
                    !npcTransform ||
                    !vitals)
                {
                    return;
                }

                // Меняем диагностический текст только при реальном переходе.
                // Так Inspector объясняет текущее решение, а Capture создаёт
                // одну запись в журнале вместо одинаковой строки каждый кадр.
                const auto setTacticalStatus =
                    [&](const char* status, const char* transition)
                    {
                        if (npcTactical->status == status)
                            return;
                        npcTactical->status = status;
                        npcTactical->lastTacticalTransition = transition;
                    };

                const auto* modules =
                    registry.GetComponent<NpcCombatModulesComponent>(npcEntity);

                // Таймеры принадлежат индивидуальному тактическому поведению.
                // Раньше их уменьшал отключённый NpcSquadCombatSystem.
                npcTactical->underFireTime = (std::max)(
                    0.0f,
                    npcTactical->underFireTime - deltaTime);

                npcTactical->decisionTimer = (std::max)(
                    0.0f,
                    npcTactical->decisionTimer - deltaTime);

                npcTactical->rejectedCoverTimer = (std::max)(
                    0.0f,
                    npcTactical->rejectedCoverTimer - deltaTime);

                npcTactical->damageReactionCooldown = (std::max)(
                    0.0f,
                    npcTactical->damageReactionCooldown - deltaTime);

                npcTactical->incomingFireTurnTimer = (std::max)(
                    0.0f,
                    npcTactical->incomingFireTurnTimer - deltaTime);
                if (npcTactical->incomingFireTurnTimer <= 0.0f)
                    npcTactical->hasIncomingFireDirection = false;

                npcTactical->phaseTimer = (std::max)(
                    0.0f,
                    npcTactical->phaseTimer - deltaTime);

                // Отдельная пауза между очередями на ходу обязана тикать даже
                // при выключенном выборе укрытия: MovingFire — независимый
                // модуль и CoverSystem является владельцем общих tactical-таймеров.
                npcTactical->movingBurstPauseTimer = (std::max)(
                    0.0f,
                    npcTactical->movingBurstPauseTimer - deltaTime);

                npcTactical->recentDamageWindow = (std::max)(
                    0.0f,
                    npcTactical->recentDamageWindow - deltaTime);
                if (npcTactical->recentDamageWindow <= 0.0f)
                {
                    // Разнесённые во времени одиночные попадания не образуют
                    // одну серию, способную остановить штурм.
                    npcTactical->recentDamageHits = 0;
                }

                // Эти три таймера решают разные задачи:
                // tacticCommitTimer не даёт Auto-тактике метаться между стилями;
                // coverHoldTimer удерживает уже выбранную позицию;
                // coverValidationFailureTime, наоборот, увеличивается ниже только
                // пока защита непрерывно не проходит проверку.
                npcTactical->tacticCommitTimer = (std::max)(
                    0.0f,
                    npcTactical->tacticCommitTimer - deltaTime);

                // Дорога ДО укрытия не расходует время удержания. Таймер идёт,
                // когда NPC уже находится за стеной, выходит к peek или стреляет.
                // Иначе дальняя точка могла съесть все восемь секунд ещё в пути.
                const bool travellingToNewCover =
                    npcBrain->state == NpcState::TakingCover &&
                    !npcTactical->movingToPeek &&
                    !npcTactical->exposed;

                if (!travellingToNewCover)
                {
                    npcTactical->coverHoldTimer = (std::max)(
                        0.0f,
                        npcTactical->coverHoldTimer - deltaTime);
                }

                const bool receivedThreatSignal =
                    npcTactical->damageReactionPending;
                const bool receivedDamage =
                    receivedThreatSignal &&
                    npcTactical->damageReactionWasHit;
                // Сигнал урона будит принятие решения немедленно. Координаты
                // стрелявшего он не раскрывает — используется только память NPC.
                if (npcTactical->damageReactionPending)
                {

                    npcTactical->damageReactionPending = false;
                    npcTactical->damageReactionWasHit = false;
                    npcTactical->damageReactionCooldown = 2.0f;
                    npcTactical->decisionTimer = 0.0f;
                }

                // Пока укрытием владеет Combat Lab, автоматический селектор не
                // меняет его за спиной пользователя. Движение, коллизии и
                // ручные Peek/Return по-прежнему исполняются StateSystem.
                if (npcTactical->debugForcedCoverAction)
                {
                    setTacticalStatus(
                        "Combat Lab owns the current cover order",
                        "Automatic cover decisions paused for forced testing");
                    return;
                }

                if (modules && !modules->coverSelection)
                {
                    setTacticalStatus(
                        "Cover selection module disabled",
                        "Combat Lab disabled automatic cover decisions");
                    return;
                }

                // Выключенный или мёртвый NPC решений больше не принимает.
                if (!registry.HasComponent<ActiveComponent>(npcEntity))
                {
                    setTacticalStatus(
                        "Cover logic inactive: NPC entity is disabled",
                        "NPC became inactive");
                    return;
                }

                if (vitals->health <= 0.0f)
                {
                    setTacticalStatus(
                        "Cover logic inactive: NPC is dead",
                        "NPC can no longer make tactical decisions");
                    return;
                }

                // Ручная команда имеет приоритет над автономным выбором укрытия.
                if (npcBrain->command.IsActive())
                {
                    setTacticalStatus(
                        "Cover logic paused by manual command",
                        "Manual command has priority over autonomous cover");
                    return;
                }

                // Для выбора защиты нужна существующая активная цель.
                if (combat.targetEntity == INVALID_ENTITY ||
                    !registry.IsAlive(combat.targetEntity) ||
                    !registry.HasComponent<ActiveComponent>(combat.targetEntity))
                {
                    setTacticalStatus(
                        "Cover idle: no active combat target",
                        "No enemy is currently selected");
                    return;
                }

                // Не читаем Transform скрытой цели. Укрытие оценивается только
                // относительно последней позиции, которую NPC реально наблюдал.
                if (!combat.hasLastKnownPosition)
                {
                    setTacticalStatus(
                        "Cover idle: target position is not confirmed",
                        "NPC has no personally observed target position");
                    return;
                }

                // Защищаемся только от сущности вражеской фракции.
                const auto* targetFaction =
                    registry.GetComponent<FactionComponent>(combat.targetEntity);

                if (!targetFaction ||
                    !AreEnemies(npcFaction->faction, targetFaction->faction))
                {
                    setTacticalStatus(
                        "Cover idle: selected target is not an enemy",
                        "Faction check rejected the combat target");
                    return;
                }

                // Выбираем индивидуальный стиль только по известным самому NPC
                // данным. Transform скрытой цели здесь намеренно не читается:
                // иначе AI снова будет постоянно знать, где находится игрок.
                if (!modules || modules->automaticTactics)
                {
                    NpcTacticalDecisionSystem::UpdateStyle(
                        *npcTactical,
                        npcTransform->position,
                        combat.lastKnownPosition,
                        deltaTime);
                }

                // Авторское укрытие хранит Entity в reservedCover. Динамическое
                // укрытие Entity не имеет, поэтому учитываем отдельный флаг.
                const bool hasCurrentCover =
                    npcTactical->reservedCover != INVALID_ENTITY ||
                    npcTactical->dynamicCoverActive;
                bool forceCoverReevaluation = false;

                if (hasCurrentCover)
                {
                    // Если другая система уже вывела NPC из Attacking, peek
                    // считается прерванным немедленно. Defensive сначала
                    // возвращается за стену, остальные стили освобождают точку
                    // и продолжают движение — без залипшего exposed/peekPose.
                    if (npcTactical->exposed &&
                        npcBrain->state != NpcState::Attacking)
                    {
                        npcTactical->isPeeking = false;
                        npcTactical->wantsPeek = false;
                        npcTactical->peekState =
                            NpcTacticalComponent::PeekSide::none;

                        if (npcTactical->activeStyle ==
                            NpcTacticalStyle::Defensive)
                        {
                            npcTactical->exposed = false;
                            npcTactical->movingToPeek = false;
                            npcTactical->moveTarget =
                                npcTactical->coverPosition;
                            npcBrain->state = NpcState::TakingCover;
                            NpcNavMeshSystem::InvalidatePath(npcEntity);
                            setTacticalStatus(
                                "Returning after interrupted peek",
                                "Defensive peek ended before a firing stance");
                        }
                        else
                        {
                            ReleaseCurrentCover(*npcTactical);
                            npcTactical->decisionTimer = 0.0f;
                            npcBrain->state = NpcState::Chasing;
                            NpcNavMeshSystem::InvalidatePath(npcEntity);
                            setTacticalStatus(
                                "Advancing after interrupted peek",
                                "Combat state left the firing stance");
                        }
                        return;
                    }

                    // У авторской точки берём настроенную высоту защиты.
                    // Динамическая позиция проверяется на уровне корпуса стоящего
                    // NPC: низкая стенка пока не считается полноценной защитой.
                    const auto* currentCover =
                        npcTactical->reservedCover != INVALID_ENTITY
                            ? registry.GetComponent<NpcCoverPointComponent>(
                                npcTactical->reservedCover)
                            : nullptr;

                    const bool coverEntityValid =
                        npcTactical->dynamicCoverActive ||
                        (registry.IsAlive(npcTactical->reservedCover) &&
                            currentCover && currentCover->enabled);

                    const float protectionHeight = currentCover
                        ? currentCover->protectionHeight
                        : 1.25f;

                    const bool protectionValid =
                        coverEntityValid &&
                        NpcCoverQuery::IsProtectedFromTarget(
                            registry,
                            combat.targetEntity,
                            combat.lastKnownPosition,
                            npcTactical->coverPosition,
                            protectionHeight,
                            world);

                    // Один неудачный raycast ещё не доказывает, что позиция
                    // действительно раскрыта. Например, луч мог на один кадр
                    // пройти возле края движущегося коллайдера.
                    if (protectionValid)
                    {
                        npcTactical->coverValidationFailureTime = 0.0f;
                    }
                    else
                    {
                        npcTactical->coverValidationFailureTime += deltaTime;
                    }

                    const bool coverStillValid =
                        protectionValid ||
                        npcTactical->coverValidationFailureTime <
                            (std::max)(0.0f,
                                npcTactical->coverValidationGraceDuration);

                    if (coverStillValid)
                    {
                        const bool hitWhileExposed =
                            receivedDamage && npcTactical->exposed;

                        const bool exposureExpired =
                            npcTactical->phaseTimer <= 0.0f;

                        const bool defensiveLostContact =
                            npcTactical->activeStyle ==
                                NpcTacticalStyle::Defensive &&
                            !combat.targetVisible;

                        // Возвращаемся за стену после окна огня или сразу после
                        // попадания. Скобки обязательны: hitWhileExposed не должен
                        // срабатывать для NPC, который вообще не выглядывал.
                        if (npcTactical->exposed &&
                            ((npcBrain->state == NpcState::Attacking &&
                                (exposureExpired || hitWhileExposed)) ||
                                defensiveLostContact))
                        {
                            ++npcTactical->completedPeekCount;

                            // Только попадание, а не обычное окончание таймера,
                            // делает выглядывание неудачным.
                            if (hitWhileExposed)
                            {
                                ++npcTactical->failedPeekCount;
                            }

                            // Даже без попаданий один и тот же край нельзя
                            // разыгрывать бесконечно. У каждого стиля свой
                            // бюджет повторных выходов из текущего укрытия.
                            npcTactical->coverChangeRequested =
                                (!modules || modules->coverReposition) &&
                                NpcCombatDecisionRules::ShouldRepositionAfterPeekCycle(
                                    npcTactical->activeStyle,
                                    npcTactical->completedPeekCount,
                                    npcTactical->failedPeekCount,
                                    npcTactical->failedPeeksBeforeRelocate);

                            npcTactical->isPeeking = false;
                            npcTactical->wantsPeek = false;
                            npcTactical->exposed = false;
                            npcTactical->movingToPeek = false;
                            npcTactical->moveTarget = npcTactical->coverPosition;
                            npcBrain->state = NpcState::TakingCover;
                            NpcNavMeshSystem::InvalidatePath(npcEntity);
                            setTacticalStatus(
                                "Returning behind cover",
                                defensiveLostContact
                                    ? "Defensive style lost visual contact"
                                    : hitWhileExposed
                                    ? "Hit while exposed"
                                    : "Exposure timer expired");
                        }

                        // Запрошенную смену выполняем лишь после минимального
                        // времени удержания позиции и только из защищённой фазы.
                        // Это убирает бесконечный бег «дошёл — выстрелил — убежал».
                        const bool mayChangeCover =
                            (!modules || modules->coverReposition) &&
                            npcTactical->coverChangeRequested &&
                            npcTactical->coverHoldTimer <= 0.0f &&
                            !npcTactical->exposed &&
                            !npcTactical->movingToPeek;

                        if (mayChangeCover)
                        {
                            forceCoverReevaluation = true;
                            ReleaseCurrentCover(*npcTactical);
                            npcTactical->decisionTimer = 0.0f;
                            // Если новая точка не найдётся, NPC должен хотя бы
                            // продолжить бой, а не зависнуть в InCover уже без
                            // занятого укрытия.
                            npcBrain->state = NpcState::Chasing;
                            setTacticalStatus(
                                "Changing firing angle",
                                "Peek budget ended; selecting another position");
                            // Не выходим: ниже в этом же кадре ищем замену.
                        }
                        // Если смена не началась, после паузы снова резко выходим
                        // к peekPosition. Во время возврата TakingCover эту ветку
                        // StateSystem сам доведёт до InCover.
                        else if ((!modules || modules->peekCycle) &&
                            !npcTactical->coverChangeRequested &&
                            npcBrain->state == NpcState::InCover &&
                            npcTactical->phaseTimer <= 0.0f)
                        {
                            npcTactical->movingToPeek = true;
							npcTactical->wantsPeek = true;
                            npcTactical->moveTarget =
                                npcTactical->peekPosition;

                            npcBrain->state = NpcState::TakingCover;

                            setTacticalStatus(
                                "Leaving cover for a firing peek",
                                "Hide timer expired; moving to peek position");

                            NpcNavMeshSystem::InvalidatePath(npcEntity);
                        }

                        if (!mayChangeCover)
                        {
                            return;
                        }
                    }
                    else
                    {
                        // Защита отсутствует непрерывно дольше grace-периода.
                        // Авторскую точку временно запрещаем выбирать повторно;
                        // динамическая точка просто забывается.
						npcTactical->wantsPeek = false;
                        ReleaseCurrentCover(*npcTactical);
                        npcTactical->decisionTimer = 0.0f;
                        npcBrain->state = NpcState::Chasing;

                        setTacticalStatus(
                            "Current cover no longer protects",
                            "Cover validation failed longer than the grace period");
                    }
                }

                // У каждого стиля своя устойчивость к подавлению:
                //  * Defensive заранее занимает защиту;
                //  * Balanced реагирует и на near miss, и на попадание;
                //  * Assault не разворачивается от первой пули. Он отступает,
                //    только получив короткую серию реальных попаданий либо
                //    потеряв большую часть здоровья.
                const NpcCoverRequestDecision coverRequest =
                    NpcTacticalDecisionSystem::EvaluateCoverRequest(
                        *npcTactical,
                        *vitals);

                if (!coverRequest.requested && !forceCoverReevaluation)
                {
                    setTacticalStatus(
                        "Holding firing position: no recent incoming fire",
                        coverRequest.reason);
                    return;
                }

                if (npcTactical->decisionTimer > 0.0f)
                {
                    setTacticalStatus(
                        "Waiting before next cover evaluation",
                        "Cover decision cooldown is active");
                    return;
                }

                // NpcCoverSystem больше не знает, КАК искать позицию. Он
                // передаёт подтверждённые факты геометрическому модулю и
                // получает одинаковый результат для авторского и динамического
                // поставщика укрытий.
                const NpcCoverSearchResult searchResult =
                    NpcCoverQuery::FindBest(
                        registry,
                        combat.targetEntity,
                        npcBrain->currentRoom,
                        npcTransform->position,
                        combat.lastKnownPosition,
                        *npcTactical,
                        world,
                        !modules || modules->dynamicCover);

                if (!searchResult.found)
                {
                    npcTactical->decisionTimer = (std::max)(
                        0.2f,
                        npcTactical->decisionInterval);
                    npcTactical->noSuitableCover = true;
                    setTacticalStatus(
                        "No suitable individual cover",
                        "Authored and dynamic cover providers found no valid position");
                    return;
                }

                // Сохраняем результат выбора. StateSystem увидит TakingCover
                // и самостоятельно построит/исполнит маршрут к moveTarget.
                NpcCoverLifecycle::Assign(
                    *npcTactical,
                    searchResult.coverEntity,
                    searchResult.dynamic,
                    searchResult.coverPosition,
                    searchResult.peekPosition,
                    searchResult.crouched,
                    npcTransform->position,
                    false);
                setTacticalStatus(
                    searchResult.dynamic
                        ? "Moving to dynamically discovered cover"
                        : "Moving to authored individual cover",
                    searchResult.dynamic
                        ? "No authored point passed; NavMesh and raycasts found cover"
                        : "Tactical scoring selected an authored cover point");

                npcBrain->state = NpcState::TakingCover;

                // Старый путь преследования больше не должен оставаться
                // активным после назначения новой тактической цели.
                NpcNavMeshSystem::InvalidatePath(npcEntity);
            });
    }

private:
    // В этом классе намеренно остаётся только очистка runtime-состояния
    // занятого укрытия. Выбор стиля вынесен в NpcTacticalDecisionSystem,
    // а геометрические запросы — в NpcCoverQuery.

    // Полностью освобождает текущую позицию перед новым выбором. Для авторской
    // точки одновременно ставится короткий запрет на немедленное переизбрание.
    static void ReleaseCurrentCover(NpcTacticalComponent& tactical)
    {
        if (tactical.reservedCover != INVALID_ENTITY)
        {
            tactical.rejectedCover = tactical.reservedCover;
            tactical.rejectedCoverTimer = 4.0f;
        }

        NpcCoverLifecycle::Clear(tactical);
    }

};
