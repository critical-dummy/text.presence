#include "json.hpp"

#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace {

void append_utf8(std::string& out, unsigned int cp) {
    if (cp <= 0x7f) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7ff) {
        out.push_back(static_cast<char>(0xc0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3f)));
    } else if (cp <= 0xffff) {
        out.push_back(static_cast<char>(0xe0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3f)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3f)));
    } else if (cp <= 0x10ffff) {
        out.push_back(static_cast<char>(0xf0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3f)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3f)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3f)));
    }
}

unsigned int hex_value(char ch) {
    if (ch >= '0' && ch <= '9') return static_cast<unsigned int>(ch - '0');
    if (ch >= 'a' && ch <= 'f') return static_cast<unsigned int>(ch - 'a' + 10);
    if (ch >= 'A' && ch <= 'F') return static_cast<unsigned int>(ch - 'A' + 10);
    return 0xffffffffu;
}

}

namespace tpc {

JsonValue::JsonValue() = default;

JsonValue JsonValue::boolean(bool value) {
    JsonValue result;
    result.type_ = Type::Boolean;
    result.boolean_value_ = value;
    return result;
}

JsonValue JsonValue::number(std::string value) {
    JsonValue result;
    result.type_ = Type::Number;
    result.string_value_ = std::move(value);
    return result;
}

JsonValue JsonValue::string(std::string value) {
    JsonValue result;
    result.type_ = Type::String;
    result.string_value_ = std::move(value);
    return result;
}

JsonValue JsonValue::object() {
    JsonValue result;
    result.type_ = Type::Object;
    return result;
}

JsonValue JsonValue::array() {
    JsonValue result;
    result.type_ = Type::Array;
    return result;
}

JsonValue::Type JsonValue::type() const {
    return type_;
}

bool JsonValue::is_object() const {
    return type_ == Type::Object;
}

bool JsonValue::is_array() const {
    return type_ == Type::Array;
}

bool JsonValue::is_string() const {
    return type_ == Type::String;
}

const std::string& JsonValue::string_value() const {
    return string_value_;
}

const std::vector<JsonValue>& JsonValue::array_value() const {
    return array_value_;
}

const std::map<std::string, JsonValue>& JsonValue::object_value() const {
    return object_value_;
}

const JsonValue* JsonValue::find(const std::string& key) const {
    if (!is_object()) return nullptr;

    const auto it = object_value_.find(key);
    return it == object_value_.end() ? nullptr : &it->second;
}

JsonParser::JsonParser(std::string source)
    : source_(std::move(source)) {}

void JsonParser::skip_whitespace() {
    while (position_ < source_.size() &&
           std::isspace(static_cast<unsigned char>(source_[position_]))) {
        ++position_;
    }
}

bool JsonParser::consume(char expected) {
    skip_whitespace();

    if (position_ >= source_.size() || source_[position_] != expected) {
        return false;
    }

    ++position_;
    return true;
}

[[noreturn]] void JsonParser::fail(const char* message) const {
    throw std::runtime_error(message);
}

std::string JsonParser::parse_string() {
    skip_whitespace();

    if (position_ >= source_.size() || source_[position_] != '"') {
        fail("expected JSON string");
    }

    ++position_;
    std::string result;

    while (position_ < source_.size()) {
        const char ch = source_[position_++];

        if (ch == '"') return result;

        if (ch != '\\') {
            result.push_back(ch);
            continue;
        }

        if (position_ >= source_.size()) {
            fail("unterminated JSON escape");
        }

        const char escape = source_[position_++];

        switch (escape) {
        case '"': result.push_back('"'); break;
        case '\\': result.push_back('\\'); break;
        case '/': result.push_back('/'); break;
        case 'b': result.push_back('\b'); break;
        case 'f': result.push_back('\f'); break;
        case 'n': result.push_back('\n'); break;
        case 'r': result.push_back('\r'); break;
        case 't': result.push_back('\t'); break;
        case 'u': {
            if (position_ + 4 > source_.size()) {
                fail("invalid unicode escape");
            }

            unsigned int cp = 0;
            for (int i = 0; i < 4; ++i) {
                const unsigned int value = hex_value(source_[position_++]);
                if (value == 0xffffffffu) {
                    fail("invalid unicode escape");
                }
                cp = (cp << 4) | value;
            }

            append_utf8(result, cp);
            break;
        }
        default:
            fail("invalid JSON escape");
        }
    }

    fail("unterminated JSON string");
}

JsonValue JsonParser::parse_object() {
    if (!consume('{')) fail("expected JSON object");

    JsonValue result = JsonValue::object();

    if (consume('}')) return result;

    while (true) {
        const std::string key = parse_string();

        if (!consume(':')) fail("expected ':' in object");

        result.object_value_[key] = parse_value();

        if (consume('}')) return result;
        if (!consume(',')) fail("expected ',' in object");
    }
}

JsonValue JsonParser::parse_array() {
    if (!consume('[')) fail("expected JSON array");

    JsonValue result = JsonValue::array();

    if (consume(']')) return result;

    while (true) {
        result.array_value_.push_back(parse_value());

        if (consume(']')) return result;
        if (!consume(',')) fail("expected ',' in array");
    }
}

JsonValue JsonParser::parse_string_value() {
    return JsonValue::string(parse_string());
}

JsonValue JsonParser::parse_number_value() {
    skip_whitespace();
    const std::size_t start = position_;

    while (position_ < source_.size()) {
        const char ch = source_[position_];
        if ((ch >= '0' && ch <= '9') ||
            ch == '-' || ch == '+' ||
            ch == '.' || ch == 'e' || ch == 'E') {
            ++position_;
        } else {
            break;
        }
    }

    if (position_ == start) fail("invalid JSON number");

    return JsonValue::number(
        source_.substr(start, position_ - start)
    );
}

JsonValue JsonParser::parse_value() {
    skip_whitespace();

    if (position_ >= source_.size()) {
        fail("unexpected end of JSON");
    }

    switch (source_[position_]) {
    case '{':
        return parse_object();
    case '[':
        return parse_array();
    case '"':
        return parse_string_value();
    case 't':
        if (source_.compare(position_, 4, "true") == 0) {
            position_ += 4;
            return JsonValue::boolean(true);
        }
        break;
    case 'f':
        if (source_.compare(position_, 5, "false") == 0) {
            position_ += 5;
            return JsonValue::boolean(false);
        }
        break;
    case 'n':
        if (source_.compare(position_, 4, "null") == 0) {
            position_ += 4;
            return JsonValue();
        }
        break;
    default:
        return parse_number_value();
    }

    fail("invalid JSON value");
}

JsonValue JsonParser::parse() {
    JsonValue result = parse_value();
    skip_whitespace();

    if (position_ != source_.size()) {
        fail("unexpected data after JSON value");
    }

    return result;
}

JsonValue parse_json(const std::string& source) {
    return JsonParser(source).parse();
}

bool read_text_file(const std::string& path, std::string& output) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;

    std::ostringstream buffer;
    buffer << input.rdbuf();
    output = buffer.str();
    return true;
}

}
