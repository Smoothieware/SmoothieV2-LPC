#pragma once

#include "../Unity/src/unity.h"

#include <vector>
#include <tuple>

class TestBase;
class TestRegistry
{
public:
    TestRegistry(){};
    ~TestRegistry(){};

    // setup the Singleton instance
    static TestRegistry& instance()
    {
        static TestRegistry instance;
        return instance;
    }

    void add_test(TestBase *fnc, const char *name, const char *fn, int ln, bool setup_teardown)
    {
        test_fncs.push_back(std::make_tuple(fnc, name, ln, fn, setup_teardown));
    }

    std::vector<std::tuple<TestBase*, const char *, int, const char *, bool>>& get_tests() { return test_fncs; }

private:
    std::vector<std::tuple<TestBase*, const char *, int, const char *, bool>> test_fncs;

};

class TestBase
{
public:
    TestBase(const char *name, const char *file, int line, bool setup_teardown)
    {
        TestRegistry::instance().add_test(this, name, file, line, setup_teardown);
    }

	virtual void test() = 0;
	virtual void setUp(){};
	virtual void tearDown(){};
};

/**
 * Register a Single test with no fixtures or setup/teardown
 */
#define REGISTER_TEST(testCaseName, testName)\
  class testCaseName##testName##Test : public TestBase \
    { public: testCaseName##testName##Test() : TestBase(#testCaseName "-" #testName, __FILE__, __LINE__, false) {} \
    void test(void); } \
    testCaseName##testName##Instance; \
    void testCaseName##testName##Test::test(void)

/**
 * From easyunit.....

 * Define a test in a TestCase using test fixtures.
 * User should put his test code between brackets after using this macro.
 *
 * This macro should only be used if test fixtures were declared earlier in
 * this order: DECLARE, SETUP, TEARDOWN.
 * @param testCaseName TestCase name where the test belongs to. Should be
 * the same name of DECLARE, SETUP and TEARDOWN.
 * @param testName Unique test name.
 */
#define REGISTER_TESTF(testCaseName, testName)\
  class testCaseName##testName##Test : public testCaseName##Declare##Test \
        { public: testCaseName##testName##Test() : testCaseName##Declare##Test (#testCaseName "-" #testName, __FILE__, __LINE__) {} \
            void test(); } \
    testCaseName##testName##Instance; \
        void testCaseName##testName##Test::test ()


/**
 * Setup code for test fixtures.
 * This code is executed before each TESTF.
 *
 * User should put his setup code between brackets after using this macro.
 *
 * @param testCaseName TestCase name of the fixtures.
 */
#define TEST_SETUP(testCaseName)\
        void testCaseName##Declare##Test::setUp ()


/**
 * Teardown code for test fixtures.
 * This code is executed after each TESTF.
 *
 * User should put his setup code between brackets after using this macro.
 *
 * @param testCaseName TestCase name of the fixtures.
 */
#define TEST_TEARDOWN(testCaseName)\
        void testCaseName##Declare##Test::tearDown ()

/**
 * Location to declare variables and objets.
 * This is where user should declare members accessible by TESTF,
 * SETUP and TEARDOWN.
 *
 * User should not use brackets after using this macro. User should
 * not initialize any members here.
 *
 * @param testCaseName TestCase name of the fixtures
 * @see END_DECLARE for more information.
 */
#define TEST_DECLARE(testCaseName)\
        class testCaseName##Declare##Test : public TestBase \
        { public: testCaseName##Declare##Test(const char *name, const char *file, int line) : TestBase (name, file, line, true) {} \
        virtual void test() = 0; virtual void setUp(); virtual void tearDown(); \
        protected:


/**
 * Ending macro used after DECLARE.
 *
 * User should use this macro after declaring members with
 * DECLARE macro.
 */
#define TEST_END_DECLARE \
        };
