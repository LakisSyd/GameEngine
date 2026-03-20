#pragma once

#include <typeindex>
#include <unordered_map>
#include <memory>
#include "ECSTypes.h"

// Базовый класс для стирания типа
struct IComponentPool { virtual ~IComponentPool() = default; 

virtual void Remove(Entity e) = 0;
};

