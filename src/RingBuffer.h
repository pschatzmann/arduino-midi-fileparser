#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#ifndef PEEK_MAX
#define PEEK_MAX 80
#endif

namespace midi {

/**
 * @brief RingBuffer that supports extensive peek operations to 
 * access and compare the next values
 * @author Phil Schatzmann
 */
class RingBuffer {
public:
  RingBuffer() = default;

  RingBuffer(RingBuffer &t){
    _aucBuffer = nullptr;
    reset();
  }

  RingBuffer(int32_t size) {
    resize(size);
    reset();
  }

  ~RingBuffer() { if (_aucBuffer!=nullptr)delete[] _aucBuffer; }

  uint8_t read() {
    if (isEmpty())
      return -1;

    uint8_t value = _aucBuffer[_iTail];
    _iTail = nextIndex(_iTail);
    _numElems--;

    return value;
  }

  // checks if the buffer is full
  bool isFull() { return available() == max_size; }

  bool isEmpty() { return available() == 0; }

  // write add an entry to the buffer
  bool write(uint8_t data) {
    bool result = false;
    if (!isFull()) {
      _aucBuffer[_iHead] = data;
      _iHead = nextIndex(_iHead);
      _numElems++;
      result = true;
    }
    return result;
  }

  int32_t write(const uint8_t *data, size_t len) {
    for (int32_t j = 0; j < (int32_t)len; j++) {
      if (!write(data[j])) {
        return j;
      }
    }
    return len;
  }

  // clears the buffer
  void reset() {
    _iHead = 0;
    _iTail = 0;
    _numElems = 0;
  }

  // provides the number of entries that are available to read
  int32_t available() { return _numElems; }

  // provides the number of entries that are available to write
  int32_t availableForWrite() { return (max_size - _numElems); }

  void resize(int32_t len) {
    // if no change: we can leave
    if (this->max_size==len && _aucBuffer!=nullptr){
      return;
    }
    // (re)allocate buffer
    if (_aucBuffer != nullptr) {
      delete[] _aucBuffer;
    }
    this->max_size = len;
    if (len > 0) {
      _aucBuffer = new uint8_t[max_size];
      assert(_aucBuffer != nullptr);
    }
    reset();
  }

  /// Returns the maximum capacity of the buffer
  int32_t size() { return max_size; }

  // peeks the actual entry from the buffer
  int peek() {
    if (isEmpty())
      return -1;

    return _aucBuffer[_iTail];
  }

  /// @brief peeks the idx next entry from the buffer
  int peek(int32_t idx) {
    if (idx < 0 || idx >= available())
      return -1;
    int32_t tmp_tail = (_iTail + idx) % max_size;
    return _aucBuffer[tmp_tail];
  }

  /// @brief Copies len bytes starting at idx into dest (without consuming
  /// them). Returns the number of bytes copied, which is limited by the
  /// available data.
  int32_t peekInto(int32_t idx, uint8_t *dest, int32_t len) {
    int32_t n = 0;
    for (; n < len && idx + n < available(); n++) {
      dest[n] = peek(idx + n);
    }
    return n;
  }

  /// @brief returns a temporary copy of the requested bytes. The result is
  /// only valid until the next call of peekStr()!
  uint8_t *peekStr(int32_t idx, int32_t len) {
    if (len <= 0) return nullptr;
    static uint8_t tmp_copy[PEEK_MAX];
    assert(len <= PEEK_MAX);
    assert(idx + len <= available());
    memset(tmp_copy, 0, PEEK_MAX);
    peekInto(idx, tmp_copy, len);
    return tmp_copy;
  }

  /// @brief alternaive syntax to peek(idx)
  uint8_t operator[](int32_t idx) {
    int result = peek(idx);
    assert(result!=-1);
    return result;
  }

  /// @brief Compares the string with the current peek values
  bool equals(const char *str) {
    int32_t len = strlen(str);
    if (len > available()) return false;
    return strncmp(str, (char*)peekStr(0, len), len) == 0;
  }

  /// @brief Removes the next n characters from the ringbuffer
  void consume(int32_t offset) {
    assert(available() >= offset);
    for (int32_t j = 0; j < offset; j++) {
      read();
    }
  }

protected:
  uint8_t *_aucBuffer = nullptr;
  int32_t _iHead = 0;
  int32_t _iTail = 0;
  int32_t _numElems = 0;
  int32_t max_size = 0;

  int32_t nextIndex(int32_t index) { return (uint32_t)(index + 1) % max_size; }
};

} // namespace