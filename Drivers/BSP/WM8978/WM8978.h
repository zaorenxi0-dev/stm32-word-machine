#ifndef WM8978_H
#define WM8978_H

#include "./SYSTEM/sys/sys.h"

uint8_t WM8978_Write_REG(uint8_t add,uint16_t byte);
uint8_t WM8978_Init();
uint16_t WM8978_Read_Reg(uint8_t reg);
void WM9878_Set_3D(uint16_t penth3d);
void WM8978_Speaker_Volume(uint8_t value);
void WM8978_Phones_Volume(uint8_t value);
void WM8978_Set_MODE(uint8_t fmt,uint8_t wl);
void WM8978_ADDA_Cfg(uint8_t dacen,uint8_t adcen);
void WM8978_Output_Cfg(uint8_t dacen,uint8_t bpsen);
#endif

