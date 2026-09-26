#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>

struct StreamPacket {
    std::vector<uint8_t> data;
    uint64_t pts = 0;
    bool isKeyFrame = false;
};

template<typename T>
class RingQueue {
public:
    explicit RingQueue(size_t maxSize) : m_maxSize(maxSize) {}

    bool push(const T& item) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_notFull.wait(lock, [this] { return m_queue.size() < m_maxSize || m_stop; });
        if (m_stop) return false;
        m_queue.push(item);
        m_notEmpty.notify_one();
        return true;
    }

    bool pop(T& item) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_notEmpty.wait(lock, [this] { return !m_queue.empty() || m_stop; });
        if (m_queue.empty()) return false;
        item = std::move(m_queue.front());
        m_queue.pop();
        m_notFull.notify_one();
        return true;
    }

    void stop() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stop = true;
        m_notEmpty.notify_all();
        m_notFull.notify_all();
    }

private:
    std::queue<T> m_queue;
    std::mutex m_mutex;
    std::condition_variable m_notEmpty;
    std::condition_variable m_notFull;
    size_t m_maxSize;
    bool m_stop = false;
};
