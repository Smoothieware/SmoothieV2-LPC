#include "../Unity/src/unity.h"
#include "TestRegistry.h"

#include "Pin.h"

#include "FreeRTOS.h"
#include "task.h"

REGISTER_TEST(PinTest, flashleds)
{
	int cnt = 0;
	printf("defining pins...\n");
	Pin myleds[] = {
		Pin("GPIO3[12]"),
		Pin("GPIO3[13]"),
		Pin("GPIO3[15]"),
		Pin("GPIO3[14]"),
		Pin("GPIO3[10]"),
		Pin("GPIO5[3]"),
		Pin("P2_4") // Pin("GPIO5[4]")
	};

	Pin button("GPIO0_7!");
	button.as_input();
	if(!button.connected()) {
		printf("Button was invalid\n");
	}

	printf("set as outputs... \n");
	for(auto& p : myleds) {
		cnt++;
		if(p.as_output() == nullptr) {
			printf("Failed to allocate pin %d\n", cnt);
			TEST_FAIL();
		} else {
			p.set(false);
			printf("Set pin %d, GPIO%d[%d]\n", cnt, p.get_gpioport(), p.get_gpiopin());
		}
	}

	printf("Running...\n");

	TickType_t delayms= pdMS_TO_TICKS(100);
	cnt = 0;
	while(button.get() == 0) {
		uint8_t m = 1;
		for(auto& p : myleds) {
			p.set(cnt & m);
			m <<= 1;
		}
		cnt++;
		vTaskDelay(delayms);
	}
	printf("Done\n");
}
