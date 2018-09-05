#pragma once
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

void add_to_xmodem_inbuff(char c);
void init_xmodem(void (*tx)(char c));
void deinit_xmodem();
int xmodemReceive(FILE *fp);

#ifdef __cplusplus
}
#endif
