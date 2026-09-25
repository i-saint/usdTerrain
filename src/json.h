#pragma once

#include <string_view>
#include <tuple>
#include <type_traits>
#include <functional>
#include <simdjson/simdjson.h>

using JsonValue = simdjson::ondemand::value;
using JsonObject = simdjson::ondemand::object;
using JsonArray = simdjson::ondemand::array;

template<class T>
inline std::tuple<std::string_view, T*> Field(const std::string_view& name, T& value)
{
    return std::make_tuple(name, &value);
}

template<class... Args>
inline int ExtractJsonFields(const JsonObject& json, Args... args)
{
    auto handle = []<class T>(std::string_view k, JsonValue v, std::tuple<std::string_view, T*> f) -> bool
    {
        if (k != std::get<0>(f))
            return false;

        if constexpr (std::is_same_v<T, JsonValue>) {
            *std::get<1>(f) = v;
            return true;
        }
        else if constexpr (std::is_same_v<T, JsonObject>) {
            if (v.get_object().get(*std::get<1>(f)) == simdjson::SUCCESS) {
                return true;
            }
        }
        else if constexpr (std::is_same_v<T, JsonArray>) {
            if (v.get_array().get(*std::get<1>(f)) == simdjson::SUCCESS) {
                return true;
            }
        }
        else if constexpr (std::is_same_v<T, std::string>) {
            std::string_view tmp;
            if (v.get_string().get(tmp) == simdjson::SUCCESS) {
                *std::get<1>(f) = std::string(tmp);
                return true;
            }
        }
        else if constexpr (std::is_same_v<T, std::string_view>) {
            if (v.get_string().get(*std::get<1>(f)) == simdjson::SUCCESS) {
                return true;
            }
        }
        else if constexpr (std::is_same_v<T, bool>) {
            if (v.get_bool().get(*std::get<1>(f)) == simdjson::SUCCESS) {
                return true;
            }
        }
        else if constexpr (std::is_integral_v<T> || std::is_enum_v<T>) {
            int tmp = 0;
            if (v.get_int32().get(tmp) == simdjson::SUCCESS) {
                *std::get<1>(f) = static_cast<T>(tmp);
                return true;
            }
        }
        else if constexpr (std::is_floating_point_v<T>) {
            double tmp = 0.0;
            if (v.get_double().get(tmp) == simdjson::SUCCESS) {
                *std::get<1>(f) = static_cast<T>(tmp);
                return true;
            }
        }
        else {
            static_assert(!sizeof(T), "Unsupported type for JSON field extraction.");
        }
    };

    int r = 0;
    for (auto [k, v] : json) {
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
