    #pragma once
    #include <cstdint>
    #include <unordered_map>
    #include <unordered_set>
    #include "Components.h"
    #include "IComponentPull.h"

    // ============================================================================
    // ComponentManager<T> — хранилище компонентов одного типа
    //
    // Внутри — unordered_map: Entity ? T
    // Каждый тип компонента имеет свой отдельный пул (pool).
    // Доступ O(1) по Entity.
    // ============================================================================
    template<typename T>
    class ComponentManager : public IComponentPool
    {
    public:
        // Проверить, есть ли компонент у сущности
        bool Has(Entity e) const { return m_components.find(e) != m_components.end(); }

        // Получить указатель на компонент (nullptr если нет)
        T* Get(Entity e)
        {
            auto it = m_components.find(e);
            return it != m_components.end() ? &it->second : nullptr;
        }

        // Move-версия — работает для ВСЕХ типов (в т.ч. некопируемых)
        void Add(Entity e, T&& comp)
        {
            m_components.erase(e);                          // удаляем старый (если есть)
            m_components.emplace(e, std::move(comp));       // вставляем через move
        }

        // Copy-версия — только для типов с copy-constructor
        template<typename U = T, typename = std::enable_if_t<std::is_copy_constructible_v<U>>>
        void Add(Entity e, const U& comp)
        {
            m_components.erase(e);
            m_components.emplace(e, comp);                  // copy — только для copyable T
        }

        // Удалить компонент (вызывается из ECSRegistry::DestroyEntity)
        void Remove(Entity e) override { m_components.erase(e); }

        // Итерация по всем компонентам: for (auto& [entity, comp] : pool)
        auto begin() { return m_components.begin(); }
        auto end()   { return m_components.end(); }

        // Количество компонентов в пуле
        size_t GetCount() const { return m_components.size(); }

    private:
        std::unordered_map<Entity, T> m_components;
    };

    // ============================================================================
    // ComponentManager<ActiveComponent> — специализация для тег-компонента
    //
    // ActiveComponent не содержит данных — хранить пустые структуры в unordered_map
    // расточительно. Используем unordered_set<Entity>: только факт присутствия.
    //
    // Интерфейс намеренно совпадает с обычным ComponentManager<T>,
    // чтобы ECSRegistry не знал о разнице.
    //
    // Неделя 2: заменяет bool IsActive в GameObject
    // ============================================================================
    template<>
    class ComponentManager<ActiveComponent> : public IComponentPool
    {
    public:
        // Проверить активность сущности
        bool Has(Entity e) const { return m_entities.count(e) > 0; }

        // Возвращаем указатель на статичный dummy — нужно для совместимости с GetComponent<T>()
        // Реальных данных нет, используй только Has() для проверки активности
        ActiveComponent* Get(Entity e) { return Has(e) ? &m_dummy : nullptr; }

        // Добавить тег активности
        void Add(Entity e, const ActiveComponent&) { m_entities.insert(e); }

        // Удалить тег — сущность становится неактивной
        void Remove(Entity e) override { m_entities.erase(e); }

        auto begin() { return m_entities.begin(); }
        auto end()   { return m_entities.end(); }

        // Количество активных сущностей
        size_t GetCount() const { return m_entities.size(); }

    private:
        std::unordered_set<Entity> m_entities;

        // Единственный экземпляр — нужен только чтобы вернуть ненулевой указатель из Get()
        ActiveComponent m_dummy{};
    };