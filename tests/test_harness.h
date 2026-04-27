#ifndef CGS_TEST_HARNESS_H
#define CGS_TEST_HARNESS_H

#include <cmath>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

namespace cgs {
namespace testing {

struct AssertionFailure : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct TestCase {
    const char* name;
    std::function<void()> fn;
};

std::vector<TestCase>& registry();

struct Registrar {
    Registrar(const char* name, std::function<void()> fn) {
        registry().push_back({name, std::move(fn)});
    }
};

}}

#define CGS_CONCAT_INNER(a, b) a##b
#define CGS_CONCAT(a, b) CGS_CONCAT_INNER(a, b)

#define TEST_CASE(name)                                                    \
    static void CGS_CONCAT(test_fn_, __LINE__)();                          \
    static cgs::testing::Registrar CGS_CONCAT(test_reg_, __LINE__){        \
        name, &CGS_CONCAT(test_fn_, __LINE__)};                            \
    static void CGS_CONCAT(test_fn_, __LINE__)()

#define EXPECT_TRUE(cond)                                                  \
    do { if (!(cond)) throw cgs::testing::AssertionFailure(                \
        std::string("EXPECT_TRUE(" #cond ") at ") + __FILE__ + ":" +       \
        std::to_string(__LINE__)); } while (0)

#define EXPECT_EQ(a, b)                                                    \
    do { auto _a = (a); auto _b = (b);                                     \
         if (!(_a == _b)) throw cgs::testing::AssertionFailure(            \
            std::string("EXPECT_EQ failed at ") + __FILE__ + ":" +         \
            std::to_string(__LINE__)); } while (0)

#define EXPECT_NEAR(a, b, tol)                                             \
    do { double _a = (double)(a); double _b = (double)(b); double _t = (double)(tol); \
         if (std::fabs(_a - _b) > _t) throw cgs::testing::AssertionFailure(\
            std::string("EXPECT_NEAR failed at ") + __FILE__ + ":" +       \
            std::to_string(__LINE__)); } while (0)

#endif
