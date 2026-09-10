#ifndef __USART_H
#define __USART_H

#include <stdio.h>
//PA9  PA10
uint8_t Serial1_TakeRxData(uint8_t *out);   // 标志位和数据一起取，原子，见 USART.c
void Serial1_Init(void);
void Serial1_SendByte(uint8_t Byte);
void Serial1_SendNumber(uint32_t Number, uint8_t Length);

//PA2 PA3
uint8_t Serial2_TakeRxData(uint8_t *out);
void Serial2_Init(void);
void Serial2_SendByte(uint8_t Byte);
void Serial2_SendNumber(uint32_t Number, uint8_t Length);

#endif
