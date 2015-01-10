#ifndef BOT_EVENT_BUS_H
#define BOT_EVENT_BUS_H

#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>
#include <lua.hpp>
#include <assert.h>

namespace bot {

class EventBus {
public:
    class Value {
    public:
        enum class Type {
            Nil,
            Number,
            String,
            Bool,
            Table
        };

        struct hash {
        public:
            size_t operator()(const Value &x) const {
                size_t tag = type_(static_cast<unsigned>(x.type())), value = 0;
                switch (x.type()) {
                case Type::Number:
                    value = std::hash<double>()(x.number());
                    break;
                case Type::String:
                    value = std::hash<std::string>()(x.string());
                    break;
                case Type::Bool:
                    value = std::hash<bool>()(x.boolean());
                    break;
                case Type::Table:
                    assert(!"Table hashing NYI");
                    break;
                default:
                    break;
                }
                return tag ^ value;
            }

        private:
            std::hash<unsigned> type_;
        };

        typedef std::unordered_map<Value, Value, Value::hash> Table;

        Value() : type_(Type::Nil) {}
        Value(double num) : type_(Type::Number), number_(num) {}
        Value(std::string &&str) : type_(Type::String), string_(str) {}
        Value(const std::string &str) : type_(Type::String), string_(str) {}
        Value(bool b) : type_(Type::Bool), bool_(b) {}
        Value(std::unique_ptr<Table> &&t) : type_(Type::Table), table_(std::move(t)) {}
        Value(const Value &other)
            : type_(other.type_)
        {
            switch (type_) {
            case Type::Number:
                number_ = other.number_;
                break;
            case Type::String:
                string_ = other.string_;
                break;
            case Type::Bool:
                bool_ = other.bool_;
                break;
            case Type::Table:
                table_ = std::unique_ptr<Table>(new Table(*other.table_));
                break;
            default:
                break;
            }
        }
        Value(Value &&other)
            : type_(other.type_)
        {
            switch (type_) {
            case Type::Number:
                number_ = other.number_;
                break;
            case Type::String:
                string_ = std::move(other.string_);
                break;
            case Type::Bool:
                bool_ = other.bool_;
                break;
            case Type::Table:
                table_ = std::move(other.table_);
                break;
            default:
                break;
            }
            new (&other) Value();
        }
        ~Value()
        {
            switch (type_) {
            case Type::String:
                string_.~basic_string();
                break;
            case Type::Table:
                table_.~unique_ptr();
                break;
            default:
                break;
            }
        }

        bool operator==(const Value &other) const {
            if (type_ != other.type_) {
                return false;
            }
            switch (type_) {
            case Type::Nil:
                return true;
            case Type::Number:
                return number_ == other.number_;
            case Type::String:
                return string_ == other.string_;
            case Type::Bool:
                return bool_ == other.bool_;
            case Type::Table:
                return table_ == other.table_;
            }
        }

        Type type() const {
            return type_;
        }

        const char *strtype() const {
            const char *const types[] = {
                "nil", "number", "string", "bool", "table"
            };
            return types[static_cast<unsigned>(type_)];
        }

        double number() const {
            assert(type_ == Type::Number);
            return number_;
        }

        const std::string &string() const {
            assert(type_ == Type::String);
            return string_;
        }

        bool boolean() const {
            assert(type_ == Type::Bool);
            return bool_;
        }

        Table &table() {
            assert(type_ == Type::Table);
            return *table_;
        }

        const Table &table() const {
            assert(type_ == Type::Table);
            return *table_;
        }

        const Value &get(const std::string &key) const {
            static Value nil;
            auto it = table().find(key);
            return it == table().end() ? nil : it->second;
        }

    private:
        const Type type_;
        union {
            int nil_;
            double number_;
            std::string string_;
            bool bool_;
            std::unique_ptr<Table> table_;
        };
    };

    void fire(const std::string &name, const Value &val);
    void hook(std::string &&name, std::function<void (const Value &val)> &&fn);
    void openlib(lua_State *L);

private:
    //void hookLua(const std::string &name, lua::Value fn);

    std::unordered_map<std::string, std::vector<std::function<void (const Value &val)>>> handlers;
    //std::unordered_map<std::string, std::vector<lua::Value>> lua_handlers;
};

}

#endif
