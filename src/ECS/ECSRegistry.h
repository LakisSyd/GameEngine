#pragma once
// Добавить перегрузку ForEach для двух компонент

#include <typeindex>
#include <unordered_map>
#include <memory>
#include "ComponentManager.h"

class ECSRegistry {
public:
    Entity CreateEntity() { return nextEntity++; }

    void DestroyEntity(Entity e) {
        for (auto& [ti, pool] : m_pools)
            pool->Remove(e);
    }

    // copy-версия — только для копируемых типов
    template<typename T, typename = std::enable_if_t<std::is_copy_constructible_v<T>>>
    void AddComponent(Entity e, const T& comp) {
        GetOrCreatePool<T>().Add(e, comp);
    }

    // move-версия — для всех типов (в т.ч. некопируемых, как RigidBodyComponent)
    template<typename T>
    void AddComponent(Entity e, T&& comp) {
        GetOrCreatePool<T>().Add(e, std::forward<T>(comp));
    }

    template<typename T1, typename T2, typename Fn>
    void ForEach(Fn&& fn)
    {
        auto& pool1 = GetComponents<T1>();
        auto& pool2 = GetComponents<T2>();
        for (auto& [entity, comp1] : pool1)
        {
            if (pool2.Has(entity))
            {
                auto& comp2 = *pool2.Get(entity);
                fn(entity, comp1, comp2);
            }
        }
    }

    template<typename T>
    T* GetComponent(Entity e) {
        return GetOrCreatePool<T>().Get(e);
    }

    template<typename T>
    bool HasComponent(Entity e) {
        return GetOrCreatePool<T>().Has(e);
    }

    template<typename T>
    void RemoveComponent(Entity e) {
        GetOrCreatePool<T>().Remove(e);
    }

    template<typename T>
    ComponentManager<T>& GetComponents() {
        return GetOrCreatePool<T>();
    }
    template<typename T, typename Fn>
    void ForEach(Fn&& fn)
    {
        auto& pool = GetComponents<T>();
        for (auto& [entity, component] : pool)
        {
            fn(entity, component);
        }
    }
    
private:
    Entity nextEntity = 0;
    std::unordered_map<std::type_index, std::unique_ptr<IComponentPool>> m_pools;

    template<typename T>
    ComponentManager<T>& GetOrCreatePool() {
        std::type_index ti(typeid(T));
        auto it = m_pools.find(ti);
        if (it == m_pools.end()) {
            auto pool = std::make_unique<ComponentManager<T>>();
            auto* ptr = pool.get();
            m_pools[ti] = std::move(pool);
            return *ptr;
        }
        return *static_cast<ComponentManager<T>*>(it->second.get());
    }
};