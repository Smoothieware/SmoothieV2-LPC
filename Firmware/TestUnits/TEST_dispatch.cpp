#include "GCode.h"
#include "GCodeProcessor.h"
#include "Dispatcher.h"

#include <sstream>
#include "OutputStream.h"

#include "../Unity/src/unity.h"
#include "TestRegistry.h"

// just swaps the parameters
#define TEST_ASSERT_STRING_S(a, b) TEST_ASSERT_EQUAL_STRING(b, a)

Dispatcher *dispatcher= new Dispatcher;

TEST_DECLARE(Dispatcher)
    bool cb1;
    bool cb2;
    bool cb3;
    GCodeProcessor gp;
    GCodeProcessor::GCodes_t gcodes;
    Dispatcher::Handlers_t::iterator h3;
    GCode::Args_t args;
TEST_END_DECLARE

TEST_SETUP(Dispatcher)
{
    cb1= false;
    cb2= false;
    cb3= false;
    auto fnc1= [this](GCode& gc, OutputStream& os) { args= gc.get_args(); cb1= true; return true; };
    auto fnc2= [this](GCode& gc, OutputStream& os) { cb2= true; return true; };
    auto fnc3= [this](GCode& gc, OutputStream& os) { cb3= true; return true; };
    THEDISPATCHER->clear_handlers();
    THEDISPATCHER->add_handler(Dispatcher::GCODE_HANDLER, 1, fnc1);
    THEDISPATCHER->add_handler(Dispatcher::MCODE_HANDLER, 1, fnc2);
    h3= THEDISPATCHER->add_handler(Dispatcher::GCODE_HANDLER, 1, fnc3);
    gcodes.clear();
    bool ok= gp.parse("G1 X1 Y2 M1 G4 S10", gcodes);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_INT(3, gcodes.size());
    args.clear();
}

TEST_TEARDOWN(Dispatcher)
{
    THEDISPATCHER->clear_handlers();
}

REGISTER_TESTF(Dispatcher, check_callbacks)
{
    TEST_ASSERT_FALSE(cb1);
    TEST_ASSERT_FALSE(cb2);
    TEST_ASSERT_FALSE(cb3);

    std::ostringstream oss;
    OutputStream os(&oss);
    TEST_ASSERT_TRUE(THEDISPATCHER->dispatch(gcodes[0], os));
    TEST_ASSERT_STRING_S(oss.str().c_str(), "ok\n");

    TEST_ASSERT_TRUE( cb1 );
    TEST_ASSERT_FALSE(cb2);
    TEST_ASSERT_TRUE( cb3 );
    TEST_ASSERT_EQUAL_INT(2, args.size());
    TEST_ASSERT_EQUAL_INT(1, args['X']);
    TEST_ASSERT_EQUAL_INT(2, args['Y']);

    oss.str("");
    TEST_ASSERT_TRUE(THEDISPATCHER->dispatch(gcodes[1], os));
    TEST_ASSERT_STRING_S(oss.str().c_str(), "ok\n");
    TEST_ASSERT_TRUE( cb2 );
    oss.str("");
    TEST_ASSERT_FALSE(THEDISPATCHER->dispatch(gcodes[2], os));
    TEST_ASSERT_EQUAL_INT(0, oss.str().size());
}

REGISTER_TESTF(Dispatcher, Remove_second_G1_handler)
{
    TEST_ASSERT_FALSE(cb1);
    TEST_ASSERT_FALSE(cb2);
    TEST_ASSERT_FALSE(cb3);

    THEDISPATCHER->remove_handler(Dispatcher::GCODE_HANDLER, h3);
    OutputStream os; // NULL output stream
    TEST_ASSERT_TRUE(THEDISPATCHER->dispatch(gcodes[0], os));
    TEST_ASSERT_TRUE ( cb1 );
    TEST_ASSERT_FALSE ( cb3 );
}

REGISTER_TESTF(Dispatcher, one_off_dispatch)
{
    std::ostringstream oss;
    OutputStream os(&oss);

    TEST_ASSERT_FALSE(cb1);
    TEST_ASSERT_TRUE(args.empty());
    THEDISPATCHER->dispatch(os, 'G', 1, 'X', 456.0, 'Y', -789.0, 'Z', 123.0, 0);
    TEST_ASSERT_STRING_S(oss.str().c_str(), "ok\n");

    TEST_ASSERT_TRUE ( cb1 );
    //for(auto &i : args) printf("%c: %f\n", i.first, i.second);

    TEST_ASSERT_EQUAL_INT(3, args.size());
    TEST_ASSERT_EQUAL_INT(456, args['X']);
    TEST_ASSERT_EQUAL_INT(-789, args['Y']);
    TEST_ASSERT_EQUAL_INT(123, args['Z']);
}
