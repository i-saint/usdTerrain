#pragma once

#include <string_view>
#include <tuple>
#include <type_traits>
#include <functional>
#include <simdjson/simdjson.h>

using JsonValue = simdjson::ondemand::value;
using JsonObject = simdjson::ondemand::object;
using JsonArray = simdjson::ondemand::array;

namespace detail {
template<class T, class = void>
struct unary_arg {
    using type = void;
};

template<class R, class A>
struct unary_arg<R(*)(A), void> {
    using type = A;
};

template<class R, class A>
struct unary_arg<R(&)(A), void> {
    using type = A;
};

template<class C, class R, class A>
struct unary_arg<R(C::*)(A), void> {
    using type = A;
};

template<class C, class R, class A>
struct unary_arg<R(C::*)(A) const, void> {
    using type = A;
};

template<class C, class R, class A>
struct unary_arg<R(C::*)(A) noexcept, void> {
    using type = A;
};

template<class C, class R, class A>
struct unary_arg<R(C::*)(A) const noexcept, void> {
    using type = A;
};

template<class T>
struct unary_arg<T, std::void_t<decltype(&std::remove_reference_t<T>::operator())>>
    : unary_arg<decltype(&std::remove_reference_t<T>::operator())> {};

template<class T>
using unary_arg_t = typename unary_arg<std::remove_cv_t<std::remove_reference_t<T>>>::type;

template<class T>
inline constexpr bool is_unary_invocable_v =
    !std::is_void_v<unary_arg_t<T>> &&
    std::is_invocable_v<T, unary_arg_t<T>>;

template<class T>
inline constexpr bool is_enum_invocable_v =
    is_unary_invocable_v<T> &&
    std::is_enum_v<std::remove_cv_t<std::remove_reference_t<unary_arg_t<T>>>>;
}

template<class T>
inline decltype(auto) Field(std::string_view name, T&& value)
{
    if constexpr (detail::is_unary_invocable_v<T>) {
        return std::make_tuple(name, std::forward<T>(value));
    }
    else if constexpr (std::is_pointer_v<T>) {
        return std::make_tuple(name, value);
    }
    else {
        return std::make_tuple(name, &value);
    }
}

template<class... Args>
inline int ExtractJsonFields(JsonObject json, Args... args)
{
    auto handle = []<class T>(std::string_view k, JsonValue v, std::tuple<std::string_view, T> f) -> bool
    {
        if (k != std::get<0>(f))
            return false;

        using ValueType = std::remove_pointer_t<T>;
        if constexpr (std::is_same_v<T, JsonValue*>) {
            *std::get<1>(f) = v;
            return true;
        }
        else if constexpr (std::is_same_v<T, JsonObject*>) {
            if (v.get_object().get(*std::get<1>(f)) == simdjson::SUCCESS) {
                return true;
            }
        }
        else if constexpr (std::is_same_v<T, JsonArray*>) {
            if (v.get_array().get(*std::get<1>(f)) == simdjson::SUCCESS) {
                return true;
            }
        }
        else if constexpr (std::is_same_v<T, std::string*>) {
            std::string_view tmp;
            if (v.get_string().get(tmp) == simdjson::SUCCESS) {
                *std::get<1>(f) = std::string(tmp);
                return true;
            }
        }
        else if constexpr (std::is_same_v<T, std::string_view*>) {
            if (v.get_string().get(*std::get<1>(f)) == simdjson::SUCCESS) {
                return true;
            }
        }
        else if constexpr (std::is_same_v<T, bool*>) {
            if (v.get_bool().get(*std::get<1>(f)) == simdjson::SUCCESS) {
                return true;
            }
        }
        else if constexpr (std::is_integral_v<ValueType> || std::is_enum_v<ValueType>) {
            int tmp = 0;
            if (v.get_int32().get(tmp) == simdjson::SUCCESS) {
                *std::get<1>(f) = static_cast<ValueType>(tmp);
                return true;
            }
        }
        else if constexpr (std::is_floating_point_v<ValueType>) {
            double tmp = 0.0;
            if (v.get_double().get(tmp) == simdjson::SUCCESS) {
                *std::get<1>(f) = static_cast<ValueType>(tmp);
                return true;
            }
        }
        else if constexpr (std::is_invocable_v<T, JsonObject>) {
            JsonObject tmp;
            if (v.get_object().get(tmp) == simdjson::SUCCESS) {
                std::get<1>(f)(tmp);
                return true;
            }
        }
        else if constexpr (std::is_invocable_v<T, JsonArray>) {
            JsonArray tmp;
            if (v.get_array().get(tmp) == simdjson::SUCCESS) {
                std::get<1>(f)(tmp);
                return true;
            }
        }
        else if constexpr (std::is_invocable_v<T, std::string_view>) {
            std::string_view tmp;
            if (v.get_string().get(tmp) == simdjson::SUCCESS) {
                std::get<1>(f)(tmp);
                return true;
            }
        }
        else if constexpr (std::is_invocable_v<T, bool>) {
            bool tmp = false;
            if (v.get_bool().get(tmp) == simdjson::SUCCESS) {
                std::get<1>(f)(tmp);
                return true;
            }
        }
        else if constexpr (std::is_invocable_v<T, int>) {
            int tmp = 0;
            if (v.get_int32().get(tmp) == simdjson::SUCCESS) {
                std::get<1>(f)(tmp);
                return true;
            }
        }
        else if constexpr (detail::is_enum_invocable_v<T>) {
            using EnumType = std::remove_cv_t<std::remove_reference_t<detail::unary_arg_t<T>>>;
            int tmp = 0;
            if (v.get_int32().get(tmp) == simdjson::SUCCESS) {
                std::get<1>(f)(static_cast<EnumType>(tmp));
                return true;
            }
        }
        else if constexpr (std::is_invocable_v<T, float>) {
            double tmp = 0.0;
            if (v.get_double().get(tmp) == simdjson::SUCCESS) {
                std::get<1>(f)(static_cast<float>(tmp));
                return true;
            }
        }
        else {
            static_assert(!sizeof(T), "Unsupported type for JSON field extraction.");
        }
        return false;
    };

    int r = 0;
    for (auto field : json) {
        std::string_view k;
        if (field.unescaped_key().get(k) != simdjson::SUCCESS) {
            continue;
        }

        JsonValue v;
        if (field.value().get(v) != simdjson::SUCCESS) {
            continue;
        }

        bool handled = (handle(k, v, args) || ...);
        if (handled) {
            ++r;
        }
    }
    return r;
}

template<class... Args>
inline int ExtractJsonFields(JsonValue json, Args... args)
{
    if (auto obj = json.get_object()) {
        return ExtractJsonFields(obj.value_unsafe(), args...);
    }
    return 0;
}
