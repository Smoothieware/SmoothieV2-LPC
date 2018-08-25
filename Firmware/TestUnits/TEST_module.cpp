#include "../Unity/src/unity.h"
#include "TestRegistry.h"

#include "Module.h"

#include <string.h>

static bool g_on_halt = false;
static char g_request_key[32] = "";

class TestSingleModule : public Module
{
public:
    TestSingleModule(const char *name) : Module(name) {};
    void on_halt(bool flg) { g_on_halt = flg; }
    bool request(const char *key, void *value) { strcpy(g_request_key, key); *(int *)value = 1234; return true; }
};


REGISTER_TEST(Module, single_module)
{
    TestSingleModule mod("module1");
    TEST_ASSERT_TRUE(mod.was_added());

    TEST_ASSERT_NULL(Module::lookup("xxxx"));
    TEST_ASSERT_NOT_NULL(Module::lookup("module1"));
    TEST_ASSERT_TRUE(Module::lookup_group("module1").empty());

    TEST_ASSERT_FALSE(g_on_halt);
    Module::broadcast_halt(true);
    TEST_ASSERT_TRUE(g_on_halt);
    Module::broadcast_halt(false);
    TEST_ASSERT_FALSE(g_on_halt);

    int ret = 0;
    bool ok = Module::lookup("module1")->request("testkey", &ret);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("testkey", g_request_key);
    TEST_ASSERT_EQUAL_INT(1234, ret);
}

REGISTER_TEST(Module, single_module_destructed)
{
    TEST_ASSERT_NULL(Module::lookup("module1"));
}

static std::map<std::string, bool> g_halt_map;
class TestMultiModule : public Module
{
public:
    TestMultiModule(const char *group, const char *name) : Module(group, name) {};
    void on_halt(bool flg) { g_halt_map[instance_name] = flg; }
    bool request(const char *key, void *value)
    {
        if(strcmp(key, "getname") == 0) {
            *((std::string *)value) = instance_name;
            return true;
        } else {
            return false;
        }
    }
};

REGISTER_TEST(Module, multi_module)
{
    TestMultiModule mod1("group1", "i1");
    TestMultiModule mod2("group1", "i2");

    TEST_ASSERT_TRUE(mod1.was_added());
    TEST_ASSERT_TRUE(mod2.was_added());

    TEST_ASSERT_NULL(Module::lookup("group1"));

    Module *m1= Module::lookup("group1", "i1");
    TEST_ASSERT_NOT_NULL(m1);
    Module *m2= Module::lookup("group1", "i2");
    TEST_ASSERT_NOT_NULL(m2);

    // if we do not have longjmp it will continue so we short circuit here to stop a crash further on
    //if(m1 == nullptr || m2 == nullptr) return;

    TEST_ASSERT_NULL(Module::lookup("group1", "i3"));

    TEST_ASSERT_TRUE(Module::lookup_group("group1").size() == 2);
    TEST_ASSERT_EQUAL_PTR(m1, Module::lookup_group("group1")[0]);
    TEST_ASSERT_EQUAL_PTR(m2, Module::lookup_group("group1")[1]);

    TEST_ASSERT_TRUE(g_halt_map.empty());
    Module::broadcast_halt(true);
    TEST_ASSERT_TRUE(g_halt_map["i1"]);
    TEST_ASSERT_TRUE(g_halt_map["i2"]);
    Module::broadcast_halt(false);
    TEST_ASSERT_FALSE(g_halt_map["i1"]);
    TEST_ASSERT_FALSE(g_halt_map["i2"]);

    bool ok;
    std::string ret;
    ok= m1->request("xxx", &ret);
    TEST_ASSERT_FALSE(ok);
    ok = m1->request("getname", &ret);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("i1", ret.c_str());

    ok = m2->request("getname", &ret);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("i2", ret.c_str());
}
