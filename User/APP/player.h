/**
 ****************************************************************************************************
 * @file        player.h
 * @author      zhangguangmao
 * @version     V1.0
 * @date        2025-12-20
 * @brief       WAV 音乐播放器：播放列表扫描 + DMA 双缓冲非阻塞播放
 * @license     Copyright (c) 2025
 ****************************************************************************************************
 * @attention
 *
 * 一、播放原理
 *   沿用 I2S2(SPI2) + DMA1_Stream4 的「双缓冲（Double Buffer / MultiBuffer）」机制：
 *     - 两个等长的音频缓冲区 buf1 / buf2 交给 DMA，DMA 在两者之间自动来回搬运；
 *     - 每搬完一个缓冲区产生一次传输完成中断，中断里只记录「CT 当前目标位」并置标志；
 *     - 音频任务收到标志后，去填充「刚刚播完、已经空闲」的那个缓冲区。
 *   这样写文件与放音并行进行，CPU 不需要等 DMA，也不会出现阻塞式 delay。
 *
 * 二、任务划分（并发安全的关键）
 *   audio_player 任务：唯一接触 FatFS / WM8978 / I2S / DMA 的执行体；
 *   界面任务（LVGL）：只读状态、只写「请求」，不直接操作硬件。
 *   两条任务通过 player.c 内部的请求变量解耦，避免对 I2C(SDIO) 与 DMA 的并发访问。
 *
 * 三、支持的格式
 *   16bit PCM WAV（单/双声道，采样率需落在 I2S 分频表支持范围内，如 8k/16k/22.05k/32k/44.1k/48k）。
 *   非 16bit PCM（24bit / 32bit / ADPCM / MP3 伪装扩展名等）会返回 PLAYER_ERR_FORMAT，
 *   界面据此提示，这也是本项目刻意保留的边界：不做不可靠的静默转换。
 ****************************************************************************************************
 */

#ifndef __PLAYER_H
#define __PLAYER_H

#include "./SYSTEM/sys/sys.h"

/* 播放状态 -------------------------------------------------------------------- */
#define PLAYER_STOP         0       /* 已停止                                   */
#define PLAYER_PLAY         1       /* 播放中                                   */
#define PLAYER_PAUSE        2       /* 已暂停                                   */

/* 返回码 / 错误码 -------------------------------------------------------------- */
#define PLAYER_OK            0      /* 成功                                     */
#define PLAYER_ERR_DIR     (-1)     /* 0:/MUSIC 目录打不开                       */
#define PLAYER_ERR_EMPTY   (-2)     /* 目录下没有可播放的 wav                    */
#define PLAYER_ERR_OPEN    (-3)     /* 文件打开失败                             */
#define PLAYER_ERR_FORMAT  (-4)     /* 非 16bit PCM WAV（或采样率不支持）        */
#define PLAYER_ERR_MEM     (-5)     /* 内存不足                                 */

/* 参数 ------------------------------------------------------------------------ */
#define PLAYER_DIR          "0:/MUSIC"      /* 音乐目录                          */
#define PLAYER_MAX_TRACK    24              /* 播放列表上限（首）                */
#define PLAYER_NAME_LEN     64              /* 文件名（含扩展名）缓冲长度        */
#define PLAYER_DISP_LEN     40              /* 显示名缓冲长度                    */
#define PLAYER_VOL_MIN      0
#define PLAYER_VOL_MAX      63
#define PLAYER_VOL_DEFAULT  20

/* 单个音频缓冲区大小：16bit 立体声 44.1kHz 时约 23ms，双缓冲共 46ms 余量 */
#define PLAYER_BUF_SIZE     4096

/* 初始化 ---------------------------------------------------------------------- */
void    player_init(void);                  /* 创建音频任务并开始扫描播放列表     */
uint8_t player_ready(void);                 /* 1: 播放列表已扫描完成              */
int8_t  player_error(void);                 /* 最近一次错误码                     */

/* 列表与状态（界面只读） -------------------------------------------------------- */
uint16_t    player_count(void);
const char *player_display_name(uint16_t idx);
int16_t     player_current(void);           /* 当前曲目下标，-1 表示无            */
uint8_t     player_state(void);             /* PLAYER_STOP / PLAY / PAUSE         */
uint8_t     player_volume(void);
void        player_progress(uint32_t *cur_sec, uint32_t *total_sec);

/* 控制请求（异步投递给音频任务执行） -------------------------------------------- */
void    player_play_index(uint16_t idx);
void    player_toggle(void);
void    player_stop(void);
void    player_next(void);
void    player_prev(void);
void    player_volume_step(int8_t delta);

#endif /* __PLAYER_H */
