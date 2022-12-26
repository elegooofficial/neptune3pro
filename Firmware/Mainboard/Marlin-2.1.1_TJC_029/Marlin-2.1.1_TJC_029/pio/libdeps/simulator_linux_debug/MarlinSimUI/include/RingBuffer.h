#pragma once

#include <cstdint>
#include <mutex>
#include <atomic>

template <typename T, std::size_t S> class RingBuffer {
public:
  RingBuffer() {index_read = index_write = 0;}

  std::size_t available() const {return mask(index_write - index_read);}
  std::size_t free() const {return size() - available();}
  bool empty() const {return index_read == index_write;}
  bool full() const {return next(index_write) == index_read;}
  void clear() {index_read = index_write = 0;}

  bool peek(T *const value) const {
    if (value == nullptr || empty()) return false;
    *value = buffer[index_read];
    return true;
  }

  inline std::size_t read(T* dst, std::size_t length) {
    std::scoped_lock lock(m);
    length = std::min(length, available());
    const std::size_t length1 = std::min(length, buffer_size - index_read);
    memcpy(dst, (char*)buffer + index_read, length1);
    memcpy(dst + length1, (char*)buffer, length - length1);
    index_read = mask(index_read + length);
    return length;
  }

  inline std::size_t write(T* src, std::size_t length) {
    std::scoped_lock lock(m);
    length = std::min(length, free());
    const std::size_t length1 = std::min(length, buffer_size - index_write);
    memcpy((char*)buffer + index_write, src, length1);
    memcpy((char*)buffer, src + length1, length - length1);
    index_write = mask(index_write + length);
    return length;
  }

  std::size_t read(T *const value) {
    std::scoped_lock lock(m);
    if (value == nullptr || empty()) return 0;
    *value = buffer[index_read];
    index_read = next(index_read);
    return 1;
  }

  std::size_t write(const T value) {
    std::scoped_lock lock(m);
    std::size_t next_head = next(index_write);
    if (next_head == index_read) return 0;     // buffer full
    buffer[index_write] = value;
    index_write = next_head;
    return 1;
  }

  constexpr std::size_t size() const {
    return buffer_size - 1;
  }

private:
  inline std::size_t mask(std::size_t val) const {
    return val & buffer_mask;
  }

  inline std::size_t next(std::size_t val) const {
    return mask(val + 1);
  }

  std::mutex m;
  static const std::size_t buffer_size = S;
  static const std::size_t buffer_mask = buffer_size - 1;
  volatile T buffer[buffer_size];
  std::atomic_size_t index_write;
  std::atomic_size_t index_read;
};
