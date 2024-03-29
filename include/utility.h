#pragma once
#include <string>
#include <format>
#include <vector>
#include <iostream>
#include <concepts>

namespace dark {

struct error : std::exception {
    std::string msg;
    error(std::string __s) : msg(std::move(__s)) {
        std::cerr << std::format("\033[31m{}\n\033[0m", msg);
    }
    const char *what() const noexcept override { return msg.c_str(); }
};

struct warning : std::exception {
    std::string msg;
    warning(std::string __s) : msg(std::move(__s)) {
        std::cerr << std::format("\033[33mWarning: {}\n\033[0m", msg);
    }
    const char *what() const noexcept override { return msg.c_str(); }
};

/**
 * @brief Some run-time assertion. 
 * If failed, there's a bug in my code.
 * Distinguish it from 'semantic_check'.
 */
template <typename ..._Args>
inline void runtime_assert(bool __cond, _Args &&...__args) {
    if (__builtin_expect(__cond, true)) return;
    throw error((std::string(std::forward <_Args>(__args)) + ...));
}

/* Cast to base is safe. */
template <typename _Up, typename _Tp>
requires std::is_base_of_v <std::remove_pointer_t<_Up>, _Tp>
inline _Up safe_cast(_Tp *__ptr) noexcept {
    return static_cast <_Up> (__ptr);
}

/* Cast to derived may throw! */
template <typename _Up, typename _Tp>
requires (!std::is_base_of_v <std::remove_pointer_t<_Up>, _Tp>)
inline _Up safe_cast(_Tp *__ptr) {
    auto __tmp = dynamic_cast <_Up> (__ptr);
    runtime_assert(__tmp, "Cast failed.");
    return __tmp;
}


namespace detail {

template <typename _Tp>
concept __value_hidable = (sizeof(_Tp) <= sizeof(void *)) && std::is_trivial_v <_Tp>;

} // namespace detail

/* A wrapper that is using to help hide implement. */
struct hidden_impl {
  private:
    void *impl {};
  public:
    /* Set the hidden implement pointer. */
    void set_ptr(void *__impl) noexcept { impl = __impl; }
    /* Set the hidden implement value. */
    template <class T> requires detail::__value_hidable <T>
    void set_val(T __val) noexcept { get_val <T> () = __val; }

    /* Get the hidden implement pointer. */
    template <class T = void>
    T *get_ptr() const noexcept { return static_cast <T *> (impl); }
    /* Get the hidden implement value. */
    template <class T> requires detail::__value_hidable <T>
    T &get_val() noexcept { return *(reinterpret_cast <T *> (&impl)); }
};

/* A central allocator that is intented to avoid memleak. */
template <typename _Vp>
struct central_allocator {
 private:
    std::vector <_Vp *> data;
  public:
    /* Allocate one node. */
    template <typename _Tp = _Vp, typename ..._Args>
    requires std::is_base_of_v <_Vp, _Tp>
    _Tp *allocate(_Args &&...args) {
        auto *__ptr = new _Tp(std::forward <_Args>(args)...);
        return data.push_back(__ptr), __ptr;
    }

    ~central_allocator() { for (auto *__ptr : data) delete __ptr; }
};

template <typename _Range>
std::string join_strings(_Range &&__container) {
    std::size_t __cnt {};
    std::string __ret {};
    for (auto &__str : __container) __cnt += __str.size();
    __ret.reserve(__cnt);
    for (auto &__str : __container) __ret += __str;
    return __ret;
}

} // namespace dark
