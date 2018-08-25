#pragma once
#ifdef __cplusplus
extern "C" {
#endif

int tmr0_setup(uint32_t frequency, uint32_t delay, void *mr0handler, void *mr1handler);
void tmr0_mr1_start();

#ifdef __cplusplus
}
#endif
