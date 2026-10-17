#ifndef IIS_H
#define IIS_H

#include "./SYSTEM/sys/sys.h"

#define IIS_WS_CLK()	do{__HAL_RCC_GPIOB_CLK_ENABLE();} while(0)
#define IIS_WS_PORT		GPIOB
#define IIS_WS_PIN		GPIO_PIN_12

#define IIS_CK_CLK()	do{__HAL_RCC_GPIOB_CLK_ENABLE();} while(0)
#define IIS_CK_PORT		GPIOB
#define IIS_CK_PIN		GPIO_PIN_13

#define IIS_MCK_CLK()	do{__HAL_RCC_GPIOC_CLK_ENABLE();} while(0)
#define IIS_MCK_PORT	GPIOC
#define IIS_MCK_PIN		GPIO_PIN_6

#define IIS_SD_CLK()	do{__HAL_RCC_GPIOC_CLK_ENABLE();} while(0)
#define IIS_SD_PORT		GPIOC
#define IIS_SD_PIN		GPIO_PIN_3

#define IIS_ext_SD_CLK()	do{__HAL_RCC_GPIOC_CLK_ENABLE();} while(0)
#define IIS_ext_SD_PORT		GPIOC
#define IIS_ext_SD_PIN		GPIO_PIN_2





void MX_I2S2_Init(uint32_t Mode,uint32_t Standard,uint32_t DataFormat,uint32_t CPOL);
void HAL_I2S_MspInit(I2S_HandleTypeDef* i2sHandle);
void IIS_TX_DMA_ENABLE();
void audio_start();
void audio_stop();
void DMA1_Stream4_IRQHandler();
uint8_t IIS_Set_AudioFreq(uint32_t freq);
void IIS_DMA_Init(uint8_t* buf1,uint8_t* buf2,uint16_t length);
#endif 

