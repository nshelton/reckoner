#pragma once

#include <array>
#include <cstddef>

/// Generic ring buffer for fixed-capacity FIFO storage
template<typename T, size_t N>
class RingBuffer {
public:
    void push(const T& item) {
        m_buffer[m_index] = item;
        m_index = (m_index + 1) % N;
        if (m_count < N) m_count++;
    }

    void push(T&& item) {
        m_buffer[m_index] = std::move(item);
        m_index = (m_index + 1) % N;
        if (m_count < N) m_count++;
    }

    const T& operator[](size_t i) const {
        return m_buffer[i];
    }

    size_t size() const { return m_count; }
    size_t capacity() const { return N; }
    size_t writeIndex() const { return m_index; }
    bool empty() const { return m_count == 0; }
    bool full() const { return m_count == N; }

    void clear() {
        m_index = 0;
        m_count = 0;
    }

    // Iterator support for range-based for loops
    auto begin() const { return m_buffer.begin(); }
    auto end() const { return m_buffer.begin() + m_count; }

private:
    std::array<T, N> m_buffer{};
    size_t m_index = 0;
    size_t m_count = 0;
};

/// Ring buffer for tracking fetch latencies with statistics
template<size_t N>
class LatencyRingBuffer {
public:
    void push(float latency_ms) {
        m_buffer[m_index] = latency_ms;
        m_index = (m_index + 1) % N;
        if (m_count < N) m_count++;
    }

    float average() const {
        if (m_count == 0) return 0.0f;
        float sum = 0.0f;
        for (size_t i = 0; i < m_count; ++i) {
            sum += m_buffer[i];
        }
        return sum / m_count;
    }

    float min() const {
        if (m_count == 0) return 0.0f;
        float m = m_buffer[0];
        for (size_t i = 1; i < m_count; ++i) {
            if (m_buffer[i] < m) m = m_buffer[i];
        }
        return m;
    }

    float max() const {
        if (m_count == 0) return 0.0f;
        float m = m_buffer[0];
        for (size_t i = 1; i < m_count; ++i) {
            if (m_buffer[i] > m) m = m_buffer[i];
        }
        return m;
    }

    size_t count() const { return m_count; }

private:
    std::array<float, N> m_buffer{};
    size_t m_index = 0;
    size_t m_count = 0;
};
