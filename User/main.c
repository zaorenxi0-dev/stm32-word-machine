/**
 * @file        main.c
 * @brief       嵌入式单词机主程序：外设初始化后启动 LVGL 界面与 FreeRTOS 任务
 */
#include "./SYSTEM/sys/sys.h"
#include "./SYSTEM/usart/usart.h"
#include "./SYSTEM/delay/delay.h"
#include "./BSP/LED/led.h"
#include "./BSP/KEY/key.h"
#include "./BSP/SRAM/sram.h"
#include "./MALLOC/malloc.h"
#include "lvgl_demo.h"
#include "./BSP/SDIO/sdio_sdcard.h"
#include "ff.h"			
#include "diskio.h"		
#include "./FATFS/exfuns/exfuns.h"
#include "./BSP/WORD/word.h"
#include "./BSP/WM8978/WM8978.h"
#include "./BSP/IIC/myiic.h"
#include "./BSP/TOUCH/touch.h"
#include "./APP/audioplay.h"

uint8_t flag=1;
uint32_t word_number=5;
extern uint32_t word_quantity;
extern uint8_t page_number;
UINT br, bw;
FIL fil;
DIR dir;
UINT res;
uint16_t red;



int main(void)
{
    HAL_Init();                         /* 初始化HAL库 */
    sys_stm32_clock_init(336, 8, 2, 7); /* 设置时钟,168Mhz */
    delay_init(168);                    /* 延时初始化 */
    usart_init(115200);                 /* 串口初始化为115200 */
    led_init();                         /* 初始化LED */
    key_init();                         /* 初始化按键 */
    sram_init();
	WM8978_Init();	/* WM8978 音频编解码器初始化 */
    my_mem_init(SRAMIN);                /* 初始化内部SRAM内存池 */
    my_mem_init(SRAMEX); 	/* 初始化外部SRAM内存池 */
    sd_init();
	WM8978_Speaker_Volume(10);
	
	exfuns_init();                  /* 为fatfs相关变量申请内存 */
    f_mount(fs[0], "0:", 1);        /* 挂载SD卡 */
    f_mount(fs[1], "1:", 1); 
	
	
	
	
	
	
	
	
	
	
	//WAV_PLAY("0:/MUSIC/1.wav");

	
	
	
	
	
	
	
	
	res = f_open(&fil, "0:/text/word.csv", FA_READ);  
	if(res!=0)
	{
	printf("open error\r\n");
	
	}
	printf("open success\r\n");
	
	
	while(flag)
	{
	sddata_to_flash();
	}
	
	
	for(int i=0;i<=word_quantity/1000;i++)
	{
	flash_to_offset(i);
	offset_to_wordlist();
	}
	sram_to_word_list(page_number);
	printf("start lvgl_demo\r\n");
    lvgl_demo();                        /* 启动FreeRTOS调度 */
}


