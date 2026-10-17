#include "WM8978.h"
#include "./BSP/IIC/myiic.h"
#include "./SYSTEM/delay/delay.h"

static uint16_t WM8978_REGVAL_TBL[58]=
{
	0X0000,0X0000,0X0000,0X0000,0X0050,0X0000,0X0140,0X0000,
	0X0000,0X0000,0X0000,0X00FF,0X00FF,0X0000,0X0100,0X00FF,
	0X00FF,0X0000,0X012C,0X002C,0X002C,0X002C,0X002C,0X0000,
	0X0032,0X0000,0X0000,0X0000,0X0000,0X0000,0X0000,0X0000,
	0X0038,0X000B,0X0032,0X0000,0X0008,0X000C,0X0093,0X00E9,
	0X0000,0X0000,0X0000,0X0000,0X0003,0X0010,0X0010,0X0100,
	0X0100,0X0002,0X0001,0X0001,0X0039,0X0039,0X0039,0X0039,
	0X0001,0X0001
}; 
uint8_t re=0;
uint8_t WM8978_Init()
{
	IIC_INIT();
	re=WM8978_Write_REG(0,0);

	
	delay_ms(10); 
	
	WM8978_Write_REG(1,0X1B);	//R1,MICEN设置为1(MIC使能),BIASEN设置为1(模拟器工作),VMIDSEL[1:0]设置为:11(5K)
	WM8978_Write_REG(2,0X1B0);	//R2,ROUT1,LOUT1输出使能(耳机可以工作),BOOSTENR,BOOSTENL使能
	WM8978_Write_REG(3,0X6C);	//R3,LOUT2,ROUT2输出使能(喇叭工作),RMIX,LMIX使能	
	WM8978_Write_REG(6,0);		//R6,MCLK由外部提供
	WM8978_Write_REG(43,1<<4);	//R43,INVROUT2反向,驱动喇叭
	WM8978_Write_REG(47,1<<8);	//R47设置,PGABOOSTL,左通道MIC获得20倍增益
	WM8978_Write_REG(48,1<<8);	//R48设置,PGABOOSTR,右通道MIC获得20倍增益
	WM8978_Write_REG(49,1<<1);	//R49,TSDEN,开启过热保护 
	WM8978_Write_REG(10,1<<3);	//R10,SOFTMUTE关闭,128x采样,最佳SNR 
	WM8978_Write_REG(14,1<<3);	//R14,ADC 128x采样率
	
	
	return 0;
}




uint8_t WM8978_Write_REG(uint8_t add,uint16_t byte)
{
	IIC_START();
	
	IIC_Send_Byte((0x1A<<1)|0);
	if(IIC_Rec_ACK()) 
	return 1;

	IIC_Send_Byte((add<<1)|((byte>>8)&0X01));
	if(IIC_Rec_ACK()) 
	return 1;
	
	IIC_Send_Byte(byte&0xFF);
	if(IIC_Rec_ACK()) 
	return 1;
	
	IIC_STOP();
	return 0;
}

uint16_t WM8978_Read_Reg(uint8_t reg)
{  
	  return WM8978_REGVAL_TBL[reg];	
} 


void WM8978_Speaker_Volume(uint8_t value)
{
	if(value>0x3F) value=0x3F;
	
	WM8978_Write_REG(54,value);
	WM8978_Write_REG(55,value|(1<<8));

}	


void WM8978_Phones_Volume(uint8_t value)
{
	if(value>0x3F) value=0x3F;
	
	WM8978_Write_REG(52,0x3F);
	WM8978_Write_REG(53,0x3F);

}	
void WM8978_Set_MODE(uint8_t fmt,uint8_t wl)
{
	fmt&=0X03;
	wl&=0X03;
	WM8978_Write_REG(4,(fmt<<3)|(wl<<5));
}

//WM8978 DAC/ADC配置
//adcen:adc使能(1)/关闭(0)
//dacen:dac使能(1)/关闭(0)
void WM8978_ADDA_Cfg(uint8_t dacen,uint8_t adcen)
{
	WM8978_Write_REG(3,0x6F);
	WM8978_Write_REG(2,0x1B0);
}

void WM8978_Output_Cfg(uint8_t dacen,uint8_t bpsen)
{
	uint16_t regval=0;
	if(dacen)regval|=1<<0;	//DAC输出使能
	if(bpsen)
	{
		regval|=1<<1;		//BYPASS使能
		regval|=5<<2;		//0dB增益
	} 
	WM8978_Write_REG(50,regval);//R50设置
	WM8978_Write_REG(51,regval);//R51设置 
}

