#pragma once
#include <algorithm>
#include <cmath>
#include <DirectXMath.h>
#include "ECSTypes.h"

// Симметричная реакция пары: совпавшие позиции тоже дают противоположные
// направления. Возвращается намерение движения, не изменение Transform.
inline DirectX::XMFLOAT3 NpcPairSeparation(Entity self, Entity other,
    float x, float z, float spacing)
{
    // x/z — горизонтальный вектор ОТ второго NPC к текущему. Вертикаль
    // игнорируется, чтобы персонажи на разных этажах не расталкивали друг друга.
    float distance = std::hypot(x, z);
    // Уже достаточно далеко — корректировать направление не требуется.
    if (distance >= spacing) return {};
    // Чем сильнее пересечение личного пространства, тем сильнее отталкивание.
    const float weight = (std::max)(0.3f, spacing-distance) * 2.0f;
    // При полном совпадении позиций направление выбирается стабильно по Entity:
    // один идёт влево, второй вправо, а не получают одинаковый случайный вектор.
    if (distance < 0.001f)
        return {self < other ? -weight : weight, 0.0f, 0.0f};
    return {x/distance*weight, 0.0f, z/distance*weight};
}
