#ifndef __USART_H
#define __USART_H

#include <stdio.h>
//PA9  PA10
uint8_t Serial1_GetRxFlag(void);
uint8_t Serial1_GetRxData(void);
void Serial1_Init(void);
void Serial1_SendByte(uint8_t Byte);
void Serial1_SendNumber(uint32_t Number, uint8_t Length);

//PA2 PA3
uint8_t Serial2_GetRxFlag(void);
uint8_t Serial2_GetRxData(void);
void Serial2_Init(void);
void Serial2_SendByte(uint8_t Byte);
void Serial2_SendNumber(uint32_t Number, uint8_t Length);

//PB10  PB11
uint8_t Serial3_GetRxFlag(void);
uint8_t Serial3_GetRxData(void);
void Serial3_Init(void);
void Serial3_SendByte(uint8_t Byte);
void Serial3_SendNumber(uint32_t Number, uint8_t Length);

#endif
