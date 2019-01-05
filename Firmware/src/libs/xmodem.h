#pragma once
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

void add_to_xmodem_inbuff(char c);
int init_xmodem(void (*tx)(char c));
void deinit_xmodem();
int xmodemReceive(FILE *fp);
int ymodemReceive();

#ifdef __cplusplus
}
#endif
