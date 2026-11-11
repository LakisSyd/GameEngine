#pragma once
#include "AnimationComponent.h"
#include "Components.h"
#include "ECSRegistry.h"
#include "Gameplay/Combat/WeaponComponent.h"
#include "Gameplay/AI/NpcCombatDecisionRules.h"
#include "Gameplay/Player/PlayerComponents.h"
#include "NPCComponents.h"
#include <algorithm>

class CharacterAnimationControllerSystem
{
public:
    static AnimationBinding* FindBinding(AnimationComponent& anim, AnimationState state)
    {
        for (auto& binding : anim.animations)
        {
            if (binding.state == state)
                return &binding;
        }

        return nullptr;
    }

    static float GetClipDurationSeconds(const AssimpModelClass::AnimationClip& clip)
    {
        const float ticksPerSecond = clip.ticksPerSecond > 0.0f
            ? clip.ticksPerSecond
            : 24.0f;
        return clip.duration / ticksPerSecond;
    }

    static void Update(ECSRegistry& registry, float deltaTime)
    {
        // Порядок выбора анимации внутри одной сущности:
        // 1. Определяем обычную анимацию движения: ожидание, шаг или бег.
        // 2. Учитываем оружие и при необходимости добавляем верхний слой рук.
        // 3. Применяем боевые действия, одноразовые клипы и позу приседа.
        // 4. Режим Animation Studio обрабатывается последним и поэтому имеет
        //    наивысший приоритет над игровой логикой.
        registry.ForEach<AnimationComponent, CharacterMovement>(
            [&](Entity entity, AnimationComponent& anim, CharacterMovement& movement)
            {
                // Неактивная сущность не должна оставлять зависший слой рук
                // или неисполненный одноразовый запрос.
                if (!registry.HasComponent<ActiveComponent>(entity))
                {
                    anim.isPlaying = false;
                    anim.upperBodyAnimationIndex = -1;
                    anim.rifleAnimationWeapon = INVALID_ENTITY;
                    anim.rifleShotTimeRemaining = 0.0f;
                    anim.oneShotRequested = false;
                    anim.oneShotTimeRemaining = 0.0f;
                    return;
                }

                auto* player = registry.GetComponent<PlayerComponent>(entity);
                const auto* npcBrain = registry.GetComponent<NpcBrainComponent>(entity);
                const auto* npcTactical =
                    registry.GetComponent<NpcTacticalComponent>(entity);

                // У игрока состояние приседа задаётся вводом в системе движения.
                // У NPC та же поза включается тактической системой укрытий.
                // AnimationComponent получает уже готовый общий флаг и дальше
                // не обязан знать, кто именно управляет персонажем.
                if (player)
                    anim.crouchPose = movement.isCrouching;
                else if (npcBrain)
                    anim.crouchPose = npcTactical && npcTactical->crouched;
                else
                    anim.crouchPose = false;

                const PlayerComponent::AimMode aimMode = player
                    ? player->aimMode
                    : npcBrain && (npcBrain->state == NpcState::Attacking ||
                        npcBrain->state == NpcState::InCover)
                    ? PlayerComponent::AimMode::Shoulder : PlayerComponent::AimMode::Hip;

                auto* mesh = registry.GetComponent<MeshRendererComponent>(entity);
                if (!mesh || !mesh->model)
                    return;

                auto& model = *mesh->model;

                // Индексы клипов привязаны к конкретной модели. Поэтому ищем их
                // заново после замены файла модели, а не полагаемся на старое значение.
                auto resolve = [&](AnimationState state) {
                    auto* binding = FindBinding(anim, state);
                    if (!binding)
                        return -1;

                    binding->index = model.FindAnimationIndexBySemanticName(binding->name);
                    return binding->index;
                };

                // Базовый слой всегда следует за движением персонажа.
                anim.currentState = movement.locomotionState == PlayerLocomotionState::Idle
                    ? AnimationState::Idle : movement.locomotionState == PlayerLocomotionState::Run
                    ? AnimationState::Run : AnimationState::Walk;
                int base = resolve(anim.currentState);
                if (base < 0 && anim.currentState != AnimationState::Idle)
                    base = resolve(anim.currentState == AnimationState::Run
                        ? AnimationState::Walk : AnimationState::Run);
                if (base < 0) base = resolve(AnimationState::Idle);

                // Верхний слой накладывается от корня верхней части тела. Он нужен для
                // действий рук, не прерывающих обычную ходьбу.
                int upper = -1;
                bool rifleEquipped = false;
                bool pistolEquipped = false;

                // --- Оружие: стойка, ходьба, бег и короткая отдача. ---
                const auto* equippedWeapon = registry.GetComponent<EquippedWeaponComponent>(entity);
                const Entity animationWeapon = equippedWeapon ? equippedWeapon->weapon
                    : registry.HasComponent<WeaponComponent>(entity) ? entity : INVALID_ENTITY;
                if (animationWeapon != INVALID_ENTITY)
                {
                    if (auto* weapon = registry.GetComponent<WeaponComponent>(animationWeapon))
                    {
                        rifleEquipped = true;
                        pistolEquipped = weapon->pistolPose;
                        bool fullBodyRifleAnimation = false;

                        if (!pistolEquipped && aimMode == PlayerComponent::AimMode::Hip && anim.currentState == AnimationState::Idle)
                        {
                            const int rifleHipIdle = resolve(AnimationState::RifleHipIdle);
                            if (rifleHipIdle >= 0)
                            {
                                base = rifleHipIdle;
                                fullBodyRifleAnimation = true;
                            }
                        }

                        if (!pistolEquipped && aimMode == PlayerComponent::AimMode::Hip &&
                            anim.currentState == AnimationState::Walk)
                        {
                            const int rifleWalk = resolve(AnimationState::RifleWalk);

                            if (rifleWalk >= 0)
                            {
                                base = rifleWalk;
                                fullBodyRifleAnimation = true;
                            }
                        }

                        if (!pistolEquipped && anim.currentState == AnimationState::Run)
                        {
                            const int rifleRun = resolve(AnimationState::RifleRun);
                            if (rifleRun >= 0)
                            {
                                base = rifleRun;
                                fullBodyRifleAnimation = true;
                            }
                        }
                        // Явная привязка в компоненте анимации имеет приоритет.
                        // Пустая привязка берёт имя клипа из профиля оружия —
                        // это сохраняет совместимость со старыми сценами.
                        auto resolveWeaponClip = [&](AnimationState state,
                            const std::string& weaponName) {
                            auto* binding = FindBinding(anim, state);
                            const std::string& name = binding && !binding->name.empty()
                                ? binding->name : weaponName;
                            const int index = model.FindAnimationIndexBySemanticName(name);
                            if (binding) binding->index = index;
                            return index;
                        };
                        upper = fullBodyRifleAnimation ? -1
                            : resolveWeaponClip(AnimationState::RifleIdle,
                                weapon->rifleIdleAnimation);
                        if (pistolEquipped)
                        {
                            const int pistolBase = resolve(movement.isMoving
                                ? AnimationState::PistolWalk : AnimationState::PistolIdle);
                            if (pistolBase >= 0) { base = pistolBase; upper = -1; }
                        }
                        const int shotIndex = pistolEquipped
                            ? resolve(AnimationState::PistolShot)
                            : resolveWeaponClip(AnimationState::RifleGun,
                                weapon->rifleGunAnimation);
                        const auto* shotClip = model.GetAnimation(shotIndex);
                        bool shotJustFired = false;
                        if (anim.rifleAnimationWeapon != animationWeapon)
                        {
                            anim.rifleAnimationWeapon = animationWeapon;
                            anim.observedShotsFired = weapon->shotsFired;
                            anim.rifleShotTimeRemaining = 0.0f;
                            // Система оружия записывает 0.08 в кадр выстрела.
                            // Это также ловит выстрел в кадр экипировки оружия.
                            shotJustFired = weapon->shotFlashTime >= 0.079f;
                        }
                        else if (weapon->shotsFired != anim.observedShotsFired)
                        {
                            anim.observedShotsFired = weapon->shotsFired;
                            shotJustFired = true;
                        }
                        if (!shotClip)
                            anim.rifleShotTimeRemaining = 0.0f;
                        else if (shotJustFired)
                        {
                            anim.rifleShotTimeRemaining =
                                GetClipDurationSeconds(*shotClip) /
                                anim.GetClipSpeed(shotClip->name);
                            anim.upperBodyTime = 0.0f;
                        }

                        if (shotClip && anim.rifleShotTimeRemaining > 0.0f)
                        {
                            upper = shotIndex;
                            anim.rifleShotTimeRemaining = (std::max)(
                                0.0f, anim.rifleShotTimeRemaining - deltaTime);
                        }
                    }
                }
                if (!rifleEquipped)
                {
                    anim.rifleAnimationWeapon = INVALID_ENTITY;
                    anim.rifleShotTimeRemaining = 0.0f;
                }
                // Неподвижный безоружный NPC сохраняет защитную стойку между
                // ударами. Во время преследования остаётся обычная анимация
                // движения с опущенными руками.
                const auto* npcCombat = registry.GetComponent<NPCCombatComponent>(entity);
                const bool unarmedNpcCombat = !player && !rifleEquipped &&
                    npcBrain && npcCombat && !npcBrain->command.IsActive() &&
                    (npcBrain->state == NpcState::Chasing ||
                        npcBrain->state == NpcState::Attacking);
                if (!player && !rifleEquipped && npcBrain && npcCombat &&
                    !npcBrain->command.IsActive() &&
                    npcBrain->state == NpcState::Attacking && !movement.isMoving &&
                    registry.IsAlive(npcCombat->targetEntity))
                {
                    const int guard = model.FindAnimationIndexBySemanticName(
                        anim.meleeActions.guardWorldClip);
                    if (guard >= 0) base = guard;
                }

                // Явно назначенная базовая анимация атаки имеет приоритет.
                // Если её нет, игрок и NPC используют настроенную защитную
                // стойку, чтобы ноги не скользили во время удара.
                auto resolveAttackBase = [&]() {
                    if (!anim.meleeActions.attackBaseWorldClip.empty())
                    {
                        const int custom = model.FindAnimationIndexBySemanticName(
                            anim.meleeActions.attackBaseWorldClip);
                        if (custom >= 0) return custom;
                    }
                    return model.FindAnimationIndexBySemanticName(
                        anim.meleeActions.guardWorldClip);
                };

                // --- Блок ближнего боя игрока: защитная стойка и удар. ---
                if (player && !rifleEquipped && player->meleeGuardActive)
                {
                    const int guard = model.FindAnimationIndexBySemanticName(
                        anim.meleeActions.guardWorldClip);
                    if (anim.currentState == AnimationState::Idle)
                    {
                        if (guard >= 0) base = guard;
                    }
                    else if (anim.currentState == AnimationState::Walk)
                    {
                        const int movingGuard = model.FindAnimationIndexBySemanticName(
                            anim.meleeActions.guardMoveWorldClip);
                        if (movingGuard >= 0) base = movingGuard;
                        else if (guard >= 0) upper = guard;
                    }
                    else if (guard >= 0) upper = guard;
                }
                if (player && player->punchActive)
                {
                    // Удар проигрывается один раз поверх ходьбы. Его время
                    // ведёт система удара, а руки направляет IK после сэмпла.
                    std::string punchName;
                    if (player->punchVariant >= 0 &&
                        player->punchVariant < static_cast<int>(anim.punchVariants.size()))
                        punchName = anim.punchVariants[player->punchVariant].worldClip;
                    upper = punchName.empty()
                        ? resolve(AnimationState::Punch)
                        : model.FindAnimationIndexBySemanticName(punchName);
                    if (upper < 0) upper = resolve(AnimationState::Punch);
                    const int attackBase = resolveAttackBase();
                    if (attackBase >= 0) base = attackBase;
                    anim.poseVariant = player->punchVariant;
                    anim.rifleShotTimeRemaining = 0.0f;
                }
                else if (!anim.oneShotRequested && anim.oneShotTimeRemaining <= 0.0f)
                {
                    anim.poseVariant = -1;
                }

                // Вариант удара нужен не только игроку: NPC запускает его через
                // oneShot и должен получить тот же клип и ключи позы, что и игрок.
                auto resolveOneShotClip = [&]() {
                    if (anim.oneShotState == AnimationState::Punch &&
                        anim.poseVariant >= 0 &&
                        anim.poseVariant < static_cast<int>(anim.punchVariants.size()))
                    {
                        const std::string& punchName =
                            anim.punchVariants[anim.poseVariant].worldClip;
                        if (!punchName.empty())
                        {
                            const int punchClip =
                                model.FindAnimationIndexBySemanticName(punchName);
                            if (punchClip >= 0)
                                return punchClip;
                        }
                    }
                    return resolve(anim.oneShotState);
                };

                // Запрос одноразовой анимации используется и NPC, и в будущем
                // может использоваться любым игровым персонажем. В отличие от
                // обычного состояния движения, этот клип не заменяется
                // ожиданием или ходьбой до конца.
                if (anim.oneShotRequested)
                {
                    const int requestedClip = resolveOneShotClip();
                    anim.oneShotRequested = false;
                    anim.oneShotTimeRemaining = 0.0f;

                    if (const auto* clip = model.GetAnimation(requestedClip))
                    {
                        anim.oneShotTimeRemaining = GetClipDurationSeconds(*clip) /
                            anim.GetClipSpeed(clip->name);
                        anim.upperBodyTime = 0.0f;
                    }
                }

                if (anim.oneShotTimeRemaining > 0.0f)
                {
                    const int oneShotClip = resolveOneShotClip();
                    if (oneShotClip >= 0)
                    {
                        // Одноразовое действие выше обычной позы оружия и удара
                        // игрока. Режим студии анимации ниже имеет ещё больший
                        // приоритет, поскольку он предназначен для авторинга.
                        upper = oneShotClip;
                        if (!rifleEquipped && anim.oneShotState == AnimationState::Punch)
                        {
                            const int attackBase = resolveAttackBase();
                            if (attackBase >= 0) base = attackBase;
                        }
                        anim.oneShotTimeRemaining = (std::max)(
                            0.0f,
                            anim.oneShotTimeRemaining - deltaTime);
                    }
                    else
                    {
                        // Удалённый или переименованный клип не должен оставлять
                        // компонент в бесконечном состоянии ожидания.
                        anim.oneShotTimeRemaining = 0.0f;
                    }
                }
                else if (!player || !player->punchActive)
                {
                    anim.poseVariant = -1;
                }

                const bool unarmedPunchActive = !rifleEquipped &&
                    ((player && player->punchActive) ||
                        (anim.oneShotState == AnimationState::Punch &&
                            anim.oneShotTimeRemaining > 0.0f));

                if (npcTactical && npcBrain &&
                    NpcCombatDecisionRules::CanUsePeekPose(
                        npcTactical->isPeeking,
                        npcTactical->exposed,
                        movement.isMoving,
                        npcBrain->state) &&
                    npcTactical->peekState == NpcTacticalComponent::PeekSide::left &&
                    rifleEquipped)
                {
                    // Выглядывание — полнотелая винтовочная поза, поэтому
                    // отдельный верхний слой оружия поверх неё не нужен.
                    const int peekLeft = resolve(AnimationState::RiflePeekLeft);
                    if (peekLeft >= 0)
                    {
                        base = peekLeft;
                        upper = -1;
                        anim.currentState = AnimationState::RiflePeekLeft;
                    }
                }

                // Единый блок приседа работает и для игрока, и для NPC.
                // crouchPose выше уже получил состояние из правильного источника.
                // Тип оружия и движение определяют конкретный вариант клипа.
                else if (anim.crouchPose && !unarmedPunchActive && !unarmedNpcCombat)
                {
                    const AnimationState crouchState = pistolEquipped
                        ? (movement.isMoving ? AnimationState::CrouchPistolWalk : AnimationState::CrouchPistolIdle)
                        : rifleEquipped
                        ? (movement.isMoving ? AnimationState::CrouchRifleWalk : AnimationState::CrouchRifleIdle)
                        : (movement.isMoving ? AnimationState::CrouchWalk : AnimationState::CrouchIdle);
                    const int crouchBase = resolve(crouchState);
                    if (crouchBase >= 0) { base = crouchBase; anim.currentState = crouchState; }
                }

                // На остановке поиск проигрывает до трёх назначенных клипов
                // по очереди. Слот меняется после окончания клипа, а не каждый
                // кадр. При движении к предполагаемой точке остаётся ходьба.
                const bool waitingInSearch = !player && !rifleEquipped && npcBrain &&
                    !npcBrain->command.IsActive() &&
                    npcBrain->state == NpcState::Searching && !movement.isMoving;
                if (waitingInSearch)
                {
                    if (!anim.searchAnimationActive)
                    {
                        anim.searchAnimationSlot = -1;
                        anim.searchAnimationTimeRemaining = 0.0f;
                    }
                    anim.searchAnimationActive = true;
                    anim.searchAnimationTimeRemaining -= (std::max)(0.0f, deltaTime);
                    const AnimationState slots[] = {
                        AnimationState::Search1, AnimationState::Search2, AnimationState::Search3 };
                    if (anim.searchAnimationTimeRemaining <= 0.0f)
                    {
                        for (int attempt = 0; attempt < 3; ++attempt)
                        {
                            anim.searchAnimationSlot = (anim.searchAnimationSlot + 1) % 3;
                            const int clipIndex = resolve(slots[anim.searchAnimationSlot]);
                            if (const auto* clip = model.GetAnimation(clipIndex))
                            {
                                anim.searchAnimationTimeRemaining = (std::max)(0.01f,
                                    GetClipDurationSeconds(*clip) / anim.GetClipSpeed(clip->name));
                                anim.currentTime = 0.0f;
                                break;
                            }
                        }
                    }
                    if (anim.searchAnimationTimeRemaining > 0.0f && anim.searchAnimationSlot >= 0)
                    {
                        anim.currentState = slots[anim.searchAnimationSlot];
                        base = resolve(anim.currentState);
                    }
                    upper = -1;
                }
                else
                {
                    anim.searchAnimationActive = false;
                    anim.searchAnimationTimeRemaining = 0.0f;
                }

                // --- Студия анимации. Предпросмотр намеренно последний: он
                // должен показывать выбранный автором клип, а не игровую логику. ---
                if (anim.socketPreviewEnabled)
                {
                    if (anim.socketPreviewPunch)
                    {
                        // Базовый слой выбирается так же, как при игровом
                        // ударе стоя, при ходьбе или беге.
                        const AnimationState movementState =
                            anim.socketPreviewPunchMovement == 1 ? AnimationState::Walk
                            : anim.socketPreviewPunchMovement == 2 ? AnimationState::Run
                            : AnimationState::Idle;
                        base = resolve(movementState);
                        if (base < 0) base = resolve(AnimationState::Idle);
                        if (movementState == AnimationState::Idle &&
                            anim.meleeActions.guardEnabled)
                        {
                            const int guard = model.FindAnimationIndexBySemanticName(
                                anim.meleeActions.guardWorldClip);
                            if (guard >= 0) base = guard;
                        }
                        else if (anim.meleeActions.guardEnabled &&
                            movementState == AnimationState::Walk)
                        {
                            const int movingGuard = model.FindAnimationIndexBySemanticName(
                                anim.meleeActions.guardMoveWorldClip);
                            if (movingGuard >= 0) base = movingGuard;
                        }
                        // Явно выбранная боевая основа имеет приоритет над
                        // автоматическим ожиданием, ходьбой и защитной стойкой,
                        // чтобы предпросмотр совпадал с режимом Play.
                        const int attackBase = resolveAttackBase();
                        if (attackBase >= 0) base = attackBase;
                        const int variant = anim.socketPreviewPunchVariant;
                        std::string punchName;
                        if (variant >= 0 && variant < static_cast<int>(anim.punchVariants.size()))
                            punchName = anim.punchVariants[variant].worldClip;
                        upper = punchName.empty()
                            ? resolve(AnimationState::Punch)
                            : model.FindAnimationIndexBySemanticName(punchName);
                        if (upper < 0) upper = resolve(AnimationState::Punch);
                        anim.poseVariant = variant;
                    }
                    else
                    {
                        // Обычный просмотр показывает целиком выбранный клип.
                        if (model.GetAnimation(anim.socketPreviewClip))
                            base = anim.socketPreviewClip;
                        upper = -1;
                        anim.poseVariant = -1;
                    }
                }
                // Фиксируем выбранные слои. При смене верхнего клипа его таймлайн
                // должен начаться с начала, иначе новая анимация прыгнет в середину.
                if (anim.upperBodyAnimationIndex != upper)
                    anim.upperBodyTime = 0.0f;
                anim.upperBodyAnimationIndex = upper;
                if (base < 0) base = upper;
                anim.isPlaying = base >= 0;
                if (anim.animationIndex != base)
                {
                    anim.animationIndex = base;
                    anim.currentTime = 0.0f;
                }
            });
    }
};
using AnimationController = CharacterAnimationControllerSystem;
