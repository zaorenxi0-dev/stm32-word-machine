/**
 ****************************************************************************************************
 * @file        player.c
 * @author      zhangguangmao
 * @version     V1.0
 * @date        2025-12-20
 * @brief       WAV 音乐播放器实现：播放列表扫描 + DMA 双缓冲非阻塞播放
 * @license     Copyright (c) 2025
 ****************************************************************************************************
 */

#include "player.h"
#include <string.h>
#include "ff.h"
#include "diskio.h"
#include "FreeRTOS.h"
#include "task.h"
#include "./MALLOC/malloc.h"
#include "./BSP/IIS/iis.h"
#include "./BSP/WM8978/WM8978.h"
#include "./SYSTEM/usart/usart.h"

/* iis.c 中定义了 DMA 传输完成回调函数指针，但 iis.h 未对外声明，这里按 audioplay.c 的写法自行 extern */
extern void (*IIS_DMA_CALLBACK)(void);

/* 音频任务参数 */
#define PLAYER_TASK_PRIO    2           /* 低于 LVGL 任务(3)，保证界面不被放音拖慢 */
#define PLAYER_TASK_STK     384         /* 单位: Word，f_read 调用栈不深，够用      */
#define PLAYER_POLL_MS      5           /* 轮询周期，远小于一个缓冲区的播放时长     */

/* 卷标属性：本版 FatFs 只提供了 AM_DIR / AM_ARC，这里按标准补齐 AM_VOL(0x08) */
#define PLAYER_AM_VOL       0x08

/* 界面投递的请求 --------------------------------------------------------------- */
#define PLAYER_REQ_NONE     0
#define PLAYER_REQ_PLAY     1
#define PLAYER_REQ_TOGGLE   2
#define PLAYER_REQ_STOP     3
#define PLAYER_REQ_NEXT     4
#define PLAYER_REQ_PREV     5

/* WAV 头部解析结果 ------------------------------------------------------------- */
typedef struct
{
    uint32_t sample_rate;               /* 采样率       */
    uint16_t channels;                  /* 声道数       */
    uint16_t bits;                      /* 位深         */
    uint32_t byte_rate;                 /* 字节率(用于换算时长) */
    uint16_t block_align;               /* 块对齐       */
    uint32_t data_start;                /* 数据段在文件中的偏移 */
    uint32_t data_size;                 /* 数据段字节数 */
} player_wav_t;

/* 播放列表 --------------------------------------------------------------------- */
static char     pl_name[PLAYER_MAX_TRACK][PLAYER_NAME_LEN];   /* 原始文件名，拼路径用 */
static char     pl_disp[PLAYER_MAX_TRACK][PLAYER_DISP_LEN];   /* 显示名（去扩展名）   */
static uint16_t pl_count = 0;

/* 运行时状态 ------------------------------------------------------------------- */
static uint8_t      *p_buf1 = NULL;
static uint8_t      *p_buf2 = NULL;
static FIL           p_fil;
static uint8_t       p_fil_open = 0;

static uint8_t       p_state = PLAYER_STOP;
static int16_t       p_index = -1;
static uint8_t       p_volume = PLAYER_VOL_DEFAULT;
static int8_t        p_error = PLAYER_OK;

static uint32_t      p_total = 0;           /* 数据段总字节数         */
static uint32_t      p_remain = 0;          /* 数据段剩余字节数       */
static uint32_t      p_byte_rate = 0;       /* 字节率，换算秒用       */
static uint8_t       p_silence_fills = 0;   /* 已排入的纯静音缓冲区数 */

/* DMA 中断与任务之间共享的标志（全部为单字节/单字读写，天然原子） */
static volatile uint8_t p_dma_flag = 0;     /* 1: 有一个缓冲区已播完  */
static volatile uint8_t p_dma_target = 0;   /* DMA_SxCR.CT 当前目标位 */

/* 界面 -> 任务 的请求与共享只读状态 */
static volatile uint8_t  p_req = PLAYER_REQ_NONE;
static volatile int16_t  p_req_idx = 0;
static volatile uint8_t  p_vol_target = PLAYER_VOL_DEFAULT;
static volatile uint8_t  p_ready = 0;
static uint8_t           p_inited = 0;

/* ------------------------------------------------------------------------------------------------
 * 小工具
 * ---------------------------------------------------------------------------------------------- */

/* 判断文件名是否以 .wav 结尾（大小写不敏感） */
static uint8_t name_is_wav(const char *n)
{
    uint16_t len = (uint16_t)strlen(n);

    if (len < 5)
    {
        return 0;
    }
    if (n[len - 4] != '.')
    {
        return 0;
    }
    if ((n[len - 3] | 0x20) != 'w')
    {
        return 0;
    }
    if ((n[len - 2] | 0x20) != 'a')
    {
        return 0;
    }
    if ((n[len - 1] | 0x20) != 'v')
    {
        return 0;
    }
    return 1;
}

/* 生成显示名：去掉扩展名；非 ASCII（GBK 汉字文件名）在现有字库里没有字形，用 '?' 占位，
   一个双字节汉字只占一个 '?'，这样仍能区分不同曲目 */
static void name_to_disp(char *dst, const char *src)
{
    uint16_t i = 0;
    uint16_t o = 0;
    uint8_t  c;

    while ((src[i] != '\0') && (o < (PLAYER_DISP_LEN - 1U)))
    {
        c = (uint8_t)src[i];
        if (c == '.')
        {
            break;                          /* 扩展名不显示 */
        }
        if (c < 0x80U)
        {
            dst[o] = (char)c;
            o++;
            i++;
        }
        else
        {
            dst[o] = '?';
            o++;
            i += ((uint8_t)src[i + 1] != 0U) ? 2U : 1U;
        }
    }
    dst[o] = '\0';
}

/* ------------------------------------------------------------------------------------------------
 * 播放列表扫描（运行在音频任务中）
 * ---------------------------------------------------------------------------------------------- */

static void player_scan(void)
{
    DIR      dir;
    FILINFO  fno;
    FRESULT  res;
    uint16_t n = 0;

    pl_count = 0;

    res = f_opendir(&dir, PLAYER_DIR);
    if (res != FR_OK)
    {
        p_error = PLAYER_ERR_DIR;
        printf("player: open %s fail (%d)\r\n", PLAYER_DIR, (int)res);
        return;
    }

    for (;;)
    {
        res = f_readdir(&dir, &fno);
        if ((res != FR_OK) || (fno.fname[0] == '\0'))
        {
            break;                          /* 目录读完了 */
        }
        if ((fno.fattrib & (AM_DIR | PLAYER_AM_VOL)) != 0)
        {
            continue;                       /* 跳过子目录与卷标 */
        }
        if (name_is_wav(fno.fname) == 0)
        {
            continue;
        }
        if (strlen(fno.fname) >= PLAYER_NAME_LEN)
        {
            printf("player: name too long, skipped\r\n");
            continue;
        }
        if (n >= PLAYER_MAX_TRACK)
        {
            break;                          /* 列表已满 */
        }

        strcpy(pl_name[n], fno.fname);
        name_to_disp(pl_disp[n], fno.fname);
        n++;
    }

    f_closedir(&dir);

    if (n == 0U)
    {
        p_error = PLAYER_ERR_EMPTY;
        printf("player: no wav in %s\r\n", PLAYER_DIR);
    }
    else
    {
        p_error = PLAYER_OK;
        printf("player: %d track(s)\r\n", (int)n);
    }
    pl_count = n;
}

/* ------------------------------------------------------------------------------------------------
 * WAV 头部解析
 * ---------------------------------------------------------------------------------------------- */

/* 按 RIFF 规范逐个 chunk 遍历，直到取到 fmt 与 data；
   比「固定读 512 字节再按偏移猜」更稳，能正确越过 LIST/fact 等附加块 */
static int8_t wav_parse(FIL *fp, player_wav_t *info)
{
    uint8_t  hdr[12];
    uint8_t  chunk[8];
    uint8_t  fmtbuf[16];
    UINT     br = 0;
    uint32_t pos;
    uint32_t csize;
    uint32_t fsz;
    uint16_t audio_format;
    uint8_t  got_fmt = 0;

    if (f_read(fp, hdr, 12, &br) != FR_OK)
    {
        return PLAYER_ERR_FORMAT;
    }
    if (br != 12U)
    {
        return PLAYER_ERR_FORMAT;
    }
    if ((hdr[0] != 'R') || (hdr[1] != 'I') || (hdr[2] != 'F') || (hdr[3] != 'F'))
    {
        return PLAYER_ERR_FORMAT;
    }
    if ((hdr[8] != 'W') || (hdr[9] != 'A') || (hdr[10] != 'V') || (hdr[11] != 'E'))
    {
        return PLAYER_ERR_FORMAT;
    }

    fsz = (uint32_t)f_size(fp);
    pos = 12U;

    while ((pos + 8U) <= fsz)
    {
        if (f_lseek(fp, pos) != FR_OK)
        {
            return PLAYER_ERR_FORMAT;
        }
        if (f_read(fp, chunk, 8, &br) != FR_OK)
        {
            return PLAYER_ERR_FORMAT;
        }
        if (br != 8U)
        {
            return PLAYER_ERR_FORMAT;
        }

        csize = (uint32_t)chunk[4] | ((uint32_t)chunk[5] << 8) |
                ((uint32_t)chunk[6] << 16) | ((uint32_t)chunk[7] << 24);

        if ((chunk[0] == 'f') && (chunk[1] == 'm') && (chunk[2] == 't') && (chunk[3] == ' '))
        {
            if (csize < 16U)
            {
                return PLAYER_ERR_FORMAT;
            }
            if (f_read(fp, fmtbuf, 16, &br) != FR_OK)
            {
                return PLAYER_ERR_FORMAT;
            }
            if (br != 16U)
            {
                return PLAYER_ERR_FORMAT;
            }

            audio_format      = (uint16_t)fmtbuf[0] | ((uint16_t)fmtbuf[1] << 8);
            info->channels    = (uint16_t)fmtbuf[2] | ((uint16_t)fmtbuf[3] << 8);
            info->sample_rate = (uint32_t)fmtbuf[4] | ((uint32_t)fmtbuf[5] << 8) |
                                ((uint32_t)fmtbuf[6] << 16) | ((uint32_t)fmtbuf[7] << 24);
            info->byte_rate   = (uint32_t)fmtbuf[8] | ((uint32_t)fmtbuf[9] << 8) |
                                ((uint32_t)fmtbuf[10] << 16) | ((uint32_t)fmtbuf[11] << 24);
            info->block_align = (uint16_t)fmtbuf[12] | ((uint16_t)fmtbuf[13] << 8);
            info->bits        = (uint16_t)fmtbuf[14] | ((uint16_t)fmtbuf[15] << 8);

            if (audio_format != 1U)
            {
                printf("player: not PCM (fmt=%d)\r\n", (int)audio_format);
                return PLAYER_ERR_FORMAT;   /* 只支持线性 PCM */
            }
            if (info->bits != 16U)
            {
                printf("player: %dbit not supported\r\n", (int)info->bits);
                return PLAYER_ERR_FORMAT;   /* 只支持 16bit */
            }
            if ((info->channels == 0U) || (info->byte_rate == 0U))
            {
                return PLAYER_ERR_FORMAT;
            }
            got_fmt = 1;
        }
        else if ((chunk[0] == 'd') && (chunk[1] == 'a') && (chunk[2] == 't') && (chunk[3] == 'a'))
        {
            if (got_fmt == 0U)
            {
                return PLAYER_ERR_FORMAT;   /* data 出现在 fmt 之前，非法 */
            }
            info->data_start = pos + 8U;
            info->data_size  = csize;

            /* 部分文件的 data 长度字段不可信，按文件实际剩余长度兜底 */
            if ((info->data_size == 0U) || ((info->data_start + info->data_size) > fsz))
            {
                info->data_size = fsz - info->data_start;
            }
            return PLAYER_OK;
        }

        pos += 8U + csize + (csize & 1U);   /* RIFF chunk 按偶数字节对齐 */
    }

    return PLAYER_ERR_FORMAT;
}

/* ------------------------------------------------------------------------------------------------
 * 缓冲区填充与音质无关的公共逻辑
 * ---------------------------------------------------------------------------------------------- */

static void player_fill(uint8_t *buf)
{
    UINT     br = 0;
    uint32_t want;

    if ((p_fil_open == 0U) || (p_remain == 0U))
    {
        memset(buf, 0, PLAYER_BUF_SIZE);    /* 数据放完，补静音 */
        p_silence_fills++;
        return;
    }

    want = (p_remain < PLAYER_BUF_SIZE) ? p_remain : PLAYER_BUF_SIZE;

    if (f_read(&p_fil, buf, want, &br) != FR_OK)
    {
        br = 0;
    }
    if (br < PLAYER_BUF_SIZE)
    {
        memset(&buf[br], 0, PLAYER_BUF_SIZE - br);   /* 尾部补零，避免残留旧数据 */
    }
    p_remain = (br >= p_remain) ? 0U : (p_remain - br);
}

/* ------------------------------------------------------------------------------------------------
 * DMA 回调：只做标志记录，不做任何耗时操作（在中断上下文执行）
 * ---------------------------------------------------------------------------------------------- */

static void player_dma_callback(void)
{
    /* CT=1 表示 DMA 当前目标为 buf2，即刚刚播完的是 buf1 */
    p_dma_target = ((DMA1_Stream4->CR & DMA_SxCR_CT) != 0U) ? 1U : 0U;
    p_dma_flag = 1U;
}

/* ------------------------------------------------------------------------------------------------
 * 打开并播放指定曲目（仅音频任务调用）
 * ---------------------------------------------------------------------------------------------- */

static void player_stop_impl(void)
{
    audio_stop();
    if (p_fil_open != 0U)
    {
        f_close(&p_fil);
        p_fil_open = 0;
    }
    p_state = PLAYER_STOP;
    p_remain = 0;
    p_total = 0;
    p_silence_fills = 0;
}

static int8_t player_open(uint16_t idx)
{
    char           path[PLAYER_NAME_LEN + 16];
    player_wav_t   info;
    int8_t         rc;

    if (idx >= pl_count)
    {
        return PLAYER_ERR_EMPTY;
    }

    player_stop_impl();

    /* 拼接 0:/MUSIC/xxx.wav */
    strcpy(path, PLAYER_DIR);
    strcat(path, "/");
    strcat(path, pl_name[idx]);

    if (f_open(&p_fil, path, FA_READ) != FR_OK)
    {
        p_error = PLAYER_ERR_OPEN;
        return PLAYER_ERR_OPEN;
    }
    p_fil_open = 1;

    memset(&info, 0, sizeof(info));
    rc = wav_parse(&p_fil, &info);
    if (rc != PLAYER_OK)
    {
        f_close(&p_fil);
        p_fil_open = 0;
        p_error = rc;
        return rc;
    }

    /* 双缓冲只分配一次，之后长期复用 */
    if (p_buf1 == NULL)
    {
        p_buf1 = (uint8_t *)mymalloc(SRAMIN, PLAYER_BUF_SIZE);
    }
    if (p_buf2 == NULL)
    {
        p_buf2 = (uint8_t *)mymalloc(SRAMIN, PLAYER_BUF_SIZE);
    }
    if ((p_buf1 == NULL) || (p_buf2 == NULL))
    {
        f_close(&p_fil);
        p_fil_open = 0;
        p_error = PLAYER_ERR_MEM;
        return PLAYER_ERR_MEM;
    }

    /* WM8978 + I2S 按 16bit 配置 */
    WM8978_ADDA_Cfg(1, 0);
    WM8978_Output_Cfg(1, 0);
    WM8978_Set_MODE(2, 0);
    WM8978_Speaker_Volume(p_volume);

    MX_I2S2_Init(I2S_MODE_MASTER_TX, I2S_STANDARD_PHILIPS, I2S_DATAFORMAT_16B_EXTENDED, I2S_CPOL_LOW);

    if (IIS_Set_AudioFreq(info.sample_rate) != 0)
    {
        printf("player: freq %d not supported\r\n", (int)info.sample_rate);
        f_close(&p_fil);
        p_fil_open = 0;
        p_error = PLAYER_ERR_FORMAT;
        return PLAYER_ERR_FORMAT;
    }

    /* DMA 双缓冲就绪，并挂上回调 */
    IIS_DMA_Init(p_buf1, p_buf2, PLAYER_BUF_SIZE);
    IIS_DMA_CALLBACK = player_dma_callback;

    if (f_lseek(&p_fil, info.data_start) != FR_OK)
    {
        f_close(&p_fil);
        p_fil_open = 0;
        p_error = PLAYER_ERR_OPEN;
        return PLAYER_ERR_OPEN;
    }

    p_total         = info.data_size;
    p_remain        = info.data_size;
    p_byte_rate     = info.byte_rate;
    p_silence_fills = 0;
    p_index         = (int16_t)idx;
    p_error         = PLAYER_OK;

    /* 先填满两个缓冲区，再启动 DMA，避免开头出现断音 */
    player_fill(p_buf1);
    player_fill(p_buf2);

    p_dma_flag = 0;                     /* 清掉切换过程中可能残留的一次中断 */
    p_state = PLAYER_PLAY;
    audio_start();
    return PLAYER_OK;
}

/* 从 start 开始沿 delta 方向找第一首能正常打开的曲目，找不到就停下 */
static void player_skip_to_valid(int16_t start, int8_t delta)
{
    int16_t  i = start;
    uint16_t tries = 0;

    if (pl_count == 0U)
    {
        return;
    }
    if (i < 0)
    {
        i = (int16_t)pl_count - 1;
    }
    if (i >= (int16_t)pl_count)
    {
        i = 0;
    }

    while (tries < pl_count)
    {
        if (player_open((uint16_t)i) == PLAYER_OK)
        {
            return;
        }
        i = (int16_t)(i + delta);
        if (i < 0)
        {
            i = (int16_t)pl_count - 1;
        }
        if (i >= (int16_t)pl_count)
        {
            i = 0;
        }
        tries++;
    }

    p_state = PLAYER_STOP;              /* 一圈下来没有一首能放 */
}

/* ------------------------------------------------------------------------------------------------
 * 每秒/每周期要做的维护工作（仅音频任务调用）
 * ---------------------------------------------------------------------------------------------- */

/* 音量在这里统一落地，保证 WM8978 的 I2C 访问只由一个任务发起 */
static void player_apply_volume(void)
{
    uint8_t v = p_vol_target;

    if (v != p_volume)
    {
        p_volume = v;
        WM8978_Speaker_Volume(p_volume);
    }
}

static void player_pump(void)
{
    uint8_t *free_buf;

    if ((p_state != PLAYER_PLAY) || (p_dma_flag == 0U) || (p_buf1 == NULL))
    {
        return;
    }
    p_dma_flag = 0;

    /* CT=1 -> 正在搬 buf2，空闲的是 buf1 */
    free_buf = (p_dma_target != 0U) ? p_buf1 : p_buf2;

    if ((p_remain == 0U) && (p_silence_fills > 0U))
    {
        /* 两个缓冲区都已经是静音，说明本曲真正放完了，自动续播下一首 */
        printf("player: track done, next\r\n");
        player_skip_to_valid((int16_t)(p_index + 1), 1);
        return;
    }

    player_fill(free_buf);
}

static void player_service_request(void)
{
    uint8_t req = p_req;

    if (req == PLAYER_REQ_NONE)
    {
        return;
    }
    p_req = PLAYER_REQ_NONE;

    switch (req)
    {
        case PLAYER_REQ_PLAY:
            player_skip_to_valid(p_req_idx, 1);
            break;

        case PLAYER_REQ_TOGGLE:
            if (pl_count == 0U)
            {
                break;
            }
            if (p_state == PLAYER_PLAY)
            {
                audio_stop();
                p_state = PLAYER_PAUSE;
            }
            else if (p_state == PLAYER_PAUSE)
            {
                audio_start();          /* 继续用 DMA 里没播完的数据 */
                p_state = PLAYER_PLAY;
            }
            else
            {
                player_skip_to_valid((p_index >= 0) ? p_index : 0, 1);
            }
            break;

        case PLAYER_REQ_STOP:
            player_stop_impl();
            break;

        case PLAYER_REQ_NEXT:
            player_skip_to_valid((int16_t)(p_index + 1), 1);
            break;

        case PLAYER_REQ_PREV:
            player_skip_to_valid((int16_t)(p_index - 1), -1);
            break;

        default:
            break;
    }
}

/* ------------------------------------------------------------------------------------------------
 * 音频任务
 * ---------------------------------------------------------------------------------------------- */

static void player_task(void *pvParameters)
{
    pvParameters = pvParameters;

    player_scan();
    p_ready = 1;                        /* 扫描完成后界面才允许读列表 */

    for (;;)
    {
        player_apply_volume();
        player_service_request();
        player_pump();

        vTaskDelay(pdMS_TO_TICKS(PLAYER_POLL_MS));
    }
}

/* ------------------------------------------------------------------------------------------------
 * 对外接口
 * ---------------------------------------------------------------------------------------------- */

void player_init(void)
{
    if (p_inited != 0U)
    {
        return;
    }
    p_inited = 1;
    p_error  = PLAYER_OK;

    if (xTaskCreate((TaskFunction_t)player_task,
                    "player_task",
                    (uint16_t)PLAYER_TASK_STK,
                    (void *)NULL,
                    (UBaseType_t)PLAYER_TASK_PRIO,
                    (TaskHandle_t *)NULL) != pdPASS)
    {
        p_inited = 0;
        p_error  = PLAYER_ERR_MEM;
        printf("player: create task fail\r\n");
    }
}

uint8_t player_ready(void)
{
    return p_ready;
}

int8_t player_error(void)
{
    return p_error;
}

uint16_t player_count(void)
{
    return pl_count;
}

const char *player_display_name(uint16_t idx)
{
    if (idx >= pl_count)
    {
        return "";
    }
    return pl_disp[idx];
}

int16_t player_current(void)
{
    return p_index;
}

uint8_t player_state(void)
{
    return p_state;
}

uint8_t player_volume(void)
{
    return p_vol_target;
}

void player_progress(uint32_t *cur_sec, uint32_t *total_sec)
{
    uint32_t tot = 0;
    uint32_t cur = 0;

    if (p_byte_rate > 0U)
    {
        tot = p_total / p_byte_rate;
        if (p_remain <= p_total)
        {
            cur = (p_total - p_remain) / p_byte_rate;
        }
    }
    if (cur_sec != NULL)
    {
        *cur_sec = cur;
    }
    if (total_sec != NULL)
    {
        *total_sec = tot;
    }
}

void player_play_index(uint16_t idx)
{
    if (idx >= pl_count)
    {
        return;
    }
    p_req_idx = (int16_t)idx;
    p_req     = PLAYER_REQ_PLAY;
}

void player_toggle(void)
{
    p_req = PLAYER_REQ_TOGGLE;
}

void player_stop(void)
{
    p_req = PLAYER_REQ_STOP;
}

void player_next(void)
{
    p_req = PLAYER_REQ_NEXT;
}

void player_prev(void)
{
    p_req = PLAYER_REQ_PREV;
}

void player_volume_step(int8_t delta)
{
    int16_t v = (int16_t)p_vol_target + (int16_t)delta;

    if (v < PLAYER_VOL_MIN)
    {
        v = PLAYER_VOL_MIN;
    }
    if (v > PLAYER_VOL_MAX)
    {
        v = PLAYER_VOL_MAX;
    }
    p_vol_target = (uint8_t)v;
}
