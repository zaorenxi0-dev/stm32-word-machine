#ifndef MYIIC_H
#define MYIIC_H

#include "./SYSTEM/sys/sys.h"

#define IIC_SCL_CLK()  do{ __HAL_RCC_GPIOB_CLK_ENABLE(); }while(0)
#define IIC_SCL_PIN   GPIO_PIN_8
#define IIC_SCL_PORT     GPIOB


#define IIC_SDA_CLK()  do{ __HAL_RCC_GPIOB_CLK_ENABLE(); }while(0)
#define IIC_SDA_PIN   GPIO_PIN_9
#define IIC_SDA_PORT    GPIOB

#define SDA(x)   do{x?\
					HAL_GPIO_WritePin(IIC_SDA_PORT, IIC_SDA_PIN,GPIO_PIN_SET):\
					HAL_GPIO_WritePin(IIC_SDA_PORT, IIC_SDA_PIN,GPIO_PIN_RESET);\
                    }while(0)

#define SCL(x)   do{x?\
					HAL_GPIO_WritePin(IIC_SCL_PORT, IIC_SCL_PIN,GPIO_PIN_SET):\
					HAL_GPIO_WritePin(IIC_SCL_PORT, IIC_SCL_PIN,GPIO_PIN_RESET);\
                    }while(0)	
#define IIC_Read_SDA()	(HAL_GPIO_ReadPin(IIC_SDA_PORT,IIC_SDA_PIN)?1:0)
	


					
void IIC_INIT();
static void IIC_DALAY();
void IIC_START();
void IIC_STOP();
uint8_t IIC_Rec_ACK();
void IIC_Send_ACK();
void IIC_Send_Byte(uint8_t byte);
uint8_t IIC_Read_Byte();					
				
#endif


