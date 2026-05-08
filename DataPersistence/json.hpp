#pragma once

#include <string>
#include <vector>
#include <map>
#include <variant>
#include <stdexcept>
#include <sstream>
#include <fstream>
#include <functional>

// Lightweight JSON value: null | bool | number (double) | string | array | object
class JsonValue {
public:
    using Null   = std::monostate;
    using Bool   = bool;
    using Number = double;
    using String = std::string;
    using Array  = std::vector<JsonValue>;
    using Object = std::map<std::string, JsonValue>;

    JsonValue()                    : v_(Null{}) {}
    JsonValue(std::nullptr_t)      : v_(Null{}) {}
    JsonValue(bool b)              : v_(Bool(b)) {}
    JsonValue(int n)               : v_(Number(n)) {}
    JsonValue(int64_t n)           : v_(Number(static_cast<double>(n))) {}
    JsonValue(double n)            : v_(Number(n)) {}
    JsonValue(const char* s)       : v_(String(s)) {}
    JsonValue(std::string s)       : v_(std::move(s)) {}
    JsonValue(Array a)             : v_(std::move(a)) {}
    JsonValue(Object o)            : v_(std::move(o)) {}

    bool isNull()   const { return std::holds_alternative<Null>(v_); }
    bool isBool()   const { return std::holds_alternative<Bool>(v_); }
    bool isNumber() const { return std::holds_alternative<Number>(v_); }
    bool isString() const { return std::holds_alternative<String>(v_); }
    bool isArray()  const { return std::holds_alternative<Array>(v_); }
    bool isObject() const { return std::holds_alternative<Object>(v_); }

    bool          getBool()   const { return std::get<Bool>(v_); }
    double        getNumber() const { return std::get<Number>(v_); }
    int64_t       getInt()    const { return static_cast<int64_t>(std::get<Number>(v_)); }
    const String& getString() const { return std::get<String>(v_); }
    const Array&  getArray()  const { return std::get<Array>(v_); }
    Array&        getArray()        { return std::get<Array>(v_); }
    const Object& getObject() const { return std::get<Object>(v_); }
    Object&       getObject()       { return std::get<Object>(v_); }

    // Object key access — creates Object if currently null
    JsonValue& operator[](const std::string& key) {
        if (isNull()) v_ = Object{};
        return std::get<Object>(v_)[key];
    }
    const JsonValue& at(const std::string& key) const {
        return std::get<Object>(v_).at(key);
    }
    bool contains(const std::string& key) const {
        if (!isObject()) return false;
        return std::get<Object>(v_).count(key) > 0;
    }

    // Array index access
    JsonValue&       operator[](size_t idx)       { return std::get<Array>(v_)[idx]; }
    const JsonValue& operator[](size_t idx) const { return std::get<Array>(v_)[idx]; }

    size_t size() const {
        if (isArray())  return std::get<Array>(v_).size();
        if (isObject()) return std::get<Object>(v_).size();
        return 0;
    }

    // Push to array — creates Array if currently null
    void push_back(const JsonValue& val) {
        if (isNull()) v_ = Array{};
        std::get<Array>(v_).push_back(val);
    }

    // ── Parse ────────────────────────────────────────────────────────────────

    static JsonValue parse(const std::string& text) {
        size_t i = 0;
        return parseValue(text, i);
    }

    static JsonValue parseFile(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) return JsonValue{};
        std::string content((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());
        if (content.empty()) return JsonValue{};
        return parse(content);
    }

    // ── Stringify ────────────────────────────────────────────────────────────

    // indent < 0 -> compact; indent >= 0 -> pretty with that many spaces per level
    std::string stringify(int indent = -1) const {
        std::ostringstream ss;
        toStream(ss, indent, 0);
        return ss.str();
    }

    bool saveToFile(const std::string& path, int indent = 2) const {
        std::ofstream file(path);
        if (!file.is_open()) return false;
        file << stringify(indent);
        return true;
    }

private:
    std::variant<Null, Bool, Number, String, Array, Object> v_;

    // ── Parser helpers ────────────────────────────────────────────────────────

    static void skipWS(const std::string& s, size_t& i) {
        while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
    }

    static std::string parseString(const std::string& s, size_t& i) {
        ++i; // skip opening '"'
        std::string result;
        while (i < s.size() && s[i] != '"') {
            if (s[i] == '\\') {
                ++i;
                switch (s[i]) {
                    case '"':  result += '"';  break;
                    case '\\': result += '\\'; break;
                    case '/':  result += '/';  break;
                    case 'n':  result += '\n'; break;
                    case 'r':  result += '\r'; break;
                    case 't':  result += '\t'; break;
                    case 'b':  result += '\b'; break;
                    case 'f':  result += '\f'; break;
                    default:   result += s[i]; break;
                }
            } else {
                result += s[i];
            }
            ++i;
        }
        ++i; // skip closing '"'
        return result;
    }

    static double parseNumber(const std::string& s, size_t& i) {
        size_t start = i;
        if (s[i] == '-') ++i;
        while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) ++i;
        if (i < s.size() && s[i] == '.') {
            ++i;
            while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) ++i;
        }
        if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
            ++i;
            if (i < s.size() && (s[i] == '+' || s[i] == '-')) ++i;
            while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) ++i;
        }
        return std::stod(s.substr(start, i - start));
    }

    static Array parseArray(const std::string& s, size_t& i) {
        ++i; // skip '['
        Array arr;
        skipWS(s, i);
        if (i < s.size() && s[i] == ']') { ++i; return arr; }
        while (i < s.size()) {
            arr.push_back(parseValue(s, i));
            skipWS(s, i);
            if (i >= s.size()) break;
            if (s[i] == ']') { ++i; break; }
            if (s[i] == ',') { ++i; skipWS(s, i); }
        }
        return arr;
    }

    static Object parseObject(const std::string& s, size_t& i) {
        ++i; // skip '{'
        Object obj;
        skipWS(s, i);
        if (i < s.size() && s[i] == '}') { ++i; return obj; }
        while (i < s.size()) {
            skipWS(s, i);
            auto key = parseString(s, i);
            skipWS(s, i);
            ++i; // skip ':'
            skipWS(s, i);
            obj[key] = parseValue(s, i);
            skipWS(s, i);
            if (i >= s.size()) break;
            if (s[i] == '}') { ++i; break; }
            if (s[i] == ',') { ++i; }
        }
        return obj;
    }

    static JsonValue parseValue(const std::string& s, size_t& i) {
        skipWS(s, i);
        if (i >= s.size()) throw std::runtime_error("Unexpected end of JSON");

        char c = s[i];
        if (c == '"') return JsonValue(parseString(s, i));
        if (c == '{') return JsonValue(parseObject(s, i));
        if (c == '[') return JsonValue(parseArray(s, i));
        if (c == 't' && s.compare(i, 4, "true")  == 0) { i += 4; return JsonValue(true);  }
        if (c == 'f' && s.compare(i, 5, "false") == 0) { i += 5; return JsonValue(false); }
        if (c == 'n' && s.compare(i, 4, "null")  == 0) { i += 4; return JsonValue();      }
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c)))
            return JsonValue(parseNumber(s, i));

        throw std::runtime_error(std::string("Unexpected character: ") + c);
    }

    // ── Stringifier helper ────────────────────────────────────────────────────

    void toStream(std::ostringstream& ss, int indent, int depth) const {
        auto newline = [&](int d) {
            if (indent >= 0) {
                ss << '\n';
                ss << std::string(static_cast<size_t>(indent * d), ' ');
            }
        };

        if (isNull())   { ss << "null"; return; }
        if (isBool())   { ss << (getBool() ? "true" : "false"); return; }
        if (isNumber()) {
            double n = getNumber();
            if (n == static_cast<double>(static_cast<int64_t>(n)))
                ss << static_cast<int64_t>(n);
            else
                ss << n;
            return;
        }
        if (isString()) {
            ss << '"';
            for (char c : getString()) {
                switch (c) {
                    case '"':  ss << "\\\""; break;
                    case '\\': ss << "\\\\"; break;
                    case '\n': ss << "\\n";  break;
                    case '\r': ss << "\\r";  break;
                    case '\t': ss << "\\t";  break;
                    default:   ss << c;      break;
                }
            }
            ss << '"';
            return;
        }
        if (isArray()) {
            const auto& arr = getArray();
            if (arr.empty()) { ss << "[]"; return; }
            ss << '[';
            for (size_t k = 0; k < arr.size(); ++k) {
                newline(depth + 1);
                arr[k].toStream(ss, indent, depth + 1);
                if (k + 1 < arr.size()) ss << ',';
            }
            newline(depth);
            ss << ']';
            return;
        }
        if (isObject()) {
            const auto& obj = getObject();
            if (obj.empty()) { ss << "{}"; return; }
            ss << '{';
            size_t k = 0;
            for (const auto& [key, val] : obj) {
                newline(depth + 1);
                ss << '"' << key << '"' << ':';
                if (indent >= 0) ss << ' ';
                val.toStream(ss, indent, depth + 1);
                if (++k < obj.size()) ss << ',';
            }
            newline(depth);
            ss << '}';
        }
    }
};
