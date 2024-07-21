#include <cstddef>
#include <vector>
#include <memory>

class MemoryPool {
public:
    explicit MemoryPool(size_t chunkSize = 1024)
        : m_chunkSize(chunkSize), m_allocated(0) {}

    void* allocate(size_t size) {
        if (size > m_chunkSize) {
            return ::operator new(size);
        }

        if (m_allocated + size > m_chunkSize) {
            m_blocks.emplace_back(::operator new(m_chunkSize));
            m_allocated = 0;
        }

        void* ptr = static_cast<char*>(m_blocks.back().get()) + m_allocated;
        m_allocated += size;
        return ptr;
    }

    void deallocate(void* ptr, size_t size) {
        // 内存池不实际释放内存，因为整个块将在析构时释放
    }

private:
    size_t m_chunkSize;
    size_t m_allocated;
    std::vector<std::unique_ptr<void, decltype(&::operator delete)>> m_blocks{
        std::unique_ptr<void, decltype(&::operator delete)>(nullptr, &::operator delete)
    };
};


template <typename T>
class PoolAllocator {
public:
    using value_type = T;

    explicit PoolAllocator(MemoryPool& pool) : m_pool(pool) {}

    template <typename U>
    PoolAllocator(const PoolAllocator<U>& other) noexcept : m_pool(other.m_pool) {}

    T* allocate(size_t n) {
        return static_cast<T*>(m_pool.allocate(n * sizeof(T)));
    }

    void deallocate(T* p, size_t n) {
        m_pool.deallocate(p, n * sizeof(T));
    }

    template <typename U>
    bool operator==(const PoolAllocator<U>& other) const noexcept {
        return &m_pool == &other.m_pool;
    }

    template <typename U>
    bool operator!=(const PoolAllocator<U>& other) const noexcept {
        return !(*this == other);
    }

private:
    MemoryPool& m_pool;

    template <typename U> friend class PoolAllocator;
};