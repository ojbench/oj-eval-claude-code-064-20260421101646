#pragma once
#include <array>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <exception>
#include <iostream>
#include <ostream>
#include <span>
#include <string_view>
#include <string>
#include <vector>
#include <type_traits>
#include <cstdint>

namespace sjtu {

using sv_t = std::string_view;

struct format_error : std::exception {
public:
    format_error(const char *msg = "invalid format") : msg(msg) {}
    auto what() const noexcept -> const char * override {
        return msg;
    }

private:
    const char *msg;
};

template <typename Tp>
struct formatter;

struct format_info {
    inline static constexpr auto npos = static_cast<std::size_t>(-1);
    std::size_t spec_pos; // absolute position of the '%' in the original string
    std::size_t consumed; // how many characters consumed after '%'
};

template <typename... Args>
struct format_string {
public:
    // must be constructed at compile time, to ensure the format string is valid
    consteval format_string(const char *fmt);
    constexpr auto get_format() const -> std::string_view {
        return fmt_str;
    }
    constexpr auto get_index() const -> std::span<const format_info> {
        return fmt_idx;
    }

private:
    inline static constexpr auto Nm = sizeof...(Args);
    std::string_view fmt_str;            // the format string
    std::array<format_info, Nm> fmt_idx; // where are the specifiers
};

consteval auto find_specifier(sv_t &fmt, const char* base) -> std::size_t {
    while (true) {
        if (const auto next = fmt.find('%'); next == sv_t::npos) {
            return format_info::npos;
        } else if (next + 1 == fmt.size()) {
            throw format_error{"missing specifier after '%'"};
        } else if (fmt[next + 1] == '%') {
            // escape the specifier
            fmt.remove_prefix(next + 2);
        } else {
            std::size_t abs_pos = static_cast<std::size_t>(fmt.data() + next - base);
            fmt.remove_prefix(next + 1);
            return abs_pos;
        }
    }
};

template <typename T>
consteval auto parse_one(sv_t &fmt, format_info &info, const char* base) -> void {
    std::size_t spec_pos = find_specifier(fmt, base);
    if (spec_pos == format_info::npos) {
        throw format_error{"too few specifiers"};
    }

    const auto consumed = formatter<T>::parse(fmt);

    info = {
        .spec_pos = spec_pos,
        .consumed = consumed,
    };

    if (consumed > 0) {
        fmt.remove_prefix(consumed);
    } else {
        throw format_error{"invalid specifier"};
    }
}

template <typename... Args>
consteval auto compile_time_format_check(sv_t fmt, std::span<format_info> idx, const char* base) -> void {
    std::size_t n = 0;
    (parse_one<Args>(fmt, idx[n++], base), ...);
    if (find_specifier(fmt, base) != format_info::npos) // extra specifier found
        throw format_error{"too many specifiers"};
}

template <typename... Args>
consteval format_string<Args...>::format_string(const char *fmt) :
    fmt_str(fmt), fmt_idx() {
    compile_time_format_check<Args...>(fmt_str, fmt_idx, fmt);
}

// Specialization for strings
template <typename StrLike>
    requires(
        std::same_as<std::decay_t<StrLike>, std::string> ||
        std::same_as<std::decay_t<StrLike>, std::string_view> ||
        std::same_as<std::decay_t<StrLike>, char *> ||
        std::same_as<std::decay_t<StrLike>, const char *> ||
        std::is_array_v<std::remove_reference_t<StrLike>>
    )
struct formatter<StrLike> {
    static constexpr auto parse(sv_t fmt) -> std::size_t {
        if (fmt.starts_with("s")) return 1;
        if (fmt.starts_with("_")) return 1;
        return 0;
    }
    static auto format_to(std::ostream &os, const StrLike &val, sv_t fmt) -> void {
        if (fmt.starts_with("s") || fmt.starts_with("_")) {
            os << val;
        } else {
            throw format_error{};
        }
    }
};

// Specialization for signed integers
template <typename Int>
    requires(std::integral<Int> && std::is_signed_v<Int> && !std::same_as<std::decay_t<Int>, char> && !std::same_as<std::decay_t<Int>, bool>)
struct formatter<Int> {
    static constexpr auto parse(sv_t fmt) -> std::size_t {
        if (fmt.starts_with("d")) return 1;
        if (fmt.starts_with("_")) return 1;
        return 0;
    }
    static auto format_to(std::ostream &os, const Int &val, sv_t fmt) -> void {
        if (fmt.starts_with("d") || fmt.starts_with("_")) {
            os << static_cast<int64_t>(val);
        } else {
            throw format_error{};
        }
    }
};

// Specialization for unsigned integers
template <typename UInt>
    requires(std::integral<UInt> && std::is_unsigned_v<UInt> && !std::same_as<std::decay_t<UInt>, char> && !std::same_as<std::decay_t<UInt>, bool>)
struct formatter<UInt> {
    static constexpr auto parse(sv_t fmt) -> std::size_t {
        if (fmt.starts_with("u")) return 1;
        if (fmt.starts_with("_")) return 1;
        return 0;
    }
    static auto format_to(std::ostream &os, const UInt &val, sv_t fmt) -> void {
        if (fmt.starts_with("u") || fmt.starts_with("_")) {
            os << static_cast<uint64_t>(val);
        } else {
            throw format_error{};
        }
    }
};

// Specialization for vectors
template <typename T>
struct formatter<std::vector<T>> {
    static constexpr auto parse(sv_t fmt) -> std::size_t {
        if (fmt.starts_with("_")) return 1;
        return 0;
    }
    static auto format_to(std::ostream &os, const std::vector<T> &val, sv_t fmt) -> void {
        if (fmt.starts_with("_")) {
            os << "[";
            for (std::size_t i = 0; i < val.size(); ++i) {
                formatter<T>::format_to(os, val[i], "_");
                if (i + 1 < val.size()) os << ",";
            }
            os << "]";
        } else {
            throw format_error{};
        }
    }
};

template <typename... Args>
using format_string_t = format_string<std::decay_t<Args>...>;

template <typename... Args>
inline auto printf(format_string_t<Args...> fmt, const Args &...args) -> void {
    sv_t s = fmt.get_format();
    auto indices = fmt.get_index();

    std::size_t current_pos = 0;
    std::size_t arg_idx = 0;

    auto process_arg = [&](const auto& arg) {
        const auto& info = indices[arg_idx++];

        // Print everything before the specifier, handling escaped %%
        std::size_t spec_pos = info.spec_pos;
        sv_t prefix = s.substr(current_pos, spec_pos - current_pos);

        for (std::size_t i = 0; i < prefix.size(); ++i) {
            if (prefix[i] == '%' && i + 1 < prefix.size() && prefix[i+1] == '%') {
                std::cout << '%';
                i++;
            } else {
                std::cout << prefix[i];
            }
        }

        // Format the argument
        formatter<std::decay_t<decltype(arg)>>::format_to(std::cout, arg, s.substr(spec_pos + 1, info.consumed));

        current_pos = spec_pos + 1 + info.consumed;
    };

    (process_arg(args), ...);

    // Print remaining part of the string
    sv_t remaining = s.substr(current_pos);
    for (std::size_t i = 0; i < remaining.size(); ++i) {
        if (remaining[i] == '%' && i + 1 < remaining.size() && remaining[i+1] == '%') {
            std::cout << '%';
            i++;
        } else {
            std::cout << remaining[i];
        }
    }
}

} // namespace sjtu
