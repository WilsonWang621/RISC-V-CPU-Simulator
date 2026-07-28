#pragma once
#include<array>
#include<cassert>
/*
- Fetch Queue
- 内存请求队列
- 内存响应队列
- 一般事件队列
*/
template <typename T, std::size_t Capacity>
class CircularQueue{
    static_assert(Capacity > 0U, "CircularQueue capacity must be positive");

    std::array<T, Capacity> storage_{};
    int head = 0;
    int tail = 0;
    std::size_t size_ = 0U;

public:
    bool empty(){
        return size_ == 0;
    }

    bool full(){
        return size_ == Capacity;
    }

    std::size_t size(){
        return size_;
    }

    std::size_t capacity(){
        return Capacity;
    }

    T front(){
        assert(!empty());
        return storage_[head];
    }

    T back(){
        assert(!empty());
        return storage_[(tail + Capacity - 1) % Capacity];
    }


    bool push(T v){
        if(full()){
            return false;
        }
        storage_[tail] = v;
        tail = (tail + 1) % Capacity;
        size_++;
        return true;
    }

    bool pop(){
        if(empty()) return false;
        head = (head + 1) % Capacity;
        size_--;
        return true;
    }

    void clear(){
        head = tail = 0;
        size_ = 0U;
    }
};