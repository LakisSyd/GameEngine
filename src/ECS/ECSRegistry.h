#pragma once
// Добавить перегрузку ForEach для двух компонент

#include <typeindex>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include "ComponentManager.h"

class ECSRegistry {
public:
    Entity CreateEntity()
    {
        if (nextEntity == INVALID_ENTITY)
            throw std::overflow_error("ECS entity id space exhausted");
        const Entity entity = nextEntity++;
        m_liveEntities.insert(entity);
        return entity;
    }

    void DestroyEntity(Entity e) {
        m_liveEntities.erase(e);
        for (auto& [ti, pool] : m_pools)
            pool->Remove(e);
    }

    bool IsAlive(Entity e) const { return m_liveEntities.contains(e); }
    size_t GetEntityCount() const { return m_liveEntities.size(); }

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
        auto* pool1 = FindPool<T1>();
        auto* pool2 = FindPool<T2>();
        if (!pool1 || !pool2) return;
        for (auto& [entity, comp1] : *pool1)
        {
            if (pool2->Has(entity))
            {
                auto& comp2 = *pool2->Get(entity);
                fn(entity, comp1, comp2);
            }
        }
    }

    template<typename T>
    T* GetComponent(Entity e) {
        auto* pool = FindPool<T>();
        return pool ? pool->Get(e) : nullptr;
    }

    template<typename T>
    bool HasComponent(Entity e) const {
        const auto* pool = FindPool<T>();
        return pool && pool->Has(e);
    }

    template<typename T>
    void RemoveComponent(Entity e) {
        if (auto* pool = FindPool<T>())
            pool->Remove(e);
    }

    template<typename T>
    ComponentManager<T>& GetComponents() {
        return GetOrCreatePool<T>();
    }
    template<typename T, typename Fn>
    void ForEach(Fn&& fn)
    {
        auto* pool = FindPool<T>();
        if (!pool) return;
        for (auto& [entity, component] : *pool)
        {
            fn(entity, component);
        }
    }
    
private:
    Entity nextEntity = 0;
    std::unordered_set<Entity> m_liveEntities;
    std::unordered_map<std::type_index, std::unique_ptr<IComponentPool>> m_pools;

    template<typename T>
    ComponentManager<T>* FindPool() {
        const auto it = m_pools.find(std::type_index(typeid(T)));
        return it == m_pools.end()
            ? nullptr
            : static_cast<ComponentManager<T>*>(it->second.get());
    }

    template<typename T>
    const ComponentManager<T>* FindPool() const {
        const auto it = m_pools.find(std::type_index(typeid(T)));
        return it == m_pools.end()
            ? nullptr
            : static_cast<const ComponentManager<T>*>(it->second.get());
    }

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
