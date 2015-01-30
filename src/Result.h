#ifndef BOT_RESULT_H
#define BOT_RESULT_H

namespace bot {

template<typename T, typename E>
class Result {
public:
    enum class Type {
        Ok,
        Err
    };

    static Result Ok(T val) {
        Result res(Type::Ok);
        new (&res.ok_) T(val);
        return res;
    }

    static Result Err(E err) {
        Result res(Type::Err);
        new (&res.err_) E(err);
        return res;
    }

private:
    Result(Type type) : type_(type) {}

    const Type type_;
    union {
        T ok_;
        E err_;
    };
};

}

#endif
