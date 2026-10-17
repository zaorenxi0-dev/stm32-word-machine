#ifndef AUDIOPLAY_H
#define AUDIOPLAY_H

#include "./SYSTEM/sys/sys.h"
#include "ff.h"	

typedef __packed struct
{
	uint8_t* i2sbuf1;
	uint8_t* i2sbuf2;
	FIL* file;
	uint8_t *tbuf; 
	uint8_t stauts;
}audio_structure;

typedef __packed struct
{
	uint32_t ChunkID;
	uint32_t ChunkSize;
	uint32_t Format;
}RIFF_chunk;

typedef __packed struct
{
	uint32_t Subchunk1ID;
	uint32_t Subchunk1Size;
	uint16_t AudioFormat;
	uint16_t NumChannels;
	uint32_t SampleRate;
	uint32_t ByteRate;
	uint16_t BlockAlign;
	uint16_t BitsPerSample;
}FMT_chunk;

typedef __PACKED_STRUCT 
{
    uint32_t ChunkID;           /* chunk id;这里固定为"fact",即0X74636166; */
    uint32_t ChunkSize ;        /* 子集合大小(不包括ID和Size);这里为:4. */
    uint32_t NumOfSamples;      /* 采样的数量; */
}ChunkFACT;     /* fact块 */

typedef __packed struct
{
	uint32_t Subchunk2ID;
	uint32_t Subchunk2Size;
}DATA_chunk;


typedef __PACKED_STRUCT
{ 
    uint16_t audioformat;       /* 音频格式;0X01,表示线性PCM;0X11表示IMA ADPCM */
    uint16_t nchannels;         /* 通道数量;1,表示单声道;2,表示双声道; */
    uint16_t blockalign;        /* 块对齐(字节); */
    uint32_t datasize;          /* WAV数据大小 */

    uint32_t totsec ;           /* 整首歌时长,单位:秒 */
    uint32_t cursec ;           /* 当前播放时长 */

    uint32_t bitrate;           /* 比特率(位速) */
    uint32_t samplerate;        /* 采样率 */
    uint16_t bps;               /* 位数,比如16bit,24bit,32bit */

    uint32_t datastart;         /* 数据帧开始的位置(在文件里面的偏移) */
}__wavctrl;     /* wav 播放控制结构体 */

#define WAV_I2S_TX_DMA_BUFSIZE    8192 


uint8_t audio_play();
uint8_t MUSIC_OFFSET();
void IIS_DMA_WAV_CALLBACK();
uint8_t WAV_PLAY(char* pname);

#endif

