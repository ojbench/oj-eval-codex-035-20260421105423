#ifndef SIMPLE_STRING_SIMPLESTRING_HPP
#define SIMPLE_STRING_SIMPLESTRING_HPP

#include <stdexcept>
#include <cstring>

class MyString {
private:
    union {
        char* heap_ptr;
        char small_buffer[16];
    };
    size_t len_ = 0;
    size_t cap_ = 15; // capacity excluding null terminator; 15 for SSO
    bool is_sso_ = true;

    char* data_ptr() { return is_sso_ ? small_buffer : heap_ptr; }
    const char* data_ptr() const { return is_sso_ ? small_buffer : heap_ptr; }
    static size_t grow_capacity(size_t cur, size_t need) {
        size_t n = cur ? cur : 1;
        while (n < need) {
            n = n * 2;
        }
        return n;
    }
    void ensure_capacity(size_t new_cap) {
        if (new_cap <= cap_) return;
        if (new_cap <= 15) {
            // keep SSO capacity as 15
            cap_ = 15;
            return;
        }
        // move to or grow heap
        size_t target = new_cap;
        if (target < cap_) target = cap_;
        // choose growth policy
        target = grow_capacity(cap_ > 15 ? cap_ : 16, new_cap);
        char* new_mem = new char[target + 1];
        // copy existing data
        std::memcpy(new_mem, data_ptr(), len_);
        new_mem[len_] = 0;
        if (!is_sso_) {
            delete[] heap_ptr;
        }
        heap_ptr = new_mem;
        is_sso_ = false;
        cap_ = target;
    }
    void move_to_sso_if_possible() {
        if (len_ <= 15 && !is_sso_) {
            char tmp[16];
            std::memcpy(tmp, heap_ptr, len_);
            tmp[len_] = 0;
            delete[] heap_ptr;
            std::memcpy(small_buffer, tmp, len_ + 1);
            is_sso_ = true;
            cap_ = 15;
        }
    }

public:
    MyString() {
        small_buffer[0] = 0;
        len_ = 0;
        cap_ = 15;
        is_sso_ = true;
    }

    MyString(const char* s) {
        if (!s) {
            // treat null as empty
            small_buffer[0] = 0;
            len_ = 0; cap_ = 15; is_sso_ = true;
            return;
        }
        size_t n = std::strlen(s);
        if (n <= 15) {
            std::memcpy(small_buffer, s, n + 1);
            len_ = n; cap_ = 15; is_sso_ = true;
        } else {
            size_t target = grow_capacity(16, n);
            heap_ptr = new char[target + 1];
            std::memcpy(heap_ptr, s, n + 1);
            len_ = n; cap_ = target; is_sso_ = false;
        }
    }

    MyString(const MyString& other) {
        len_ = other.len_;
        if (other.is_sso_) {
            std::memcpy(small_buffer, other.small_buffer, len_ + 1);
            cap_ = 15; is_sso_ = true;
        } else {
            cap_ = other.cap_; is_sso_ = false;
            heap_ptr = new char[cap_ + 1];
            std::memcpy(heap_ptr, other.heap_ptr, len_ + 1);
        }
    }

    MyString(MyString&& other) noexcept {
        len_ = other.len_;
        cap_ = other.cap_;
        is_sso_ = other.is_sso_;
        if (other.is_sso_) {
            std::memcpy(small_buffer, other.small_buffer, len_ + 1);
        } else {
            heap_ptr = other.heap_ptr;
            other.heap_ptr = nullptr;
        }
        other.len_ = 0;
        other.cap_ = 15;
        other.is_sso_ = true;
        if (other.is_sso_) other.small_buffer[0] = 0;
    }

    MyString& operator=(MyString&& other) noexcept {
        if (this == &other) return *this;
        if (!is_sso_ && heap_ptr) {
            delete[] heap_ptr;
        }
        len_ = other.len_;
        cap_ = other.cap_;
        is_sso_ = other.is_sso_;
        if (other.is_sso_) {
            std::memcpy(small_buffer, other.small_buffer, len_ + 1);
        } else {
            heap_ptr = other.heap_ptr;
            other.heap_ptr = nullptr;
        }
        other.len_ = 0;
        other.cap_ = 15;
        other.is_sso_ = true;
        if (other.is_sso_) other.small_buffer[0] = 0;
        return *this;
    }

    MyString& operator=(const MyString& other) {
        if (this == &other) return *this;
        if (!other.is_sso_) {
            ensure_capacity(other.len_ > 15 ? other.len_ : 16);
            if (is_sso_ && other.len_ > 15) {
                is_sso_ = false;
            }
            if (!is_sso_ && cap_ < other.len_) {
                ensure_capacity(other.len_);
            }
            if (other.len_ > 15 && is_sso_) {
                // move to heap
                ensure_capacity(other.len_);
            }
        }
        if (other.is_sso_) {
            // switch to SSO
            if (!is_sso_) {
                delete[] heap_ptr;
                is_sso_ = true;
                cap_ = 15;
            }
            std::memcpy(small_buffer, other.small_buffer, other.len_ + 1);
            len_ = other.len_;
        } else {
            ensure_capacity(other.len_);
            std::memcpy(data_ptr(), other.data_ptr(), other.len_ + 1);
            len_ = other.len_;
        }
        return *this;
    }

    ~MyString() {
        if (!is_sso_ && heap_ptr) {
            delete[] heap_ptr;
            heap_ptr = nullptr;
        }
    }

    const char* c_str() const {
        return data_ptr();
    }

    size_t size() const {
        return len_;
    }

    size_t capacity() const {
        return is_sso_ ? 15 : cap_;
    }

    void reserve(size_t new_capacity) {
        if (new_capacity <= capacity()) return;
        if (new_capacity <= 15) {
            // no-op; SSO capacity is fixed at 15
            return;
        }
        ensure_capacity(new_capacity);
    }

    void resize(size_t new_size) {
        if (new_size <= len_) {
            len_ = new_size;
            data_ptr()[len_] = 0;
            move_to_sso_if_possible();
            return;
        }
        // expand
        if (new_size <= 15) {
            // stay or move to SSO
            if (!is_sso_) {
                // move to SSO
                // free and copy
                char tmp[16];
                std::memcpy(tmp, heap_ptr, len_);
                delete[] heap_ptr;
                std::memcpy(small_buffer, tmp, len_);
                is_sso_ = true;
                cap_ = 15;
            }
            // fill with \0
            std::memset(small_buffer + len_, 0, new_size - len_);
            len_ = new_size;
            small_buffer[len_] = 0;
        } else {
            ensure_capacity(new_size);
            char* p = data_ptr();
            std::memset(p + len_, 0, new_size - len_);
            len_ = new_size;
            p[len_] = 0;
        }
    }

    char& operator[](size_t index) {
        // No bounds check by operator[]
        return const_cast<char&>(static_cast<const MyString&>(*this).at(index));
    }

    MyString operator+(const MyString& rhs) const {
        size_t new_len = len_ + rhs.len_;
        MyString result;
        if (new_len <= 15) {
            // result in SSO
            result.is_sso_ = true;
            result.cap_ = 15;
            std::memcpy(result.small_buffer, data_ptr(), len_);
            std::memcpy(result.small_buffer + len_, rhs.data_ptr(), rhs.len_);
            result.len_ = new_len;
            result.small_buffer[new_len] = 0;
        } else {
            size_t target = grow_capacity(16, new_len);
            result.is_sso_ = false;
            result.cap_ = target;
            result.heap_ptr = new char[target + 1];
            std::memcpy(result.heap_ptr, data_ptr(), len_);
            std::memcpy(result.heap_ptr + len_, rhs.data_ptr(), rhs.len_);
            result.len_ = new_len;
            result.heap_ptr[new_len] = 0;
        }
        return result;
    }

    void append(const char* str) {
        if (!str) return;
        size_t add = std::strlen(str);
        size_t new_len = len_ + add;
        if (new_len <= 15) {
            // keep in SSO
            std::memcpy(small_buffer + len_, str, add + 1);
            len_ = new_len;
        } else {
            ensure_capacity(new_len);
            char* p = data_ptr();
            std::memcpy(p + len_, str, add + 1);
            len_ = new_len;
        }
    }

    const char& at(size_t pos) const {
        if (pos >= len_) {
            throw std::out_of_range("MyString::at out of range");
        }
        return data_ptr()[pos];
    }

    class const_iterator;

    class iterator {
    private:
        char* ptr_ = nullptr;
    public:
        iterator() = default;
        explicit iterator(char* p): ptr_(p) {}
        iterator& operator++() {
            ++ptr_; return *this;
        }
        iterator operator++(int) {
            iterator tmp = *this; ++ptr_; return tmp;
        }
        iterator& operator--() {
            --ptr_; return *this;
        }
        iterator operator--(int) {
            iterator tmp = *this; --ptr_; return tmp;
        }
        char& operator*() const { return *ptr_; }
        bool operator==(const iterator& other) const { return ptr_ == other.ptr_; }
        bool operator!=(const iterator& other) const { return ptr_ != other.ptr_; }
        bool operator==(const const_iterator& other) const;
        bool operator!=(const const_iterator& other) const;
        char* get() const { return ptr_; }
    };

    class const_iterator {
    private:
        const char* ptr_ = nullptr;
    public:
        const_iterator() = default;
        explicit const_iterator(const char* p): ptr_(p) {}
        const_iterator& operator++() { ++ptr_; return *this; }
        const_iterator operator++(int) { const_iterator tmp = *this; ++ptr_; return tmp; }
        const_iterator& operator--() { --ptr_; return *this; }
        const_iterator operator--(int) { const_iterator tmp = *this; --ptr_; return tmp; }
        const char& operator*() const { return *ptr_; }
        bool operator==(const const_iterator& other) const { return ptr_ == other.ptr_; }
        bool operator!=(const const_iterator& other) const { return ptr_ != other.ptr_; }
        const char* get() const { return ptr_; }
    };

    // cross comparisons
    inline bool iterator::operator==(const const_iterator& other) const { return ptr_ == other.get(); }
    inline bool iterator::operator!=(const const_iterator& other) const { return ptr_ != other.get(); }

public:
    iterator begin() { return iterator(const_cast<char*>(data_ptr())); }
    iterator end() { return iterator(const_cast<char*>(data_ptr()) + len_); }
    const_iterator cbegin() const { return const_iterator(data_ptr()); }
    const_iterator cend() const { return const_iterator(data_ptr() + len_); }
};

#endif
