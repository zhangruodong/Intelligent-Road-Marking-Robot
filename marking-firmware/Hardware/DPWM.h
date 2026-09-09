#ifndef __DPWM_H
#define __DPWM_H

#include <stdint.h>

void TIM4_Init(void);
void DPWM1_Init(void);
void DPWM_SetCompare1(uint16_t Compare);

void DPWM2_Init(void);
void DPWM2_SetCompare2(uint16_t Compare);

#endif
