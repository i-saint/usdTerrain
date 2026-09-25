#include "pch.h"
#include "json.h"

#include <cassert>


ARCH_EXPORT void TestExtractJsonFields()
{
    enum class TestEnum {
        None = 0,
        Value = 7,
    };

    {
        simdjson::ondemand::parser parser;
        auto json = simdjson::padded_string(std::string_view(R"({"value":42})"));
        simdjson::ondemand::document doc;
        assert(parser.iterate(json).get(doc) == simdjson::SUCCESS);

        JsonObject root;
        assert(doc.get_object().get(root) == simdjson::SUCCESS);

        JsonValue value;
        assert(ExtractJsonFields(root, Field("value", value)) == 1);

        int32_t extractedValue = 0;
        assert(value.get_int32().get(extractedValue) == simdjson::SUCCESS);
        assert(extractedValue == 42);
    }

    {
        simdjson::ondemand::parser parser;
        auto json = simdjson::padded_string(std::string_view(R"({"object":{"inner":11}})"));
        simdjson::ondemand::document doc;
        assert(parser.iterate(json).get(doc) == simdjson::SUCCESS);

        JsonObject root;
        assert(doc.get_object().get(root) == simdjson::SUCCESS);

        JsonObject objectValue;
        assert(ExtractJsonFields(root, Field("object", objectValue)) == 1);

        int innerValue = 0;
        assert(ExtractJsonFields(objectValue, Field("inner", innerValue)) == 1);
        assert(innerValue == 11);
    }

    {
        simdjson::ondemand::parser parser;
        auto json = simdjson::padded_string(std::string_view(R"({"array":[1,2,3]})"));
        simdjson::ondemand::document doc;
        assert(parser.iterate(json).get(doc) == simdjson::SUCCESS);

        JsonObject root;
        assert(doc.get_object().get(root) == simdjson::SUCCESS);

        JsonArray arrayValue;
        assert(ExtractJsonFields(root, Field("array", arrayValue)) == 1);

        int sum = 0;
        int count = 0;
        for (auto item : arrayValue) {
            int value = 0;
            assert(item.get_int32().get(value) == simdjson::SUCCESS);
            sum += value;
            ++count;
        }
        assert(count == 3);
        assert(sum == 6);
    }

    {
        simdjson::ondemand::parser parser;
        auto json = simdjson::padded_string(std::string_view(R"({"s":"hello"})"));
        simdjson::ondemand::document doc;
        assert(parser.iterate(json).get(doc) == simdjson::SUCCESS);

        JsonObject root;
        assert(doc.get_object().get(root) == simdjson::SUCCESS);

        std::string value;
        assert(ExtractJsonFields(root, Field("s", value)) == 1);
        assert(value == "hello");
    }

    {
        simdjson::ondemand::parser parser;
        auto json = simdjson::padded_string(std::string_view(R"({"s":"view"})"));
        simdjson::ondemand::document doc;
        assert(parser.iterate(json).get(doc) == simdjson::SUCCESS);

        JsonObject root;
        assert(doc.get_object().get(root) == simdjson::SUCCESS);

        std::string_view value;
        assert(ExtractJsonFields(root, Field("s", value)) == 1);
        assert(value == "view");
    }

    {
        simdjson::ondemand::parser parser;
        auto json = simdjson::padded_string(std::string_view(R"({"b":true})"));
        simdjson::ondemand::document doc;
        assert(parser.iterate(json).get(doc) == simdjson::SUCCESS);

        JsonObject root;
        assert(doc.get_object().get(root) == simdjson::SUCCESS);

        bool value = false;
        assert(ExtractJsonFields(root, Field("b", value)) == 1);
        assert(value);
    }

    {
        simdjson::ondemand::parser parser;
        auto json = simdjson::padded_string(std::string_view(R"({"i":-3})"));
        simdjson::ondemand::document doc;
        assert(parser.iterate(json).get(doc) == simdjson::SUCCESS);

        JsonObject root;
        assert(doc.get_object().get(root) == simdjson::SUCCESS);

        int value = 0;
        assert(ExtractJsonFields(root, Field("i", value)) == 1);
        assert(value == -3);
    }

    {
        simdjson::ondemand::parser parser;
        auto json = simdjson::padded_string(std::string_view(R"({"e":7})"));
        simdjson::ondemand::document doc;
        assert(parser.iterate(json).get(doc) == simdjson::SUCCESS);

        JsonObject root;
        assert(doc.get_object().get(root) == simdjson::SUCCESS);

        TestEnum value = TestEnum::None;
        assert(ExtractJsonFields(root, Field("e", value)) == 1);
        assert(value == TestEnum::Value);
    }

    {
        simdjson::ondemand::parser parser;
        auto json = simdjson::padded_string(std::string_view(R"({"f":1.25})"));
        simdjson::ondemand::document doc;
        assert(parser.iterate(json).get(doc) == simdjson::SUCCESS);

        JsonObject root;
        assert(doc.get_object().get(root) == simdjson::SUCCESS);

        float value = 0.0f;
        assert(ExtractJsonFields(root, Field("f", value)) == 1);
        assert(value == 1.25f);
    }
}
