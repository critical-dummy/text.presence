#pragma once

#include <map>
#include <string>
#include <vector>

namespace tpc {

class JsonParser;

class JsonValue {
public:
    enum class Type {
        Null,
        Boolean,
        Number,
        String,
        Object,
        Array
    };

    JsonValue();

    static JsonValue boolean(bool value);
    static JsonValue number(std::string value);
    static JsonValue string(std::string value);
    static JsonValue object();
    static JsonValue array();

    Type type() const;
    bool is_object() const;
    bool is_array() const;
    bool is_string() const;

    const std::string& string_value() const;
    const std::vector<JsonValue>& array_value() const;
    const std::map<std::string, JsonValue>& object_value() const;
    const JsonValue* find(const std::string& key) const;

private:
    friend class JsonParser;

    Type type_ = Type::Null;
    bool boolean_value_ = false;
    std::string string_value_;
    std::vector<JsonValue> array_value_;
    std::map<std::string, JsonValue> object_value_;
};

class JsonParser {
public:
    explicit JsonParser(std::string source);
    JsonValue parse();

private:
    void skip_whitespace();
    bool consume(char expected);
    JsonValue parse_value();
    JsonValue parse_object();
    JsonValue parse_array();
    JsonValue parse_string_value();
    JsonValue parse_number_value();
    std::string parse_string();
    [[noreturn]] void fail(const char* message) const;

    std::string source_;
    std::size_t position_ = 0;
};

JsonValue parse_json(const std::string& source);
bool read_text_file(const std::string& path, std::string& output);

} // namespace tpc
