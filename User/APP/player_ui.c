/**
 ****************************************************************************************************
 * @file        player_ui.c
 * @author      zhangguangmao
 * @version     V1.0
 * @date        2025-12-20
 * @brief       音乐播放界面实现（320x240）
 * @license     Copyright (c) 2025
 ****************************************************************************************************
 * @attention
 *
 *   界面布局：
 *     y   1..28   返回「<」按钮 + 曲名（横向滚动）
 *     y  32..56   「第 n/N 首」 + 播放状态
 *     y  60..78   进度条
 *     y  80..102  已播时长 / 总时长
 *     y 106..134  音量  [-][+] 与当前音量值
 *     y 140..228  [上一曲] [播放/暂停] [下一曲]
 *
 *   线程约定：本文件运行在 LVGL 任务中，只调用 player.h 的状态读取与「请求投递」接口，
 *   不直接接触 FatFS / WM8978 / I2S / DMA；界面刷新由独立的 lv_timer 周期驱动，
 *   定时器在屏幕载入时 resume、离开时 pause，避免后台空转。
 ****************************************************************************************************
 */

#include "player_ui.h"
#include "player.h"
#include "gui_guider.h"
#include <string.h>

LV_FONT_DECLARE(font)

#define PLAYER_UI_PERIOD    400         /* 界面刷新周期(ms) */

/* 界面对象 ---------------------------------------------------------------- */
static lv_obj_t   *pl_scr       = NULL;
static lv_obj_t   *pl_prev_scr  = NULL;
static lv_obj_t   *pl_lbl_title = NULL;
static lv_obj_t   *pl_lbl_idx   = NULL;
static lv_obj_t   *pl_lbl_state = NULL;
static lv_obj_t   *pl_lbl_time  = NULL;
static lv_obj_t   *pl_lbl_vol   = NULL;
static lv_obj_t   *pl_bar       = NULL;
static lv_obj_t   *pl_btn_play_label = NULL;
static lv_timer_t *pl_timer     = NULL;

/* ------------------------------------------------------------------------------------------------
 * 字符串小工具（避免依赖 printf 浮点/格式化，也避免多字节字符常量）
 * ---------------------------------------------------------------------------------------------- */

static int u32_to_str(char *dst, uint32_t v)
{
    char     tmp[12];
    int      n = 0;
    int      i = 0;

    if (v == 0U)
    {
        dst[0] = '0';
        dst[1] = '\0';
        return 1;
    }
    while ((v > 0U) && (n < 11))
    {
        tmp[n] = (char)('0' + (int)(v % 10U));
        v /= 10U;
        n++;
    }
    while (n > 0)
    {
        dst[i] = tmp[--n];
        i++;
    }
    dst[i] = '\0';
    return i;
}

/* 秒 -> "m:ss" */
static void sec_to_mmss(char *dst, uint32_t sec)
{
    char     num[12];
    uint32_t m = sec / 60U;
    uint32_t s = sec % 60U;

    dst[0] = '\0';
    u32_to_str(num, m);
    strcat(dst, num);
    strcat(dst, ":");
    if (s < 10U)
    {
        strcat(dst, "0");
    }
    u32_to_str(num, s);
    strcat(dst, num);
}

/* ------------------------------------------------------------------------------------------------
 * 界面刷新
 * ---------------------------------------------------------------------------------------------- */

static void player_ui_refresh(void)
{
    uint32_t  cur = 0;
    uint32_t  tot = 0;
    uint16_t  n;
    int16_t   idx;
    char      buf[64];
    char      num[12];

    if (pl_lbl_state == NULL)
    {
        return;
    }

    /* 1. 播放列表还没扫描完 */
    if (player_ready() == 0U)
    {
        lv_label_set_text(pl_lbl_title, "扫描中...");
        lv_label_set_text(pl_lbl_idx, "");
        lv_label_set_text(pl_lbl_state, "");
        lv_label_set_text(pl_lbl_time, "0:00 / 0:00");
        lv_bar_set_range(pl_bar, 0, 1);
        lv_bar_set_value(pl_bar, 0, LV_ANIM_OFF);
        if (pl_btn_play_label != NULL)
        {
            lv_label_set_text(pl_btn_play_label, "播放");
        }
        return;
    }

    n = player_count();

    /* 2. 目录里没有可播的文件 */
    if (n == 0U)
    {
        int8_t err = player_error();

        if (err == PLAYER_ERR_DIR)
        {
            lv_label_set_text(pl_lbl_title, "无音乐目录");
            lv_label_set_text(pl_lbl_state, "请建 0:/MUSIC 目录");
        }
        else if (err == PLAYER_ERR_MEM)
        {
            lv_label_set_text(pl_lbl_title, "初始化失败");
            lv_label_set_text(pl_lbl_state, "内存不足");
        }
        else
        {
            lv_label_set_text(pl_lbl_title, "无音乐文件");
            lv_label_set_text(pl_lbl_state, "请放入 wav 文件");
        }
        lv_label_set_text(pl_lbl_idx, "");
        lv_label_set_text(pl_lbl_time, "0:00 / 0:00");
        lv_bar_set_range(pl_bar, 0, 1);
        lv_bar_set_value(pl_bar, 0, LV_ANIM_OFF);
        if (pl_btn_play_label != NULL)
        {
            lv_label_set_text(pl_btn_play_label, "播放");
        }
        return;
    }

    idx = player_current();

    /* 3. 曲名与序号 */
    if ((idx < 0) || (idx >= (int16_t)n))
    {
        lv_label_set_text(pl_lbl_title, "未选择曲目");
        lv_label_set_text(pl_lbl_idx, "");
    }
    else
    {
        lv_label_set_text(pl_lbl_title, player_display_name((uint16_t)idx));

        strcpy(buf, "第 ");
        u32_to_str(num, (uint32_t)(idx + 1));
        strcat(buf, num);
        strcat(buf, "/");
        u32_to_str(num, (uint32_t)n);
        strcat(buf, num);
        strcat(buf, " 首");
        lv_label_set_text(pl_lbl_idx, buf);
    }

    /* 4. 状态与播放按钮文字 */
    if (player_state() == PLAYER_PLAY)
    {
        lv_label_set_text(pl_lbl_state, "播放中");
        if (pl_btn_play_label != NULL)
        {
            lv_label_set_text(pl_btn_play_label, "暂停");
        }
    }
    else if (player_state() == PLAYER_PAUSE)
    {
        lv_label_set_text(pl_lbl_state, "已暂停");
        if (pl_btn_play_label != NULL)
        {
            lv_label_set_text(pl_btn_play_label, "播放");
        }
    }
    else
    {
        lv_label_set_text(pl_lbl_state,
                          (player_error() == PLAYER_ERR_FORMAT) ? "格式不支持" : "已停止");
        if (pl_btn_play_label != NULL)
        {
            lv_label_set_text(pl_btn_play_label, "播放");
        }
    }

    /* 5. 进度与时间 */
    player_progress(&cur, &tot);
    if (cur > tot)
    {
        cur = tot;
    }

    lv_bar_set_range(pl_bar, 0, (tot > 0U) ? (int32_t)tot : 1);
    lv_bar_set_value(pl_bar, (int32_t)cur, LV_ANIM_OFF);

    {
        char t1[12];
        char t2[12];
        sec_to_mmss(t1, cur);
        sec_to_mmss(t2, tot);
        strcpy(buf, t1);
        strcat(buf, " / ");
        strcat(buf, t2);
        lv_label_set_text(pl_lbl_time, buf);
    }

    /* 6. 音量 */
    u32_to_str(buf, (uint32_t)player_volume());
    lv_label_set_text(pl_lbl_vol, buf);
}

static void player_ui_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    player_ui_refresh();
}

/* ------------------------------------------------------------------------------------------------
 * 按键事件
 * ---------------------------------------------------------------------------------------------- */

static void player_ui_back_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED)
    {
        return;
    }

    if ((pl_prev_scr != NULL) && lv_obj_is_valid(pl_prev_scr))
    {
        lv_scr_load(pl_prev_scr);
    }
    else
    {
        setup_scr_screen(&guider_ui);
        lv_scr_load(guider_ui.screen);
    }
    pl_prev_scr = NULL;
}

static void player_ui_prev_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED)
    {
        player_prev();
        player_ui_refresh();
    }
}

static void player_ui_next_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED)
    {
        player_next();
        player_ui_refresh();
    }
}

static void player_ui_play_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED)
    {
        player_toggle();
        player_ui_refresh();
    }
}

static void player_ui_vol_down_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED)
    {
        player_volume_step(-4);
        player_ui_refresh();
    }
}

static void player_ui_vol_up_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED)
    {
        player_volume_step(4);
        player_ui_refresh();
    }
}

static void player_ui_screen_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_SCREEN_LOADED)
    {
        player_ui_refresh();
        if (pl_timer != NULL)
        {
            lv_timer_resume(pl_timer);
        }
    }
    else if (code == LV_EVENT_SCREEN_UNLOADED)
    {
        if (pl_timer != NULL)
        {
            lv_timer_pause(pl_timer);
        }
    }
}

/* ------------------------------------------------------------------------------------------------
 * 控件生成辅助
 * ---------------------------------------------------------------------------------------------- */

static lv_obj_t *make_label(lv_obj_t *parent, const char *text, lv_coord_t x, lv_coord_t y,
                            lv_coord_t w, lv_coord_t h, uint32_t color, lv_text_align_t align)
{
    lv_obj_t *lbl = lv_label_create(parent);

    lv_label_set_text(lbl, text);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_pos(lbl, x, y);
    lv_obj_set_size(lbl, w, h);
    lv_obj_set_style_border_width(lbl, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(lbl, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(lbl, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(lbl, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(lbl, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(lbl, &font, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lbl, lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(lbl, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(lbl, align, LV_PART_MAIN | LV_STATE_DEFAULT);
    return lbl;
}

/* 生成一个统一风格的蓝色按钮，返回按钮对象，label 通过 out_label 返回 */
static lv_obj_t *make_button(lv_obj_t *parent, const char *text, lv_coord_t x, lv_coord_t y,
                             lv_coord_t w, lv_coord_t h, uint32_t color, uint8_t font_size_big,
                             lv_obj_t **out_label)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_t *lbl;

    lv_obj_set_pos(btn, x, y);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_style_pad_all(btn, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_radius(btn, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(btn, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(btn, lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(btn, LV_GRAD_DIR_NONE, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(btn, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(btn, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);

    lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lbl, LV_PCT(100));
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_font(lbl, font_size_big ? &lv_font_montserrat_20 : &font,
                               LV_PART_MAIN | LV_STATE_DEFAULT);

    if (out_label != NULL)
    {
        *out_label = lbl;
    }
    return btn;
}

/* ------------------------------------------------------------------------------------------------
 * 播放界面构建
 * ---------------------------------------------------------------------------------------------- */

static void player_ui_build(void)
{
    lv_obj_t *lbl;
    lv_obj_t *btn;

    pl_scr = lv_obj_create(NULL);
    lv_obj_set_size(pl_scr, 320, 240);
    lv_obj_set_scrollbar_mode(pl_scr, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(pl_scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(pl_scr, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(pl_scr, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(pl_scr, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(pl_scr, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    /* --- 返回 --- */
    btn = make_button(pl_scr, "<", 0, 1, 54, 27, 0x2195f6, 1, NULL);
    lv_obj_add_event_cb(btn, player_ui_back_cb, LV_EVENT_ALL, NULL);

    /* --- 曲名 / 序号 / 状态 --- */
    pl_lbl_title = make_label(pl_scr, "-", 58, 2, 258, 26, 0x048de6, LV_TEXT_ALIGN_LEFT);
    pl_lbl_idx   = make_label(pl_scr, "", 2, 32, 150, 24, 0x000000, LV_TEXT_ALIGN_LEFT);
    pl_lbl_state = make_label(pl_scr, "", 162, 32, 156, 24, 0xff6500, LV_TEXT_ALIGN_RIGHT);

    /* --- 进度条 --- */
    pl_bar = lv_bar_create(pl_scr);
    lv_obj_set_pos(pl_bar, 2, 60);
    lv_obj_set_size(pl_bar, 316, 18);
    lv_bar_set_range(pl_bar, 0, 1);
    lv_bar_set_value(pl_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_border_width(pl_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(pl_bar, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(pl_bar, lv_color_hex(0xe6e6e6), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(pl_bar, LV_GRAD_DIR_NONE, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(pl_bar, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(pl_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(pl_bar, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(pl_bar, lv_color_hex(0x2195f6), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(pl_bar, LV_GRAD_DIR_NONE, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(pl_bar, 4, LV_PART_INDICATOR | LV_STATE_DEFAULT);

    /* --- 时间 --- */
    pl_lbl_time = make_label(pl_scr, "0:00 / 0:00", 2, 80, 316, 24, 0x000000, LV_TEXT_ALIGN_RIGHT);

    /* --- 音量 --- */
    lbl = make_label(pl_scr, "音量", 2, 106, 56, 28, 0x000000, LV_TEXT_ALIGN_LEFT);
    (void)lbl;

    btn = make_button(pl_scr, "-", 60, 106, 40, 28, 0x2195f6, 1, NULL);
    lv_obj_add_event_cb(btn, player_ui_vol_down_cb, LV_EVENT_ALL, NULL);

    btn = make_button(pl_scr, "+", 104, 106, 40, 28, 0x2195f6, 1, NULL);
    lv_obj_add_event_cb(btn, player_ui_vol_up_cb, LV_EVENT_ALL, NULL);

    pl_lbl_vol = make_label(pl_scr, "0", 148, 106, 62, 28, 0xff6500, LV_TEXT_ALIGN_CENTER);

    /* --- 上一曲 / 播放暂停 / 下一曲 --- */
    btn = make_button(pl_scr, "上一曲", 4, 140, 100, 88, 0x2195f6, 0, NULL);
    lv_obj_add_event_cb(btn, player_ui_prev_cb, LV_EVENT_ALL, NULL);

    btn = make_button(pl_scr, "播放", 110, 140, 100, 88, 0x00b06b, 0, &pl_btn_play_label);
    lv_obj_add_event_cb(btn, player_ui_play_cb, LV_EVENT_ALL, NULL);

    btn = make_button(pl_scr, "下一曲", 216, 140, 100, 88, 0x2195f6, 0, NULL);
    lv_obj_add_event_cb(btn, player_ui_next_cb, LV_EVENT_ALL, NULL);

    /* --- 屏幕载入/离开：控制刷新定时器 --- */
    lv_obj_add_event_cb(pl_scr, player_ui_screen_cb, LV_EVENT_ALL, NULL);

    lv_obj_update_layout(pl_scr);
}

/* ------------------------------------------------------------------------------------------------
 * 对外接口
 * ---------------------------------------------------------------------------------------------- */

static void player_ui_entry_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED)
    {
        return;
    }

    if (pl_scr == NULL)
    {
        player_ui_build();
    }
    if (pl_scr == NULL)
    {
        return;
    }

    if (pl_timer == NULL)
    {
        /* 先暂停，等屏幕真正载入时再 resume */
        pl_timer = lv_timer_create(player_ui_timer_cb, PLAYER_UI_PERIOD, NULL);
        if (pl_timer != NULL)
        {
            lv_timer_pause(pl_timer);
        }
    }

    pl_prev_scr = lv_scr_act();
    lv_scr_load(pl_scr);
}

void player_ui_attach(lv_obj_t *parent, const char *btn_text)
{
    lv_obj_t *btn;

    if (parent == NULL)
    {
        return;
    }

    /* 首次使用时创建音频任务并开始扫描播放列表（幂等） */
    player_init();

    btn = make_button(parent, (btn_text != NULL) ? btn_text : "MUSIC",
                      203, 205, 64, 32, 0x2195f6, 0, NULL);
    lv_obj_add_event_cb(btn, player_ui_entry_cb, LV_EVENT_ALL, NULL);
}
