#pragma once

#include <queue>

namespace lcf {
    template <std::integral Id> 
    class SequentialIdAllocator
    {
        using AvailableIdHeap = std::priority_queue<Id, std::vector<Id>, std::greater<Id>>;
    public:
        using id_type = Id;
    public:
        SequentialIdAllocator() = default;
        ~SequentialIdAllocator() noexcept = default;
        SequentialIdAllocator(const SequentialIdAllocator &) = default;
        SequentialIdAllocator & operator=(const SequentialIdAllocator &) = default;
        SequentialIdAllocator(SequentialIdAllocator &&) noexcept = default;
        SequentialIdAllocator & operator=(SequentialIdAllocator &&) noexcept = default;
    public:
        Id allocate() noexcept
        {
            Id id = 0;
            if (m_available_ids.empty()) {
                id = m_next_id++;
            } else {
                id = m_available_ids.top();
                m_available_ids.pop();
            }
            return id;
        }
        void deallocate(Id id) noexcept
        {
            m_available_ids.emplace(id);
        }
    private:
        Id m_next_id = 0;
        AvailableIdHeap m_available_ids;
    };
}