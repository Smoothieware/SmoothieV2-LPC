#pragma once
#ifdef __cplusplus
extern "C" {
#endif

// Setup where frequency is in Hz, delay is in microseconds
int tmr0_setup(uint32_t frequency, uint32_t delay, void *mr0handler, void *mr1handler);
void tmr0_mr1_start();
void tmr0_stop();

// setup where frequency is in Hz
int tmr1_setup(uint32_t frequency, void *timer_handler);
void tmr1_stop();
int tmr1_set_frequency(uint32_t frequency);

#ifdef __cplusplus
}
#endif
