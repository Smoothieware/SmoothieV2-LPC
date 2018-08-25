#include "PlannerQueue.h"
#include "Block.h"

#include "../Unity/src/unity.h"
#include "TestRegistry.h"

REGISTER_TEST(PlannerQueue,basic)
{
    PlannerQueue rb(10);
    TEST_ASSERT_TRUE(rb.empty());
    TEST_ASSERT_FALSE(rb.full());

    // this always returns something
    TEST_ASSERT_TRUE(rb.get_head() != nullptr);

    // add one entry
    TEST_ASSERT_TRUE(rb.queue_head());
    TEST_ASSERT_FALSE(rb.empty());
    TEST_ASSERT_FALSE(rb.full());

    // add 8 more entries
    for (int i = 2; i <= 9; ++i) {
        TEST_ASSERT_TRUE(rb.queue_head());
        if(i < 9) TEST_ASSERT_FALSE(rb.full())
        else TEST_ASSERT_TRUE(rb.full());
    }

    TEST_ASSERT_FALSE(rb.empty());
    TEST_ASSERT_FALSE(rb.queue_head());
    TEST_ASSERT_TRUE(rb.full());

    // remove 8 entries
    for (int i = 1; i <= 8; ++i) {
        TEST_ASSERT_FALSE(rb.empty());
        Block *b= rb.get_tail();
        TEST_ASSERT_TRUE(b != nullptr);
        rb.release_tail();
        TEST_ASSERT_FALSE(rb.full());
    }

    // one left
    TEST_ASSERT_FALSE(rb.empty());
    TEST_ASSERT_TRUE(rb.get_tail() != nullptr);

    // release it
    rb.release_tail();

    // none left
    TEST_ASSERT_TRUE(rb.empty());
    TEST_ASSERT_TRUE(rb.get_tail() == nullptr);
}

REGISTER_TEST(PlannerQueue,iteration)
{
    PlannerQueue rb(10);
	TEST_ASSERT_TRUE(rb.empty());
    TEST_ASSERT_FALSE(rb.full());

    // add 4 entries
    for (int i = 1; i <= 4; ++i) {
        Block *b= rb.get_head();
        b->steps_event_count= i;
        TEST_ASSERT_TRUE(rb.queue_head());
    }

	TEST_ASSERT_FALSE(rb.empty());

    rb.start_iteration();
    TEST_ASSERT_TRUE(rb.is_at_head());
    TEST_ASSERT_FALSE(rb.is_at_tail());

    // 1
    Block *b= rb.tailward_get();
    TEST_ASSERT_EQUAL_INT(4, b->steps_event_count);
    TEST_ASSERT_FALSE(rb.is_at_head());
    TEST_ASSERT_FALSE(rb.is_at_tail());

    // 2
    b= rb.tailward_get();
    TEST_ASSERT_EQUAL_INT(3, b->steps_event_count);
    TEST_ASSERT_FALSE(rb.is_at_head());
    TEST_ASSERT_FALSE(rb.is_at_tail());

    // 3
    b= rb.tailward_get();
    TEST_ASSERT_EQUAL_INT(2, b->steps_event_count);
    TEST_ASSERT_FALSE(rb.is_at_head());
    TEST_ASSERT_FALSE(rb.is_at_tail());

    // 4
    b= rb.tailward_get();
    TEST_ASSERT_EQUAL_INT(1, b->steps_event_count);
    TEST_ASSERT_FALSE(rb.is_at_head());
    TEST_ASSERT_TRUE(rb.is_at_tail());

	// 3
    b= rb.headward_get();
    TEST_ASSERT_EQUAL_INT(2, b->steps_event_count);
    TEST_ASSERT_FALSE(rb.is_at_head());
    TEST_ASSERT_FALSE(rb.is_at_tail());

	// 2
    b= rb.headward_get();
    TEST_ASSERT_EQUAL_INT(3, b->steps_event_count);
    TEST_ASSERT_FALSE(rb.is_at_head());
    TEST_ASSERT_FALSE(rb.is_at_tail());

	// 1
	b= rb.headward_get();
    TEST_ASSERT_EQUAL_INT(4, b->steps_event_count);
	TEST_ASSERT_FALSE(rb.is_at_head());
	TEST_ASSERT_FALSE(rb.is_at_tail());

	// back at head
	b= rb.headward_get();
	TEST_ASSERT_TRUE(rb.is_at_head());
}
