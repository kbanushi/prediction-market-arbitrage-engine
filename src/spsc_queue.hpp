#pragma once

#include <atomic>
#include <cstddef>
#include <new>
#include <type_traits>

#ifdef __cpp_lib_hardwareinterface_size
    constexpr size_t CACHE_LINE_SIZE = std::hardware_destructive_interface_size;
#else
    constexpr size_t CACHE_LINE_SIZE = 64;
#endif

template<typename T, size_t Capacity>
class SPSCQueue{
    static_assert((Capacity != 0) && ((Capacity & (Capacity - 1)) == 0), "Capacity must be power of 2");

    private:
        alignas(CACHE_LINE_SIZE) std::atomic<size_t> write_index{};
        alignas(CACHE_LINE_SIZE) std::atomic<size_t> read_index{};

        T buffer[Capacity];

    public:
        bool push(const T& item){
            const size_t current_write = write_index.load(std::memory_order_relaxed);
            const size_t next_write = current_write + 1;

            // NOTE: Indicies are never reset, thus the difference between write and read is the current capacity.
            if (next_write - read_index.load(std::memory_order_acquire) > Capacity){ 
                return false; // Buffer is full
            }
            
            buffer[current_write & (Capacity - 1)] = item;
            
            write_index.store(next_write, std::memory_order_release);
            return true;
        }

        bool pop(T& out_item){
            const size_t current_read = read_index.load(std::memory_order_relaxed);

            if (current_read == write_index.load(std::memory_order_acquire)){
                return false; // Buffer is empty or the writer hasn't finished writing
            }

            out_item = buffer[current_read & (Capacity - 1)];

            read_index.store(current_read + 1, std::memory_order_release);
            return true;
        }

};
