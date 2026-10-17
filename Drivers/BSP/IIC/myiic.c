#include "myiic.h"
#include "./SYSTEM/delay/delay.h"

void IIC_INIT()
{
GPIO_InitTypeDef GPIO_InitTypeDefStruture;
	
IIC_SCL_CLK();
IIC_SDA_CLK();	
	
GPIO_InitTypeDefStruture.Pin=IIC_SCL_PIN;
GPIO_InitTypeDefStruture.Mode=GPIO_MODE_OUTPUT_PP;
GPIO_InitTypeDefStruture.Pull= GPIO_PULLUP;
GPIO_InitTypeDefStruture.Speed=GPIO_SPEED_FREQ_VERY_HIGH ;

HAL_GPIO_Init(IIC_SCL_PORT,&GPIO_InitTypeDefStruture);

GPIO_InitTypeDefStruture.Pin=IIC_SDA_PIN;
GPIO_InitTypeDefStruture.Mode=GPIO_MODE_OUTPUT_OD;
GPIO_InitTypeDefStruture.Pull= GPIO_PULLUP;
GPIO_InitTypeDefStruture.Speed=GPIO_SPEED_FREQ_VERY_HIGH ;

HAL_GPIO_Init(IIC_SDA_PORT,&GPIO_InitTypeDefStruture);

IIC_STOP();
}

static void IIC_DALAY()
{
delay_us(2);
}

void IIC_START()
{
	SCL(1);
	SDA(1);
	IIC_DALAY();
	SDA(0);
	IIC_DALAY();

}

void IIC_STOP()
{
	SDA(0);
	IIC_DALAY();
	SCL(1);
	IIC_DALAY();
	SDA(1);
	IIC_DALAY();
}


uint8_t IIC_Rec_ACK()
{
	uint8_t time=0,ACK=0;
	SCL(0);
	SDA(1);
	IIC_DALAY();
	SCL(1);
	IIC_DALAY();
	while(IIC_Read_SDA())
	{
	time++;
	if(time>250)
		{
			IIC_STOP();
			ACK=1;
			break;
		}
	}
	SCL(0);
	IIC_DALAY();
	return ACK;
}

void IIC_Send_ACK()
{
	SCL(0);
	SDA(0);
	IIC_DALAY();
	SCL(1);
	SDA(0);
	IIC_DALAY();
	SCL(0);
	IIC_DALAY();
	SDA(1);
}

void IIC_Send_Byte(uint8_t byte)
{
	for(int i=7;i>=0;i--)
	{
		SCL(0);
		SDA((byte>>i)&1);
		IIC_DALAY();
		SCL(1);
		IIC_DALAY();
	}
}	


uint8_t IIC_Read_Byte()
{
	uint8_t byte=0;
	for(int i=8;i>0;i--)
	{
		SCL(0);
		IIC_DALAY();
		SCL(1);
		byte=(IIC_Read_SDA()<<i)|byte;
		IIC_DALAY();
	}
	IIC_Send_ACK();
	return byte;
}



