#pragma once

#include <cstdint>
#include <utility>
#include <vector>

namespace gatorevac {

class IndexedMinHeap {
public:
    static constexpr uint32_t kAbsent = 0xFFFFFFFFu;

    explicit IndexedMinHeap(uint32_t capacity) : key_(capacity), pos_(capacity, kAbsent) {}

    bool empty() const { return heap_.empty(); }
    bool contains(uint32_t id) const { return pos_[id] != kAbsent; }

    void push(uint32_t id, double key) {
        key_[id] = key;
        pos_[id] = static_cast<uint32_t>(heap_.size());
        heap_.push_back(id);
        siftUp(pos_[id]);
    }

    void decreaseKey(uint32_t id, double key) {
        key_[id] = key;
        siftUp(pos_[id]);
    }

    void pushOrDecrease(uint32_t id, double key) {
        if (contains(id))
            decreaseKey(id, key);
        else
            push(id, key);
    }

    uint32_t pop() {
        const uint32_t top = heap_.front();
        swapNodes(0, static_cast<uint32_t>(heap_.size() - 1));
        heap_.pop_back();
        pos_[top] = kAbsent;
        siftDown(0);
        return top;
    }

    void clear() {
        for (uint32_t id : heap_)
            pos_[id] = kAbsent;
        heap_.clear();
    }

private:
    void swapNodes(uint32_t a, uint32_t b) {
        std::swap(heap_[a], heap_[b]);
        pos_[heap_[a]] = a;
        pos_[heap_[b]] = b;
    }

    void siftUp(uint32_t i) {
        while (i > 0) {
            const uint32_t parent = (i - 1) / 2;
            if (key_[heap_[i]] < key_[heap_[parent]]) {
                swapNodes(i, parent);
                i = parent;
            } else {
                break;
            }
        }
    }

    void siftDown(uint32_t i) {
        const uint32_t n = static_cast<uint32_t>(heap_.size());
        while (true) {
            const uint32_t left = 2 * i + 1;
            const uint32_t right = left + 1;
            if (left >= n) break;

            uint32_t smaller = left;
            if (right < n && key_[heap_[right]] < key_[heap_[left]]) {
                smaller = right;
            }
            if (key_[heap_[smaller]] < key_[heap_[i]]) {
                swapNodes(i, smaller);
                i = smaller;
            } else {
                break;
            }
        }
    }

    std::vector<uint32_t> heap_;
    std::vector<double> key_;
    // pos_[id] is the node's position in the heap.
    std::vector<uint32_t> pos_;
};

}
