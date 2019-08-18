/**
Goal is to create an intermediate functor struct that can point to
lambdas including capture lambdas.

Then use that functor as a member functor that passes on its owner class
as a function argument to invoke the lambda it was pointed to

This is part of a tutorial series. Visit:
https://github.com/TheMaverickProgrammer/C-Python-Like-Class-Member-Decorators
*/

#include <iostream>
#include <memory>
#include <cassert>
#include <chrono>
#include <ctime> 
#include <stdexcept>
#include <functional>
#include <type_traits>
#include <string>
#include <tuple>
#include <variant>

using namespace std::placeholders;
using namespace std;

////////////////////////////////////
// weak optional value structure  //
////////////////////////////////////

template<typename T>
struct optional_type {
    T value;
    bool OK;
    bool BAD;
    std::string msg;

    // implicitly dissolve into value type T
    operator T() {
        return value;
    }

    optional_type(T t) : value(t) { OK = true; BAD = false; }
    optional_type(bool ok, std::string msg="") : msg(msg) { OK = ok; BAD = !ok; }
};

/////////////////////////////////////
//          decorators             //
/////////////////////////////////////

// exception decorator for optional return types
template<typename F>
constexpr auto exception_fail_safe(F func)  {
    return [func](auto&&... args) -> optional_type<decltype(func(args...))> {
        using R = optional_type<decltype(func(args...))>;

        try {
            return R(func(args...));
        } catch(std::iostream::failure& e) {
            return R(false, e.what());
        } catch(std::exception& e) {
            return R(false, e.what());
        } catch(...) {
            // This ... catch clause will capture any exception thrown
            return R(false, std::string("Exception caught: default exception"));
        }
    };
}

// this decorator can output our optional data
template<typename F>
constexpr auto output(F func) {
    return [func](auto&&... args) {
        auto opt = func(args...);
        
        if(opt.BAD) {
            std::cout << "There was an error: " << opt.msg << std::endl;
        } else {
            std::cout << "Bag cost $" << opt.value << std::endl;
        }

        return opt;
    };
}

// this decorator prints time and returns value of inner function
// returning is purely conditional based on our needs, in this case
// we want to take advantage of the functional-like syntax we've created
template<typename F>
constexpr auto log_time(F func) {
    return [func](auto&&... args) {
        auto now = std::chrono::system_clock::now();
        std::time_t time = std::chrono::system_clock::to_time_t(now); 
        auto opt = func(args...);
        std::cout << "> Logged at " << std::ctime(&time) << std::endl;

        return opt;
    };
}

////////////////////////////////////
//    visitor function            //
////////////////////////////////////

template<typename F>
constexpr auto classmethod(F func) {
    return [func](auto& a, auto&&... args) {
        return (a.*func)(args...);
    };
}

///////////////////////////////////////////////
//            function traits                //
///////////////////////////////////////////////

template<typename T> 
struct function_traits;  

// traits allows us to inspect type information from our function signature
template<typename R, typename... Args> 
struct function_traits<std::function<R(Args...)>>
{
    typedef R result_type;
    using args_pack = std::tuple<Args...>;
};

// base-case definition
// we want to be able to digest a lambda function and member functions
template<typename ClassType, typename RType, typename ArgsPack>
class class_memberfunc;

// specialization for variadic arguments
template<typename ClassType, typename RType, typename... Args>
class class_memberfunc<ClassType, RType, std::tuple<Args...>> {
    ClassType* self;
    std::function<RType(ClassType&, Args...)> f;

    public:
    class_memberfunc(ClassType* self) : self(self) {
    }

    class_memberfunc(const class_memberfunc& rhs) {
        this->self = rhs.self;
        this->f  = rhs.f;
    }

    template<typename F>
    void operator=(const F& rhs) {
        f = std::function<RType(ClassType&, Args...)>(rhs);
    }

    RType operator()(Args&&... args) {
        return f(*self, args...);
    }
};

// hide all the code-rewriting
template<typename Class>
class enable_memberfunc_traits {
    protected:
    /*
    define our member functor alias 
    use function traits to deduce types
    */
    template<typename Type>
    using memberfunc = class_memberfunc<Class, 
    typename function_traits<std::function<Type>>::result_type, 
    typename function_traits<std::function<Type>>::args_pack>;
};

///////////////////////////////////////////////
// an example class with a member function   //
///////////////////////////////////////////////

class apples : public enable_memberfunc_traits<apples> {
private:
    // member function that throws
    double calculate_cost_impl(int count, double weight) {
        if(count <= 0)
            throw std::runtime_error("must have 1 or more apples");
        
        if(weight <= 0)
            throw std::runtime_error("apples must weigh more than 0 ounces");

        return count*weight*cost_per_apple;
    }

    double cost_per_apple;

public:
    // ctor
    apples(double cost_per_apple) : 
        calculate_cost(memberfunc<optional_type<double>(int, double)>(this)), 
        cost_per_apple(cost_per_apple) { 
            // decorate our member function in ctor
            this->calculate_cost = log_time(output(exception_fail_safe(classmethod(&apples::calculate_cost_impl))));
        }

    ~apples() { }

    // define a functor with the same signature as our member function
    memberfunc<optional_type<double>(int, double)> calculate_cost;
};

////////////////////////////////////
//             main               //
////////////////////////////////////

int main() {
    using namespace std;

    apples groceries1(1.09);

    groceries1.calculate_cost(5, 3.34);

    return 0;
}
