#ifndef BOT_EVENT_BUS_H
#define BOT_EVENT_BUS_H

#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>
#include <lua.hpp>
#include <assert.h>

#include "LuaWrap.h"
#include "LuaRef.h"

namespace bot {

class EventBus : public ObjectMixins<EventBus> {
public:
    class Value {
    public:
        enum class Type {
            Nil,
            Number,
            String,
            Bool,
            Table,
            OwnedObject,
            RefObject
        };

        struct hash {
        public:
            size_t operator()(const Value &x) const {
                size_t tag = type_(static_cast<unsigned>(x.type())), value = 0;
                switch (x.type()) {
                case Type::Nil:
                    break;
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
                case Type::OwnedObject:
                case Type::RefObject:
                    value = x.object().hash();
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
        Value(const char *str) : type_(Type::String), string_(str) {}
        Value(std::string &&str) : type_(Type::String), string_(str) {}
        Value(const std::string &str) : type_(Type::String), string_(str) {}
        explicit Value(bool b) : type_(Type::Bool), bool_(b) {}
        Value(std::unique_ptr<Table> &&t) : type_(Type::Table), table_(std::move(t)) {}
        Value(IObject &object) : type_(Type::RefObject), ref_object_(&object) {}
        Value(const Value &other)
            : type_(other.type_)
        {
            switch (type_) {
            case Type::Nil:
                break;
            case Type::Number:
                new (&number_) double(other.number_);
                break;
            case Type::String:
                new (&string_) std::string(other.string_);
                break;
            case Type::Bool:
                new (&bool_) bool(other.bool_);
                break;
            case Type::Table:
                new (&table_) std::unique_ptr<Table>(new Table(*other.table_));
                break;
            case Type::OwnedObject:
                assert(!"Owned objects cannot be copied");
                break;
            case Type::RefObject:
                new (&ref_object_) IObject*(other.ref_object_);
                break;
            }
        }
        Value(Value &&other)
            : type_(other.type_)
        {
            switch (type_) {
            case Type::Nil:
                break;
            case Type::Number:
                new (&number_) double(other.number_);
                break;
            case Type::String:
                new (&string_) std::string(std::move(other.string_));
                break;
            case Type::Bool:
                new (&bool_) bool(other.bool_);
                break;
            case Type::Table:
                new (&table_) std::unique_ptr<Table>(std::move(other.table_));
                break;
            case Type::OwnedObject:
                new (&owned_object_) std::unique_ptr<IObject>(std::move(other.owned_object_));
                break;
            case Type::RefObject:
                new (&ref_object_) IObject*(other.ref_object_);
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
            case Type::OwnedObject:
                owned_object_.~unique_ptr();
                break;
            default:
                break;
            }
        }

        Value &operator=(const Value &other) {
            this->~Value();
            if (this != &other) {
                new (this) Value(other);
            }
            return *this;
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
            case Type::OwnedObject:
            case Type::RefObject:
                return object().hash() == other.object().hash();
            }
        }

        Type type() const {
            return type_;
        }

        const char *strtype() const {
            const char *const types[] = {
                "nil", "number", "string", "bool", "table"
            };
            if (type_ == Type::OwnedObject || type_ == Type::RefObject) {
                return object().tname();
            }
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

        const IObject &object() const {
            switch (type_) {
            case Type::OwnedObject:
                return *owned_object_;
            case Type::RefObject:
                return *ref_object_;
            default:
                assert(!"Not an object");
            }
        }

        const Value &get(const std::string &key) const {
            static Value nil;
            auto it = table().find(key);
            return it == table().end() ? nil : it->second;
        }

        void wrap(lua_State *L) const;
        static Value unwrap(lua_State *L);
        std::string to_string(size_t indent = 0) const;
        void print() const;

    private:
        const Type type_;
        union {
            int nil_;
            double number_;
            std::string string_;
            bool bool_;
            std::unique_ptr<Table> table_;
            std::unique_ptr<IObject> owned_object_;
            IObject *ref_object_;
        };
    };

    EventBus() : ObjectMixins<EventBus>("EventBus") {}

    void fire(const std::string &name, const Value &val);
    void hook(std::string &&name, std::function<void (const Value &val)> &&fn);
    void openlib(lua_State *L);
    void print();

    int __index(lua_State *L);

private:
    static int lua_hook(lua_State *L);
    static int lua_fire(lua_State *L);

    std::unordered_map<std::string, std::vector<std::function<void (const Value &val)>>> handlers;
    std::unordered_map<std::string, std::vector<LuaRef>> lua_handlers;
};

}

#endif
