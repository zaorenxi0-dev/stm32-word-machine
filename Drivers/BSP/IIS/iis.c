#include "iis.h"
#include "./SYSTEM/delay/delay.h"
#include "./APP/audioplay.h"

I2S_HandleTypeDef hi2s2;
DMA_HandleTypeDef hdma_spi2_tx;
const uint16_t I2S_PSC_TBL[][5]=
{
{ 800, 256, 5, 12, 1 }, /* 8Khz 采样率 */
{ 1102, 429, 4, 19, 0 }, /* 11.025Khz 采样率 */
{ 1600, 213, 2, 13, 0 }, /* 16Khz 采样率 */
{ 2205, 429, 4, 9, 1 }, /* 22.05Khz 采样率 */
{ 3200, 213, 2, 6, 1 }, /* 32Khz 采样率 */
{ 4410, 271, 2, 6, 0 }, /* 44.1Khz 采样率 */
{ 4800, 258, 3, 3, 1 }, /* 48Khz 采样率 */
{ 8820, 316, 2, 3, 1 }, /* 88.2Khz 采样率 */
{ 9600, 344, 2, 3, 1 }, /* 96Khz 采样率 */
{ 17640, 361, 2, 2, 0 }, /* 176.4Khz 采样率 */
{ 19200, 393, 2, 2, 0 }, /* 192Khz 采样率 */
};                                  




void MX_I2S2_Init(uint32_t Mode,uint32_t Standard,uint32_t DataFormat,uint32_t CPOL)
{
	HAL_I2S_MspInit(&hi2s2);

  hi2s2.Instance = SPI2;
  hi2s2.Init.Mode = Mode;
  hi2s2.Init.Standard = Standard;
  hi2s2.Init.DataFormat = DataFormat;
  hi2s2.Init.MCLKOutput = I2S_MCLKOUTPUT_DISABLE;
  hi2s2.Init.AudioFreq = I2S_AUDIOFREQ_DEFAULT;
  hi2s2.Init.CPOL = CPOL;
  hi2s2.Init.ClockSource = I2S_CLOCK_PLL;     

 	if (HAL_I2S_Init(&hi2s2) != HAL_OK)
    {
        printf("I2S初始化失败！\n");
        return;
    }
	  SPI2->CR2|=1<<1;
	__HAL_I2S_ENABLE(&hi2s2);	


	
}


void HAL_I2S_MspInit(I2S_HandleTypeDef* i2sHandle)
{
	__HAL_RCC_SPI2_CLK_ENABLE();
	
	IIS_WS_CLK();
	IIS_CK_CLK();
	IIS_MCK_CLK();
	IIS_SD_CLK();
	IIS_ext_SD_CLK();
	
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	 
	GPIO_InitStruct.Pin = IIS_ext_SD_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF6_I2S2ext;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = IIS_SD_PIN|IIS_MCK_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = IIS_CK_PIN|IIS_WS_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
	


}

uint8_t IIS_Set_AudioFreq(uint32_t freq)
{
	uint8_t i = 0; 
    uint32_t tempreg = 0;

    RCC_PeriphCLKInitTypeDef rcc_i2s_clk_init;

    for (i = 0; i < (sizeof(I2S_PSC_TBL) / 10); i++)    /* 看看该采样率是否可以支持 */
    {
        if ((freq / 10) == I2S_PSC_TBL[i][0])
        {
            break;
        }
    }
    if (i == (sizeof(I2S_PSC_TBL) / 10))
    {
        return 1;   /* 找不到 */
    }

    rcc_i2s_clk_init.PeriphClockSelection = RCC_PERIPHCLK_I2S;        /* 外设时钟源选择 */
    rcc_i2s_clk_init.PLLI2S.PLLI2SN = (uint32_t)I2S_PSC_TBL[i][1];    /* 设置PLLI2SN */
    rcc_i2s_clk_init.PLLI2S.PLLI2SR = (uint32_t)I2S_PSC_TBL[i][2];    /* 设置PLLI2SR */
    HAL_RCCEx_PeriphCLKConfig(&rcc_i2s_clk_init);                     /* 设置时钟 */
	
	RCC->CR|=1<<26;					//开启I2S时钟
	while((RCC->CR&1<<27)==0);		//等待I2S时钟开启成功. 
    tempreg = I2S_PSC_TBL[i][3] << 0;   /* 设置I2SDIV */
    tempreg |= I2S_PSC_TBL[i][4] << 8;  /* 设置ODD位 */
    tempreg |= 1 << 9;                  /* 使能MCKOE位,输出MCK */
    SPI2->I2SPR = tempreg;           /* 设置I2SPR寄存器 */

    return 0;



}

void (*IIS_DMA_CALLBACK)();

void IIS_DMA_Init(uint8_t* buf1,uint8_t* buf2,uint16_t length)
{
	// HAL_I2S_DMAStop(&hi2s2);
    HAL_DMA_Abort(&hdma_spi2_tx);
    
    // 2. 确保DMA已禁用
    DMA1_Stream4->CR &= ~DMA_SxCR_EN;
    while(DMA1_Stream4->CR & DMA_SxCR_EN); // 等待DMA完全停止
	
	__HAL_RCC_DMA1_CLK_ENABLE();

	hdma_spi2_tx.Instance = DMA1_Stream4;
    hdma_spi2_tx.Init.Channel = DMA_CHANNEL_0;
    hdma_spi2_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma_spi2_tx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_spi2_tx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_spi2_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_spi2_tx.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    hdma_spi2_tx.Init.Mode = DMA_CIRCULAR;
    hdma_spi2_tx.Init.Priority = DMA_PRIORITY_VERY_HIGH;
    hdma_spi2_tx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
	hdma_spi2_tx.Init.MemBurst = DMA_MBURST_SINGLE;
    hdma_spi2_tx.Init.PeriphBurst = DMA_PBURST_SINGLE;
	
	HAL_DMA_Init(&hdma_spi2_tx);
	__HAL_LINKDMA(&hi2s2,hdmatx,hdma_spi2_tx);

	__HAL_DMA_CLEAR_FLAG(&hdma_spi2_tx, DMA_FLAG_TCIF0_4 | DMA_FLAG_HTIF0_4 | DMA_FLAG_TEIF0_4);
	HAL_NVIC_SetPriority(DMA1_Stream4_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream4_IRQn);
	

	HAL_DMAEx_MultiBufferStart(&hdma_spi2_tx,(uint32_t)buf1,(uint32_t)&SPI2->DR,(uint32_t)buf2,(uint32_t)length/2);

	__HAL_DMA_DISABLE(&hdma_spi2_tx); 
	__HAL_DMA_ENABLE_IT(&hdma_spi2_tx, DMA_IT_TC); 

}




void DMA1_Stream4_IRQHandler()
{

    if(__HAL_DMA_GET_FLAG(&hdma_spi2_tx, DMA_FLAG_TCIF0_4))  // 传输完成
    {
		__HAL_DMA_CLEAR_FLAG(&hdma_spi2_tx, DMA_FLAG_TCIF0_4);
			IIS_DMA_CALLBACK();
    }
    

}

void audio_start()
{
	__HAL_DMA_ENABLE(&hdma_spi2_tx);  


}



void audio_stop()
{

	__HAL_DMA_DISABLE(&hdma_spi2_tx);  


}
