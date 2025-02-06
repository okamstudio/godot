#ifndef CPP_MOCK_H
#define CPP_MOCK_H

#include <algorithm>
#include <initializer_list>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <vector>
#include <functional>
#include "thirdparty/doctest/doctest.h"

namespace cpp_mock
{
    namespace details
    {
#if __cplusplus <= 201703L
        template <class T>
        struct remove_cvref
        {
            using type = typename std::remove_cv<
                typename std::remove_reference<T>::type>::type;
        };
#else
        using std::remove_cvref;
#endif

#if __cplusplus < 201703L
        template <class R, class Fn, class Arg>
        struct is_invocable_r : std::is_constructible<
            std::function<R(Arg)>,
            std::reference_wrapper<typename std::remove_reference<Fn>::type>>
        {
        };
#else
        using std::is_invocable_r;
#endif

        template <std::size_t... I>
        struct index_sequence
        {
            using type = index_sequence;
        };

        template <size_t N>
        struct make_index_sequence
        {
        };

        template<> struct make_index_sequence<0> : index_sequence<> { };
        template<> struct make_index_sequence<1> : index_sequence<0> { };
        template<> struct make_index_sequence<2> : index_sequence<0, 1> { };
        template<> struct make_index_sequence<3> : index_sequence<0, 1, 2> { };
        template<> struct make_index_sequence<4> : index_sequence<0, 1, 2, 3> { };
        template<> struct make_index_sequence<5> : index_sequence<0, 1, 2, 3, 4> { };
        template<> struct make_index_sequence<6> : index_sequence<0, 1, 2, 3, 4, 5> { };
        template<> struct make_index_sequence<7> : index_sequence<0, 1, 2, 3, 4, 5, 6> { };
        template<> struct make_index_sequence<8> : index_sequence<0, 1, 2, 3, 4, 5, 6, 7> { };
        template<> struct make_index_sequence<9> : index_sequence<0, 1, 2, 3, 4, 5, 6, 7, 8> { };
        template<> struct make_index_sequence<10> : index_sequence<0, 1, 2, 3, 4, 5, 6, 7, 8, 9> { };
    }

    namespace invoking
    {
        using namespace ::cpp_mock::details;

        template <class T>
        struct return_values
        {
            using storage_type = std::vector<T>;

            template <class... Args>
            T operator()(Args&&...)
            {
                if (index == values.size())
                {
                    return values.back();
                }
                else
                {
                    return values[index++];
                }
            }

            storage_type values;
            std::size_t index;
        };

        struct ignore_arg
        {
            explicit ignore_arg(const void*)
            {
            }

            template <class T>
            void save(const T&)
            {
            }
        };

        template <class T>
        struct save_arg
        {
            explicit save_arg(T* arg) :
                _arg(arg)
            {
            }

            void save(const T& value)
            {
                *_arg = value;
            }

            T* _arg;
        };

        template <class T, typename = void>
        struct sync_arg_save
        {
            using type = ignore_arg;
        };

        template <class T>
        struct sync_arg_save<T&, typename std::enable_if<!std::is_const<T>::value>::type>
        {
            using type = save_arg<T>;
        };

        template <std::size_t I, class Args, class ArgSetters>
        class sync_arg
        {
        public:
            using element_type = typename std::tuple_element<I, Args>::type;

            sync_arg(Args& args, ArgSetters& setters) :
                _args(&args),
                _setters(&setters)
            {
            }

            ~sync_arg()
            {
                std::get<I>(*_setters).save(get());
            }

            element_type& get()
            {
                return std::get<I>(*_args);
            }
        private:
            Args* _args;
            ArgSetters* _setters;
        };

        template <class R, class F, class Args, class ArgSetters, std::size_t... I>
        static R invoke_indexes(const F& func, Args& args, ArgSetters& setters, index_sequence<I...>)
        {
            return func(sync_arg<I, Args, ArgSetters>(args, setters).get()...);
        }

        template <class R, class F, class Args, class ArgSetters>
        static R invoke(F&& func, Args& args, ArgSetters& setters)
        {
            return invoke_indexes<R>(func, args, setters, typename make_index_sequence<std::tuple_size<Args>::value>::type{});
        }
    }

    namespace matching
    {
        using namespace ::cpp_mock::details;

        struct any_matcher
        {
            template <class T>
            bool operator()(const T&) const { return true; }
        };

        template <class T>
        class equals_matcher
        {
        public:
            equals_matcher(T&& value) :
                _expected(std::move(value))
            {
            }

            equals_matcher(const T& value) :
                _expected(value)
            {
            }

            bool operator()(const T& actual) const
            {
                return actual == _expected;
            }
        private:
            T _expected;
        };

        template <class Tuple>
        struct method_arguments_matcher
        {
            virtual bool matches(const Tuple& args) = 0;
            virtual ~method_arguments_matcher() {}
        };

        template <class Tuple>
        struct any_method_arguments_matcher : method_arguments_matcher<Tuple>
        {
            bool matches(const Tuple&) override
            {
                return true;
            }
        };

        template <class Tuple, class MatcherTuple>
        class match_arguments_wrapper : public method_arguments_matcher<Tuple>
        {
        public:
            explicit match_arguments_wrapper(MatcherTuple&& predicates) :
                _predicates(std::move(predicates))
            {
            }

            bool matches(const Tuple& args) override
            {
                return match_arguments(_predicates, args);
            }
        private:
            MatcherTuple _predicates;
        };

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#elif defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#elif defined(_MSC_VER)
#pragma warning( push )
#pragma warning( disable : 4505 )
#endif
        template <class Head>
        static bool and_together(Head value)
        {
            return value;
        }
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#elif defined(__clang__)
#pragma clang diagnostic pop
#elif defined(_MSC_VER)
#pragma warning( pop )
#endif

        template <class Head, class... Tail>
        static bool and_together(Head head, Tail... tail)
        {
            return head && and_together(tail...);
        }

        template <class P, class T, std::size_t... I>
        static bool match_argument_indexes(const P& predicates, const T& args, index_sequence<I...>)
        {
            return and_together(std::get<I>(predicates)(std::get<I>(args))...);
        }

        template <class P, class T>
        static bool match_arguments(const P& predicates, const T& args)
        {
            static_assert(
                std::tuple_size<P>::value == std::tuple_size<T>::value,
                "The number of predicates must match the number of arguments");

            return match_argument_indexes(predicates, args, typename make_index_sequence<std::tuple_size<T>::value>::type{});
        }
    }

    template <class R, class... Args>
    struct method_action
    {
        std::shared_ptr<matching::method_arguments_matcher<std::tuple<Args...>>> matcher;
        std::function<R(Args& ...)> action;
    };

    // static matching::any_matcher _;

    namespace mocking
    {
        using namespace ::cpp_mock::details;

        template <class Fn, class Arg>
        struct argument_matcher_wrapper :
            std::conditional<is_invocable_r<bool, Fn, Arg>::value, Fn, matching::equals_matcher<Arg>>
        {
        };

        template <class... TupleArgs>
        struct argument_wrapper
        {
            template <class... MatchArgs>
            struct match_with
            {
                using type = std::tuple<
                    typename argument_matcher_wrapper<MatchArgs, TupleArgs>::type...>;
            };
        };

        template <class R, class... Args>
        class method_action_builder
        {
        public:
            explicit method_action_builder(method_action<R, Args...>* action) :
                _action(action)
            {
            }

            method_action_builder(const method_action_builder&) = delete;
            method_action_builder(method_action_builder&&) = default;

            ~method_action_builder()
            {
                if (_action->action == nullptr)
                {
                    if (_values.empty())
                    {
                        _values.push_back(R());
                    }

                    _action->action = invoking::return_values<R> { std::move(_values), 0u };
                }

                if (_action->matcher == nullptr)
                {
                    _action->matcher = std::make_shared<
                        matching::any_method_arguments_matcher<std::tuple<Args...>>>();
                }
            }

            method_action_builder& operator=(const method_action_builder&) = delete;
            method_action_builder& operator=(method_action_builder&&) = default;

            template <class... MatchArgs>
            method_action_builder& With(MatchArgs&&... args)
            {
                static_assert(
                    sizeof...(MatchArgs) == sizeof...(Args),
                    "The number of matchers must match the number of arguments");

                using matcher_tuple = typename argument_wrapper<Args...>
                    ::template match_with<MatchArgs...>::type;

                _action->matcher = std::make_shared<
                    matching::match_arguments_wrapper<std::tuple<Args...>, matcher_tuple>>(
                        matcher_tuple(std::forward<MatchArgs>(args)...));

                return *this;
            }

            method_action_builder& Do(std::function<R(Args& ...)> function)
            {
                _action->action = std::move(function);
                return *this;
            }

            method_action_builder& Return(const R& value)
            {
                _values.push_back(value);
                return *this;
            }

            method_action_builder& Return(std::initializer_list<R> values)
            {
                for (auto& value : values)
                {
                    _values.push_back(value);
                }

                return *this;
            }

        private:
            method_action<R, Args...>* _action;
            typename invoking::return_values<R>::storage_type _values;
        };

        template <class... Args>
        class method_action_builder<void, Args...>
        {
        public:
            explicit method_action_builder(method_action<void, Args...>* action) :
                _action(action)
            {
            }

            method_action_builder(const method_action_builder&) = delete;
            method_action_builder(method_action_builder&&) = default;

            ~method_action_builder()
            {
                if (_action->action == nullptr)
                {
                    _action->action = [](const Args& ...) { };
                }

                if (_action->matcher == nullptr)
                {
                    _action->matcher = std::make_shared<
                        matching::any_method_arguments_matcher<std::tuple<Args...>>>();
                }
            }

            method_action_builder& operator=(const method_action_builder&) = delete;
            method_action_builder& operator=(method_action_builder&&) = default;

            template <class... MatchArgs>
            method_action_builder& With(MatchArgs&&... args)
            {
                static_assert(
                    sizeof...(MatchArgs) == sizeof...(Args),
                    "The number of matchers must match the number of arguments");

                using matcher_tuple = typename argument_wrapper<Args...>
                    ::template match_with<MatchArgs...>::type;

                _action->matcher = std::make_shared<
                    matching::match_arguments_wrapper<std::tuple<Args...>, matcher_tuple>>(
                        matcher_tuple(std::forward<MatchArgs>(args)...));

                return *this;
            }

            method_action_builder& Do(std::function<void(Args & ...)> function)
            {
                _action->action = std::move(function);
                return *this;
            }

        private:
            method_action<void, Args...>* _action;
        };

        template <class... Args>
        class method_verify_builder
        {
        public:
            method_verify_builder(
                const char* method,
                const char* file,
                std::size_t line,
                const std::vector<std::tuple<Args...>>* calls
            ) :
                _calls(calls),
                _count(std::numeric_limits<std::size_t>::max()),
                _matched_count(calls->size()),
                _method(method),
                _file(file),
                _line(line)
            {
            }

            method_verify_builder(const method_verify_builder&) = delete;
            method_verify_builder(method_verify_builder&&) = default;

            ~method_verify_builder() noexcept(false)
            {
                if (_count == std::numeric_limits<std::size_t>::max())
                {
                    if (_matched_count == 0)
                    {
                        std::ostringstream stream;
                        write_location(stream) << "Expecting a call to "
                            << _method << " but none were received.";
                        FAIL(stream.str());
                    }
                }
                else if (_count != _matched_count)
                {
                    std::ostringstream stream;
                    write_location(stream) << "Expecting a call to " << _method << ' ';
                    write_times(stream, _count) << ", but it was invoked ";
                    write_times(stream, _matched_count) << '.';
                    FAIL(stream.str());
                }
            }

            method_verify_builder& operator=(const method_verify_builder&) = delete;
            method_verify_builder& operator=(method_verify_builder&&) = default;

            template <class... MatchArgs>
            method_verify_builder& With(MatchArgs&&... matchers)
            {
                static_assert(
                    sizeof...(MatchArgs) == sizeof...(Args),
                    "The number of matchers must match the number of arguments");

                using argument_tuple = std::tuple<Args...>;
                using matcher_tuple = typename argument_wrapper<Args...>
                    ::template match_with<MatchArgs...>::type;

                matcher_tuple matcher(std::forward<MatchArgs>(matchers)...);
                _matched_count = std::count_if(
                    _calls->begin(),
                    _calls->end(),
                    [&](const argument_tuple& args) { return match_arguments(matcher, args); });

                return *this;
            }

            method_verify_builder& Times(std::size_t count)
            {
                _count = count;
                return *this;
            }
        private:
            std::ostream& write_location(std::ostream& stream)
            {
                return stream << _file << ':' << _line << ' ';
            }

            std::ostream& write_times(std::ostream& stream, std::size_t count)
            {
                return stream << count << " time" << ((count == 1) ? "" : "s");
            }

            const std::vector<std::tuple<Args...>>* _calls;
            std::size_t _count;
            std::size_t _matched_count;
            const char* _method;
            const char* _file;
            std::size_t _line;
        };

        template <class>
        class mock_method_types;

        template <class R, class... Args>
        class mock_method_types<R(Args...)>
        {
        public:
            using method_action_t = method_action<R, typename remove_cvref<Args>::type ...>;
            using tuple_t = typename std::tuple<typename remove_cvref<Args>::type ...>;

            using action_t = typename std::vector<method_action_t>;
            using record_t = typename std::vector<tuple_t>;
        };

        template <class R, class... Args>
        static method_action_builder<R, Args...> add_action(std::vector<method_action<R, Args...>>& actions)
        {
            actions.emplace_back(method_action<R, Args...>());
            return method_action_builder<R, Args...>(&actions.back());
        }

        template <class... Args>
        static method_verify_builder<Args...> check_action(
            const char* method,
            const char* file,
            std::size_t line,
            const std::vector<std::tuple<Args...>>& invocations)
        {
            return method_verify_builder<Args...>(method, file, line, &invocations);
        }

        template <class T>
        static T return_default()
        {
            return T();
        }
    }
}

// Required for VC++ to expand variadic macro arguments correctly
#define EXPAND_MACRO(macro, ...) macro

#define MAKE_FORWARD(arg) std::forward<decltype(arg)>(arg)
#define MAKE_SETTER(arg) ::cpp_mock::invoking::sync_arg_save<decltype(arg)>::type(&arg)

#define MOCK_METHOD_IMPL(Ret, Name, Args, Specs, NameArgs, Transform, ...) \
    Ret Name(NameArgs Args) Specs \
    { \
        Name ## _Invocations.emplace_back(std::make_tuple( \
            EXPAND_MACRO(Transform(MAKE_FORWARD, __VA_ARGS__)))); \
        auto& args = Name ## _Invocations.back(); \
        for (auto it = Name ## _Actions.rbegin(); it != Name ## _Actions.rend(); ++it) \
        { \
            if (it->matcher->matches(args)) \
            { \
                auto setters = std::make_tuple( \
                    EXPAND_MACRO(Transform(MAKE_SETTER, __VA_ARGS__))); \
                return ::cpp_mock::invoking::invoke<Ret>(it->action, args, setters); \
            } \
        } \
        return ::cpp_mock::mocking::return_default<Ret>(); \
    } \
    ::cpp_mock::mocking::mock_method_types<Ret Args>::action_t Name ## _Actions; \
    mutable ::cpp_mock::mocking::mock_method_types<Ret Args>::record_t Name ## _Invocations;

#define NAME_ARGS1(type) type a
#define NAME_ARGS2(type, ...) type b, EXPAND_MACRO(NAME_ARGS1(__VA_ARGS__))
#define NAME_ARGS3(type, ...) type c, EXPAND_MACRO(NAME_ARGS2(__VA_ARGS__))
#define NAME_ARGS4(type, ...) type d, EXPAND_MACRO(NAME_ARGS3(__VA_ARGS__))
#define NAME_ARGS5(type, ...) type e, EXPAND_MACRO(NAME_ARGS4(__VA_ARGS__))
#define NAME_ARGS6(type, ...) type f, EXPAND_MACRO(NAME_ARGS5(__VA_ARGS__))
#define NAME_ARGS7(type, ...) type g, EXPAND_MACRO(NAME_ARGS6(__VA_ARGS__))
#define NAME_ARGS8(type, ...) type h, EXPAND_MACRO(NAME_ARGS7(__VA_ARGS__))
#define NAME_ARGS9(type, ...) type i, EXPAND_MACRO(NAME_ARGS8(__VA_ARGS__))
#define NAME_ARGS10(type, ...) type j, EXPAND_MACRO(NAME_ARGS9(__VA_ARGS__))

#define TRANSFORM1(Call, x) Call(x)
#define TRANSFORM2(Call, x, ...) Call(x), EXPAND_MACRO(TRANSFORM1(Call, __VA_ARGS__))
#define TRANSFORM3(Call, x, ...) Call(x), EXPAND_MACRO(TRANSFORM2(Call, __VA_ARGS__))
#define TRANSFORM4(Call, x, ...) Call(x), EXPAND_MACRO(TRANSFORM3(Call, __VA_ARGS__))
#define TRANSFORM5(Call, x, ...) Call(x), EXPAND_MACRO(TRANSFORM4(Call, __VA_ARGS__))
#define TRANSFORM6(Call, x, ...) Call(x), EXPAND_MACRO(TRANSFORM5(Call, __VA_ARGS__))
#define TRANSFORM7(Call, x, ...) Call(x), EXPAND_MACRO(TRANSFORM6(Call, __VA_ARGS__))
#define TRANSFORM8(Call, x, ...) Call(x), EXPAND_MACRO(TRANSFORM7(Call, __VA_ARGS__))
#define TRANSFORM9(Call, x, ...) Call(x), EXPAND_MACRO(TRANSFORM8(Call, __VA_ARGS__))
#define TRANSFORM10(Call, x, ...) Call(x), EXPAND_MACRO(TRANSFORM9(Call, __VA_ARGS__))

// We need to handle () and (int) differently, however, when they get passed in
// via __VA_ARG__ then it looks like we received a single parameter for both
// cases. Use the fact that we're always appending an 'a' to the parameter to
// detect the difference between 'type a' and 'a'
#define CAT(a, b) a ## b
#define GET_SECOND_ARG(a, b, ...) b
#define DEFINE_EXISTS(...) EXPAND_MACRO(GET_SECOND_ARG(__VA_ARGS__, IS_TRUE))
#define IS_TYPE_MISSING(x) DEFINE_EXISTS(CAT(TOKEN_IS_EMPTY_, x))
#define TOKEN_IS_EMPTY_a ignored, IS_FALSE
#define GET_METHOD(method, suffix) CAT(method, suffix)
#define NOOP(...)
#define MOCK_METHOD_SINGLE_IS_FALSE(Ret, Name, Args, Specs) MOCK_METHOD_IMPL(Ret, Name, Args, Specs, NOOP, NOOP)
#define MOCK_METHOD_SINGLE_IS_TRUE(Ret, Name, Args, Specs) MOCK_METHOD_IMPL(Ret, Name, Args, Specs, NAME_ARGS1, TRANSFORM1, a)
#define HANDLE_EMPTY_TYPE(...) GET_METHOD(MOCK_METHOD_SINGLE_, IS_TYPE_MISSING(__VA_ARGS__ a))

#define MOCK_METHOD_1(Ret, Name, Args, Specs) HANDLE_EMPTY_TYPE Args (Ret, Name, Args, Specs)
#define MOCK_METHOD_2(Ret, Name, Args, Specs) MOCK_METHOD_IMPL(Ret, Name, Args, Specs, NAME_ARGS2, TRANSFORM2, b, a)
#define MOCK_METHOD_3(Ret, Name, Args, Specs) MOCK_METHOD_IMPL(Ret, Name, Args, Specs, NAME_ARGS3, TRANSFORM3, c, b, a)
#define MOCK_METHOD_4(Ret, Name, Args, Specs) MOCK_METHOD_IMPL(Ret, Name, Args, Specs, NAME_ARGS4, TRANSFORM4, d, c, b, a)
#define MOCK_METHOD_5(Ret, Name, Args, Specs) MOCK_METHOD_IMPL(Ret, Name, Args, Specs, NAME_ARGS5, TRANSFORM5, e, d, c, b, a)
#define MOCK_METHOD_6(Ret, Name, Args, Specs) MOCK_METHOD_IMPL(Ret, Name, Args, Specs, NAME_ARGS6, TRANSFORM6, f, e, d, c, b, a)
#define MOCK_METHOD_7(Ret, Name, Args, Specs) MOCK_METHOD_IMPL(Ret, Name, Args, Specs, NAME_ARGS7, TRANSFORM7, g, f, e, d, c, b, a)
#define MOCK_METHOD_8(Ret, Name, Args, Specs) MOCK_METHOD_IMPL(Ret, Name, Args, Specs, NAME_ARGS8, TRANSFORM8, h, g, f, e, d, c, b, a)
#define MOCK_METHOD_9(Ret, Name, Args, Specs) MOCK_METHOD_IMPL(Ret, Name, Args, Specs, NAME_ARGS9, TRANSFORM9, i, h, g, f, e, d, c, b, a)
#define MOCK_METHOD_10(Ret, Name, Args, Specs) MOCK_METHOD_IMPL(Ret, Name, Args, Specs, NAME_ARGS10, TRANSFORM10, j, i, h, g, f, e, d, c, b, a)

#define GET_NTH_ARG(a, b, c, d, e, f, g, h, i, j, N, ...) N

#define GET_MOCK_METHOD(...) EXPAND_MACRO(GET_NTH_ARG(__VA_ARGS__, \
    MOCK_METHOD_10, \
    MOCK_METHOD_9, \
    MOCK_METHOD_8, \
    MOCK_METHOD_7, \
    MOCK_METHOD_6, \
    MOCK_METHOD_5, \
    MOCK_METHOD_4, \
    MOCK_METHOD_3, \
    MOCK_METHOD_2, \
    MOCK_METHOD_1))

#define INVALID_METHOD(...) static_assert(false, "Invalid usage. Call with return type, name, argument types and, optionally, specifiers.");
#define MOCK_METHOD_SPEC(Ret, Name, Args, Spec) GET_MOCK_METHOD Args (Ret, Name, Args, Spec)
#define MOCK_METHOD(Ret, Name, Args) MOCK_METHOD_SPEC(Ret, Name, Args, override)

#define MockMethod(...) EXPAND_MACRO(EXPAND_MACRO(GET_NTH_ARG(__VA_ARGS__, \
    INVALID_METHOD, \
    INVALID_METHOD, \
    INVALID_METHOD, \
    INVALID_METHOD, \
    INVALID_METHOD, \
    INVALID_METHOD, \
    MOCK_METHOD_SPEC, \
    MOCK_METHOD, \
    INVALID_METHOD, \
    INVALID_METHOD))(__VA_ARGS__))

#define MockConstMethod(Ret, Name, Args) MockMethod(Ret, Name, Args, const override)
#define When(call_method) ::cpp_mock::mocking::add_action(call_method ## _Actions)
#define Verify(call_method) ::cpp_mock::mocking::check_action(#call_method, __FILE__, __LINE__, call_method ## _Invocations)

#endif
