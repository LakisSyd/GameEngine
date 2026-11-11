param(
    [string]$Source = "Level.json",
    [string]$Output = "CombatTrainingGround.json"
)

# Полигон собирается из уже настроенного игрока/оружия/рига проекта. Скрипт
# намеренно оставлен рядом с инструментами: карту можно пересобрать после
# изменения профиля персонажа, не копируя сотни полей JSON вручную.
$scene = Get-Content -LiteralPath $Source -Raw | ConvertFrom-Json
$byId = @{}
foreach ($object in $scene.objects) { $byId[[int]$object.entity_id] = $object }

# Не полагаемся на старые Entity ID: редактор имеет право перенумеровать их
# после сохранения. Авторские компоненты являются стабильным источником истины.
$sourcePlayer = $scene.objects | Where-Object { $_.has_player_tag -eq $true } | Select-Object -First 1
if (-not $sourcePlayer) { throw "Source scene has no PlayerGameTag entity." }
$sourcePlayerId = [int]$sourcePlayer.entity_id
$sourcePlayerWeapon = $scene.objects | Where-Object {
    $_.has_weapon -eq $true -and [int]$_.weapon_owner -eq $sourcePlayerId
} | Select-Object -First 1
if (-not $sourcePlayerWeapon) { throw "Source scene has no weapon owned by the player." }
$sourceArms = $scene.objects | Where-Object {
    $_.has_first_person_view -eq $true -and [int]$_.first_person_owner -eq $sourcePlayerId
} | Select-Object -First 1
if (-not $sourceArms) { throw "Source scene has no first-person arms for the player." }
$sourceNpc = $scene.objects | Where-Object { $_.has_npc -eq $true } | Select-Object -First 1
if (-not $sourceNpc) { throw "Source scene has no NPC template." }

function Clone-Object([object]$value) {
    return ($value | ConvertTo-Json -Depth 100 | ConvertFrom-Json)
}
function Set-Position([object]$object, [float]$x, [float]$y, [float]$z) {
    $object.position.x = $x; $object.position.y = $y; $object.position.z = $z
}
function Add-Property([object]$object, [string]$name, [object]$value) {
    $object | Add-Member -NotePropertyName $name -NotePropertyValue $value -Force
}

$objects = [System.Collections.Generic.List[object]]::new()

# Основание и настроенный игрок с FP-руками и оружием.
$floor = Clone-Object $byId[0]
$floor.object_name = "Combat Arena Floor"
Set-Position $floor 0 2 0
$floor.scale = [pscustomobject]@{ x = 5.5; y = 1.0; z = 5.5 }
Add-Property $floor "has_navmesh_surface" $true
Add-Property $floor "navmesh_surface_enabled" $true
$objects.Add($floor)

$player = Clone-Object $sourcePlayer
Set-Position $player 0 3 -10
$player.rotation.y = 3.14159265
$playerWeaponId = [int]$sourcePlayerWeapon.entity_id
$player.inventory_slots = @($playerWeaponId, 4294967295, 4294967295)
$player.inventory_active_slot = 0
$player.equipped_weapon = $playerWeaponId
$objects.Add($player)
$playerWeapon = Clone-Object $sourcePlayerWeapon
$playerWeapon.weapon_owner = $sourcePlayerId
$objects.Add($playerWeapon)
$arms = Clone-Object $sourceArms
$arms.first_person_owner = $sourcePlayerId
$objects.Add($arms)

# Два автономных бойца с разных сторон арены. Они принадлежат
# разным фракциям, поэтому NPC-vs-NPC запускается без ручной настройки.
# Оружие остаётся отдельной Entity, как и у игрока.
foreach ($index in 0,1) {
    $npcId = 11 + $index
    $weaponId = 21 + $index
    $npc = Clone-Object $sourceNpc
    $npc.entity_id = $npcId; $npc.id = $npcId; $npc.active = $true
    $sideName = $(if ($index -eq 0) {"Iridian"} else {"Sirius"})
    $npc.object_name = "$sideName Arena NPC"
    $npc.npc_name = "$sideName Rifleman"
    $npc.npc_health = 100.0; $npc.npc_max_health = 100.0; $npc.npc_hostile = $true
    Set-Position $npc ($(if ($index -eq 0) {-4.5} else {4.5})) 3 $(if ($index -eq 0) {12.0} else {-12.0})
    # Модель смотрит вдоль -Z. Южного бойца разворачиваем на север.
    $npc.rotation.y = $(if ($index -eq 0) {0.0} else {3.14159265})
    Add-Property $npc "has_faction" $true
    Add-Property $npc "faction_id" $index
    Add-Property $npc "npc_speed" 2.1
    Add-Property $npc "npc_run_speed" 5.4
    Add-Property $npc "npc_run_distance" 3.5
    Add-Property $npc "npc_allow_running" $true
    Add-Property $npc "has_npc_combat" $true
    Add-Property $npc "npc_combat_see_range" 35.0
    Add-Property $npc "npc_combat_field_of_view" 150.0
    Add-Property $npc "npc_combat_close_awareness_range" 2.0
    Add-Property $npc "npc_combat_wander_until_contact" $true
    Add-Property $npc "npc_combat_contact_wander_radius" 16.0
    Add-Property $npc "npc_combat_ranged_attack_range" 24.0
    Add-Property $npc "npc_combat_ranged_aim_delay" 0.30
    Add-Property $npc "npc_combat_lose_target_delay" 7.0
    # Полигон загружается с минимальным профилем. Продвинутые слои включаются
    # по одному во вкладке Combat Lab, чтобы источник ошибки был очевиден.
    Add-Property $npc "has_npc_combat_modules" $true
    Add-Property $npc "npc_module_perception" $true
    Add-Property $npc "npc_module_engagement" $true
    Add-Property $npc "npc_module_pursuit" $true
    Add-Property $npc "npc_module_stationary_fire" $true
    Add-Property $npc "npc_module_search" $false
    Add-Property $npc "npc_module_automatic_tactics" $false
    Add-Property $npc "npc_module_cover_selection" $false
    Add-Property $npc "npc_module_peek_cycle" $false
    Add-Property $npc "npc_module_cover_reposition" $false
    Add-Property $npc "npc_module_dynamic_cover" $false
    Add-Property $npc "npc_module_moving_fire" $false
    Add-Property $npc "npc_module_incoming_fire_reaction" $false
    Add-Property $npc "npc_module_squad_tactics" $false
    Add-Property $npc "has_npc_tactical" $true
    Add-Property $npc "npc_squad_id" 1
    # На полигоне стили закреплены намеренно: один боец проверяет наступательное
    # перестроение, второй — более долгое удержание высокой защиты.
    # Minimal Duel starts from the same neutral behavior for both NPCs. Fixed
    # Assault/Defensive styles can be enabled later as a separate experiment.
    Add-Property $npc "npc_tactical_selected_style" 1
    Add-Property $npc "npc_tactical_aggression" $(if ($index -eq 0) {0.85} else {0.20})
    Add-Property $npc "npc_tactical_decision_interval" 0.40
    Add-Property $npc "npc_tactical_hide_duration" $(if ($index -eq 0) {2.0} else {3.2})
    Add-Property $npc "npc_tactical_expose_duration" $(if ($index -eq 0) {0.85} else {0.65})
    Add-Property $npc "npc_tactical_peek_speed" 4.2
    Add-Property $npc "npc_tactical_moving_burst_size" $(if ($index -eq 0) {3} else {2})
    Add-Property $npc "npc_tactical_moving_burst_pause" $(if ($index -eq 0) {0.60} else {0.75})
    Add-Property $npc "npc_tactical_failed_peeks_before_relocate" $(if ($index -eq 0) {1} else {2})
    Add-Property $npc "npc_tactical_minimum_cover_hold" $(if ($index -eq 0) {3.0} else {6.0})
    Add-Property $npc "npc_tactical_incoming_fire_awareness_range" 24.0
    Add-Property $npc "npc_tactical_flank_weight" $(if ($index -eq 0) {5.0} else {1.5})
    Add-Property $npc "npc_tactical_preferred_range" $(if ($index -eq 0) {9.0} else {13.0})
    # Полигон находится снаружи корабельной атмосферы: здесь проверяем бой,
    # поэтому удушье намеренно отключено только для этих двух тестовых NPC.
    Add-Property $npc "npc_takes_suffocation_damage" $false
    # Параметры присутствуют у врагов и союзников одинаково. На полигоне
    # маршрут выключен, пока автор не создаст Patrol Point с нужным Route ID.
    Add-Property $npc "npc_patrol_enabled" $false
    Add-Property $npc "npc_patrol_route_id" ($index + 1)
    Add-Property $npc "npc_patrol_loop" $true
    Add-Property $npc "npc_patrol_ping_pong" $false
    Add-Property $npc "npc_patrol_allow_running" $false
    Add-Property $npc "npc_patrol_arrival_distance" 0.45
    Add-Property $npc "npc_patrol_default_wait" 1.0
    Add-Property $npc "has_inventory" $true
    Add-Property $npc "inventory_active_slot" 0
    Add-Property $npc "inventory_slots" @($weaponId, 4294967295, 4294967295)
    Add-Property $npc "has_equipped_weapon" $true
    Add-Property $npc "equipped_weapon" $weaponId
    $objects.Add($npc)

    $weapon = Clone-Object $sourcePlayerWeapon
    $weapon.entity_id = $weaponId; $weapon.id = $weaponId
    $weapon.object_name = "NPC Rifle $($index + 1)"
    $weapon.active = $false
    Add-Property $weapon "has_weapon_owner" $true
    Add-Property $weapon "weapon_owner" $npcId
    Add-Property $weapon "pickup_enabled" $false
    Add-Property $weapon "weapon_damage" 8.0
    Add-Property $weapon "weapon_fire_rate" 3.5
    Add-Property $weapon "weapon_range" 35.0
    if ($weapon.weapon_ballistics) {
        # Старые профили могут не иметь полей магазина. Add-Property
        # одинаково работает и для добавления, и для замены.
        Add-Property $weapon.weapon_ballistics "magazine_size" 30
        Add-Property $weapon.weapon_ballistics "ammo_in_magazine" 30
        Add-Property $weapon.weapon_ballistics "reserve_ammo" 150
        Add-Property $weapon.weapon_ballistics "reload_duration" 2.2
        Add-Property $weapon.weapon_ballistics "moving_multiplier" 3.0
    }
    $objects.Add($weapon)
}

function Add-Barrier([int]$id, [string]$name, [float]$x, [float]$z,
    [float]$sx, [float]$sy, [float]$sz, [float]$yaw = 0.0) {
    $objects.Add([pscustomobject]@{
        active=$true; entity_id=$id; id=$id; object_name=$name
        model_path="assets/model/cube.obj"; texture_path="assets/texture/stone.tga"; shader_type=3
        position=[pscustomobject]@{x=$x;y=(2.0+$sy);z=$z}; rotation=[pscustomobject]@{x=0.0;y=$yaw;z=0.0}
        scale=[pscustomobject]@{x=$sx;y=$sy;z=$sz}; has_rigidbody=$true; rb_mass=0.0
        rb_is_dynamic=$false; rb_is_kinematic=$false; rb_shape_type=0; rb_use_manual_size=$false
        rb_shape_size=@(1.0,1.0,1.0); rb_shape_radius=0.5; rb_friction=0.8
        rb_linear_damping=0.0; rb_angular_damping=0.0; rb_enable_ccd=$false
        rb_is_compound=$false; rb_compound_elements=@(); rb_collision_model_path=""
    })
}
function Add-Cover([int]$id, [float]$x, [float]$z, [float]$peekX,
    [float]$peekZ, [float]$height = 1.25, [bool]$crouched = $true,
    [bool]$bothSides = $false) {
    $objects.Add([pscustomobject]@{
        active=$true; entity_id=$id; id=$id; object_name="NPC Cover Point $id"; model_path=""
        position=[pscustomobject]@{x=$x;y=3.0;z=$z}; rotation=[pscustomobject]@{x=0.0;y=0.0;z=0.0}
        scale=[pscustomobject]@{x=1.0;y=1.0;z=1.0}; has_npc_cover_point=$true
        npc_cover_peek_offset=@($peekX,0.0,$peekZ); npc_cover_protection_height=$height
        npc_cover_allow_opposite_peek=$bothSides
        npc_cover_crouched=$crouched; npc_cover_enabled=$true
    })
}

# Арена больше не содержит низких тестовых тумб. Все препятствия выше стоящего
# Kevin и гарантированно разрывают прямую видимость. Симметрия оставляет обоим
# стилям одинаковое число маршрутов, но боковые стены создают разные углы боя.
Add-Barrier 30 "Central High Wall"          0.0   0.0  3.6 1.90 0.70
Add-Barrier 31 "North-West High Wall"      -7.0   6.0  2.3 1.85 0.65
Add-Barrier 32 "North-East High Wall"       7.0   6.0  2.3 1.85 0.65
Add-Barrier 33 "South-West High Wall"      -7.0  -6.0  2.3 1.85 0.65
Add-Barrier 34 "South-East High Wall"       7.0  -6.0  2.3 1.85 0.65
Add-Barrier 35 "West Mid High Wall"       -10.0   0.0  0.70 1.85 2.6
Add-Barrier 36 "East Mid High Wall"        10.0   0.0  0.70 1.85 2.6
Add-Barrier 37 "North Approach High Wall"   0.0  10.0  2.4 1.85 0.65
Add-Barrier 38 "South Approach High Wall"   0.0 -10.0  2.4 1.85 0.65
Add-Barrier 39 "North-West Inner Pillar"   -4.0   2.8  0.70 1.90 2.0
Add-Barrier 40 "North-East Inner Pillar"    4.0   2.8  0.70 1.90 2.0
Add-Barrier 41 "South-West Inner Pillar"   -4.0  -2.8  0.70 1.90 2.0
Add-Barrier 42 "South-East Inner Pillar"    4.0  -2.8  0.70 1.90 2.0

$script:nextCoverId = 100
function Add-HorizontalCoverSet([float]$x, [float]$z, [float]$halfWidth) {
    $inside = [Math]::Max(0.7, $halfWidth - 1.0)
    $peek = 1.25
    foreach ($sideZ in -1.0, 1.0) {
        Add-Cover $script:nextCoverId ($x-$inside) ($z+$sideZ*1.30) (-$peek) 0.0 1.65 $false; ++$script:nextCoverId
        Add-Cover $script:nextCoverId ($x+$inside) ($z+$sideZ*1.30) ( $peek) 0.0 1.65 $false; ++$script:nextCoverId
    }
}
function Add-VerticalCoverSet([float]$x, [float]$z, [float]$halfDepth) {
    $inside = [Math]::Max(0.7, $halfDepth - 1.0)
    $peek = 1.25
    foreach ($sideX in -1.0, 1.0) {
        Add-Cover $script:nextCoverId ($x+$sideX*1.30) ($z-$inside) 0.0 (-$peek) 1.65 $false; ++$script:nextCoverId
        Add-Cover $script:nextCoverId ($x+$sideX*1.30) ($z+$inside) 0.0 ( $peek) 1.65 $false; ++$script:nextCoverId
    }
}

# Четыре точки на каждую стену: два защищённых места и два независимых края.
# NPC после исчерпания peek-бюджета получает реальный выбор нового угла, а не
# возвращается к единственному ближайшему маркеру.
Add-HorizontalCoverSet  0.0   0.0 3.6
Add-HorizontalCoverSet -7.0   6.0 2.3
Add-HorizontalCoverSet  7.0   6.0 2.3
Add-HorizontalCoverSet -7.0  -6.0 2.3
Add-HorizontalCoverSet  7.0  -6.0 2.3
Add-HorizontalCoverSet  0.0  10.0 2.4
Add-HorizontalCoverSet  0.0 -10.0 2.4
Add-VerticalCoverSet  -10.0   0.0 2.6
Add-VerticalCoverSet   10.0   0.0 2.6
Add-VerticalCoverSet   -4.0   2.8 2.0
Add-VerticalCoverSet    4.0   2.8 2.0
Add-VerticalCoverSet   -4.0  -2.8 2.0
Add-VerticalCoverSet    4.0  -2.8 2.0

$result = [ordered]@{ scene_name="Combat Training Ground"; version=2; objects=$objects }
$result | ConvertTo-Json -Depth 100 | Set-Content -LiteralPath $Output -Encoding utf8
Write-Host "Built $Output with $($objects.Count) entities."
