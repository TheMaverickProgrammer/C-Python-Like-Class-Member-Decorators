# Preface
Read Part 1 - C++ function decorators [here](https://github.com/TheMaverickProgrammer/C-Python-like-Decorators)

In this second experiment, I re-imagine the way class member functions can be decorated. In the first article, I set out to achieve a close equivalent of python function decorators without the use of magic MACROS or mocs. Using purely C++ 14 we came up with a design pattern to accept arbitrary function inputs that return a closure function, allowing programmers to aggregate functions together and in compile-time!

# C++ Python-like Class Member Decorators

### Skip the tutorial and view the final results
---
[decorated member functor with private class implementation](https://godbolt.org/z/rwUVeG)

[dynamic member functor re-assignment](https://godbolt.org/z/ARNe-J)

# The goal
We left off with a demonstration that class member functions could also be decorated but I wasn't satisfied with the syntax. To refresh, we left with something that looked like this:

[goto godbolt](https://godbolt.org/z/5OzQZ9)

```cpp
// Different prices for different apples
apples groceries1(1.09), groceries2(3.0), groceries3(4.0);
auto get_cost = log_time(output(exception_fail_safe(visit_apples(&apples::calculate_cost))));

auto vec = { 
    get_cost(groceries2, 2, 1.1), 
    get_cost(groceries3, 5, 1.3), 
    get_cost(groceries1, 4, 0) 
};

```

Which is ugly and inconvenient if we want to pass objects around our system, we would need to have access to these decorators in each file and rewrite our code to use these decorators instead of the object's own methods.

I like python for its rich feature set and ease of use for the end programmer... but I have always been, and may always be, a C++ fanatic because of the speed, immediate control of memory, and strict typeness. If only there was a way to unite the two...

In the first article, I pointed out that python member functions could be reassigned on the fly and decorated which is impossible in C++.

If there was only a way these python features could make their way into C++, I'd be a happy guy. I like to have my cake and eat it too, don't you?

# Revisit the class visitor: @classmethod
By the end of the first article, we needed a way for our decorators to expect a class object in order to invoke a class member function. We wrote it to visit `apples` class objects and it looked like this:

```cpp
template<typename F>
auto visit_apples(F func) {
    return [func](apples& a, auto... args) {
        return (a.*func)(args...);
    };
}
```

In python, we have a similar decorator to _properly_ decorate member functions: `@classname`. This decorator specifically tells the interpreter to pass `self` into the decorator chain, if used, so that the member function can be called correctly- specifically in the event of inherited member functions. [Further reading on stackoverflow](https://stackoverflow.com/questions/3782040/python-decorators-that-are-part-of-a-base-class-cannot-be-used-to-decorate-membe)

We've done something similar earlier, we needed to pass the instance of the object into the decorator chain and with a quick re-write we can make this class visitor, universal.

Simply swap out `apples&` for `auto&`:

[goto godbolt](https://godbolt.org/z/S3NFjF)

```cpp
////////////////////////////////////
//    visitor function            //
////////////////////////////////////

template<typename F>
constexpr auto classmethod(F func) {
    return [func](auto& a, auto&&... args) {
        return (a.*func)(args...);
    };
}
```

# The member functor: Digesting Lambdas
We need to store a lambda. The C++ compiler does a good job hiding the implementation of lambdas, it's hard to think of them as anything else but magical. 

Lambdas are functors: a class object that has an overloaded `operator()` to behave like a function. The C++ standard already has a function utility header `<functional>` which provides a wrapper around any functor we throw at it.

Initially we could write 

```cpp
std::function<double(int, double)> f = get_cost;
```

But then we've not made any room to pass the class object for the `classmethod` decorator. 

We could re-write it a little 

```cpp
std::function<double(apples&, int, double)> f = get_cost;
```

and further generalize it by rewritting it as a class

```cpp
template<typename ClassType>
class class_methodfunc {
    std::function<double(ClassType&, int, double)> f;
    // ...
};
```

We need a way to allow our functor to accept any arbitrary input - just like our initial decorator problem from before.
Unlike last time, we need to know the exact type information at compile-time in order to store these decorators. Let's generalize further.

```cpp
template<typename ClassType, typename RType, typename... Args>
class class_memberfunc {
    std::function<RType(ClassType&, Args...)> f;

    // to accept any functor including lambdas and deeply-nested decorators, we use a single typename
    template<typename F>
    void operator=(const F& rhs) {
        f = std::function<RType(ClassType&, Args...)>(rhs);
    }

    RType operator()(ClassType& self, Args&&... args) {
        return f(self, args...);
    }
};
```

# The member functor: rewriting the class 
Let's start by using our new `class_memberfunc` to rewrite the `apples` class and provide a **private** member function that we want to decorate. Remember, we cannot reassign or modify a C++ function once it's written but we _can_ reassign a functor object.

First, let's add an _implicit_ type conversion in our rusty `optional_type<>` class:

```cpp
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
```

If we try to decorate member functions that do not return optional values, we will get compile errors. 

[goto godbolt](https://godbolt.org/z/FaaFmz)

```cpp
class apples {

private:
    // private member function implementation that throws
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
        cost_per_apple(cost_per_apple) { 
            // decorate our member function in ctor and store result in functor
            this->calculate_cost = log_time(output(exception_fail_safe(classmethod(&apples::calculate_cost_impl))));
        }

    ~apples() { }

    // define a functor with the same signature as our member function
    class_memberfunc<apples, optional_type<double>, int, double> calculate_cost;
};
```

We're getting there but we've come full circle with needing to provide the class object when we invoke the function with `operator()`:

```cpp
    groceries1.calculate_cost(groveries1, 5, 6.1); 
```

That's not very native at all. And we need to specify the return type and argument list by hand. 

# The member functor: function traits
Let's inspect the contents of the function signatures that we expect our member functor to take. We want to achieve the following syntax somehow:

```cpp
    // define a functor with the same signature as our member function
    memberfunc<optional_type<double>(int, double)> calculate_cost;
    
    ...
    
    groceries1.calculate_cost(5, 6.1); // implicit class object used! But how?
```

First, we need to somehow pull the argument list out of the function sig as well as the return type. We can create a templated structure that stores these types publically so we can query them at compile time:

```cpp
///////////////////////////////////////////////
//            function traits                //
///////////////////////////////////////////////

template<typename T> 
struct function_traits;  

// traits allows us to inspect type information from our functor signature
template<typename R, typename... Args> 
struct function_traits<std::function<R(Args...)>>
{
    typedef R result_type;
    using args_pack = std::tuple<Args...>;
};
```

This is a simple function-trait utlity structure. All we care about is the ability to deduce the return type and argument list from a `std::function<>` functor. Now we can write an alias to deduce the correct `class_memberfunction` signature.

```cpp
class apples {
    /*
    define our member functor alias 
    use function traits to deduce types
    */
    template<typename Type>
    using memberfunc = class_memberfunc<
      apples, 
      typename function_traits<std::function<Type>>::result_type, 
      typename function_traits<std::function<Type>>::args_pack
    >;
```

With this short-hand we don't need to type out everything explicitly. 

```cpp
    // define a functor with the same signature as our member function
    memberfunc<optional_type<double>(int, double)> calculate_cost;
```

This will come in handy once more later.

# The member functor: passing in self
Python implicity passes along the instance object of the class using the special keyword `self`. Let's pass in the C++ equivalent `this` in the constructor of our member functor and pass that along to the functor's `operator()`!

[goto godbolt](https://godbolt.org/z/8VABuv)

```cpp
// ctor
apples(double cost_per_apple) : 
    calculate_cost(memberfunc<optional_type<double>(int, double)>(this)), 
    cost_per_apple(cost_per_apple) { 
        // decorate our member function in ctor
        this->calculate_cost = log_time(output(exception_fail_safe(classmethod(&apples::calculate_cost_impl))));
    }
```

Now we're getting somewhere!

```cpp
    groceries1.calculate_cost(5, 6.1);
```

output is

```cpp
Bag cost $18.203

> Logged at Sun Aug 18 17:01:32 2019
```

# The member functor: wrapping it up
I'm all about typing less. As I get older, while I appreicate the verbose descriptors of Java functions and classes, carpel tunnel is right around the corner waiting to strike.

Let's hide the ugly alias in an enabler trait class and inherit from it instead

[goto godbolt](https://godbolt.org/z/nZds2g)

```cpp
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

class apples : public enable_memberfunc_traits<apples> {
    // ... omitted ...
    
    memberfunc<optional_type<double>(int, double)> calculate_cost;
};
```

# Dynamic re-assignment 
We did it! We have a private implementation we wanted to decorate, and it acts on `this` just like a member function! We wrote a `classmethod` universal visitor decorator and it's shaping up to be very flexible like Python. We've accomplished every goal for the member functors that we set out to achieve. There is one more tidbit I'd like to accomplish: in the previous article, Python allows us to dynamically re-assign functions.

wip

# Further Exploration
We could reduce typing by adding a function-trait specialization for the following syntax

```cpp
    // decltype yeilds T = double(apples*)(double, int)
    memberfunc<decltype(&apples::calculate_cost_impl)> calculate_cost;
```

This would further reduce writing and could be seen as a "promise" to the compiler that the functor will match the decorated member function. 

In short, there's more that can be explored with this concept. It has opened my eyes to new possibilities with C++ and I hope it has done the same for you too. 

Thank you for reading.
