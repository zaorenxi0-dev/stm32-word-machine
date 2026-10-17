#include "audioplay.h"
#include "ff.h"			
#include "diskio.h"		
#include "./FATFS/exfuns/exfuns.h"
#include "./BSP/SRAM/sram.h"
#include "./MALLOC/malloc.h"
#include "./BSP/IIS/iis.h"
#include "./BSP/WM8978/WM8978.h"
#include "./SYSTEM/usart/usart.h"

uint8_t music_number=0,wav_flag=0,dma_success=0;
char music_name[10][256]={0};
UINT bre;

__wavctrl MUSIC_WAV_DATA;
audio_structure wav_structure;

extern void (*IIS_DMA_CALLBACK)(); 

uint8_t audio_play()
{
	DIR WAV_DIR;
	
	char* filename;
	//打开目录
	while(f_opendir(&WAV_DIR,"0:/MUSIC"))
	{
	printf("DIR OPEN FAIL");
	return 1;
	}
	//音乐名字
	if(MUSIC_OFFSET()==0)
	{
		return 1;
	}
	filename=(char*)mymalloc(SRAMIN , 256);
	if(!filename)
	{
		printf("memory fail");
		return 1;
	}
	 strcpy((char*)filename, "0:/MUSIC/");                      /* 复制路径(目录) */
     strcat((char*)filename, (const char*)music_name[0]);  /* 将文件名接在后面 */
	
	
	myfree(SRAMIN,filename);
	return 0;
}

uint8_t MUSIC_OFFSET()
{
	FILINFO* fileinfor;
	DIR tem_dir;
	uint8_t res;
	res=f_opendir(&tem_dir,"0:/MUSIC");
	if((res==FR_OK)&&(!fileinfor))
	{
		while(1)
		{
			
			fileinfor=(FILINFO*)mymalloc(SRAMIN , sizeof(FILINFO));
			res=f_readdir (&tem_dir, fileinfor);
			if(res==FR_OK)
			{
				res=exfuns_file_type((uint8_t*)fileinfor->fname);
				if((res & 0xF0) == 0x40)
				{
				strcpy(music_name[music_number], fileinfor->fname);
				music_number++;
				}
			}
			else
			{
				myfree(SRAMIN,fileinfor);
				return 0;
			}
			myfree(SRAMIN,fileinfor);
			
		}
	}
	else
	{
		printf("no music");
	    return 0;
	}


}

uint8_t WAV_GET_DATA(char* pname,__wavctrl* WAV_DATA)
{
	FIL fil;
	uint8_t res;
	uint8_t buf[512];
	RIFF_chunk* RIFF_structure;
	FMT_chunk* FMT_strtcture;
	ChunkFACT* FACT_structure;
	DATA_chunk* DATA_structure;
	
	
	
	res = f_open(&fil, pname, FA_READ); 
	if(res!=0)
	{

		printf("open music error");
		return 1;
	}

	res=f_read(&fil,buf,512,&bre);
	if(res!=0||!buf||bre!=512)
	{
		
		printf("read music error");
		return 2;
	}
	RIFF_structure=(RIFF_chunk*)buf;
	
	if(RIFF_structure->Format==0x45564157)
	{
		FMT_strtcture=(FMT_chunk*)(buf+12);
		FACT_structure=(ChunkFACT*)(buf+12+8+FMT_strtcture->Subchunk1Size);
		if(FMT_strtcture->Subchunk1ID==0x20746D66)
		{
			if(FACT_structure->ChunkID==0x74636166 || FACT_structure->ChunkID == 0x5453494C)
			{
				WAV_DATA->datastart=12+8+FMT_strtcture->Subchunk1Size+8+FACT_structure->ChunkSize;
			}
			else
			{
			
				WAV_DATA->datastart=12+8+FMT_strtcture->Subchunk1Size;
			}
			
			DATA_structure=(DATA_chunk*)(buf+WAV_DATA->datastart);
			
			if(DATA_structure->Subchunk2ID==0x61746164)
			{
					WAV_DATA->audioformat = FMT_strtcture->AudioFormat;       /* 音频格式 */
					WAV_DATA->nchannels = FMT_strtcture->NumChannels;       /* 通道数 */
					WAV_DATA->samplerate = FMT_strtcture->SampleRate;         /* 采样率 */
					WAV_DATA->bitrate = FMT_strtcture->ByteRate * 8;          /* 得到位速 */
                    WAV_DATA->blockalign = FMT_strtcture->BlockAlign;         /* 块对齐 */
                    WAV_DATA->bps = FMT_strtcture->BitsPerSample;             /* 位数,16/24/32位 */
                    
                    WAV_DATA->datasize = DATA_structure->Subchunk2Size;           /* 数据块大小 */
                    WAV_DATA->datastart = WAV_DATA->datastart + 8;      /* 数据流开始的地方. */
			
			}
			else
			     return 4;
			
		}
		else
			return 5;
	
	}
	else
	{
		return 6;
	}
	
	return 0;
}





void wav_buf_fill(uint8_t* buf)
{
	uint8_t* p;
	uint16_t i;
	uint32_t re;
	if(MUSIC_WAV_DATA.bps==16)
	{
		f_read(wav_structure.file,buf,WAV_I2S_TX_DMA_BUFSIZE,(UINT*)&re);
		if(re<WAV_I2S_TX_DMA_BUFSIZE)//不够数据了,补充0
		{
			for(i=re;i<WAV_I2S_TX_DMA_BUFSIZE-re;i++)buf[i]=0; 
		}
	}
	else
	{
		f_read(wav_structure.file,wav_structure.tbuf,WAV_I2S_TX_DMA_BUFSIZE*3/4,&br);
		 p = wav_structure.tbuf;
        
        for (int i = 0; i < WAV_I2S_TX_DMA_BUFSIZE*3/4;)
        {
            buf[i++] = p[1];
            buf[i] = p[2]; 
            i += 2;
            buf[i++] = p[0];
            p += 3;
        }
	
	
	}


}
void IIS_DMA_WAV_CALLBACK()
{

	if(DMA1_Stream4->CR&(1<<19))
	{
		wav_flag=0;
	}
	else
	{
		wav_flag=1;
	}
	dma_success=1;
}


uint8_t WAV_PLAY(char* pname)
{
	uint8_t res;
	WM8978_ADDA_Cfg(1,0);
	WM8978_Output_Cfg(1,0);
	
	wav_structure.i2sbuf1=mymalloc(SRAMIN , WAV_I2S_TX_DMA_BUFSIZE);
	wav_structure.i2sbuf2=mymalloc(SRAMIN , WAV_I2S_TX_DMA_BUFSIZE);
	wav_structure.file=(FIL*)mymalloc(SRAMIN , sizeof(FIL));
	wav_structure.tbuf=mymalloc(SRAMIN , WAV_I2S_TX_DMA_BUFSIZE);
	
	res=WAV_GET_DATA(pname,&MUSIC_WAV_DATA);

	if(res!=0)
	{
		return 1;
	}
	
	if(MUSIC_WAV_DATA.bps==16)
	{
		WM8978_Set_MODE(2,0);
		MX_I2S2_Init(I2S_MODE_MASTER_TX,I2S_STANDARD_PHILIPS,I2S_DATAFORMAT_16B_EXTENDED,I2S_CPOL_LOW);
	}
	else
	{
		WM8978_Set_MODE(2,2);
		MX_I2S2_Init(I2S_MODE_MASTER_TX,I2S_STANDARD_PHILIPS,I2S_DATAFORMAT_24B,I2S_CPOL_LOW);
	}

	
	IIS_Set_AudioFreq(MUSIC_WAV_DATA.samplerate);
	IIS_DMA_Init(wav_structure.i2sbuf1,wav_structure.i2sbuf2,WAV_I2S_TX_DMA_BUFSIZE);
	IIS_DMA_CALLBACK=IIS_DMA_WAV_CALLBACK;

	f_open(wav_structure.file,pname,FA_READ);
	f_lseek(wav_structure.file,MUSIC_WAV_DATA.datastart);
	
	wav_buf_fill(wav_structure.i2sbuf1);
	wav_buf_fill(wav_structure.i2sbuf2);


	//audio_start();
	while(1)
	{
			
		while(dma_success==0);
		dma_success=0;
		if(wav_flag==0)
		{
			wav_buf_fill(wav_structure.i2sbuf1);
		}
		else
		{
			wav_buf_fill(wav_structure.i2sbuf2);
		}
	
	}
	
	
	
	myfree(SRAMIN,wav_structure.i2sbuf1);
	myfree(SRAMIN,wav_structure.i2sbuf2);
	myfree(SRAMIN,wav_structure.file);
	myfree(SRAMIN,wav_structure.tbuf);
}



