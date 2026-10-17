/**
 ****************************************************************************************************
 * @file        calc_ui.c
 * @author      zhangguangmao
 * @version     V1.0
 * @date        2025-12-20
 * @brief       计算器界面实现：入口按钮 + 计算器屏幕（表达式显示 / 结果显示 / 20 键按钮矩阵）
 * @license     Copyright (c) 2025
 ****************************************************************************************************
 * @attention
 *
 *   屏幕布局（320 x 240）：
 *     - 左上角「<」返回按钮
 *     - 第 1 行上半区：表达式（右对齐、蓝色）
 *     - 第 1 行下半区：计算结果 / 错误提示（右对齐、橙色）
 *     - 下方 5 列 x 4 行按钮矩阵：
 *         7 8 9 ( )
 *         4 5 6 * /
 *         1 2 3 + -
 *         0 . DEL CLR =
 *
 *   按键逻辑：数字/运算符/括号直接追加到表达式缓冲；DEL 退格；CLR 清空；= 调用 calc_eval()
 *   完成一次双栈求值并把结果刷到结果行。
 ****************************************************************************************************
 */

#include "calc_ui.h"
#include "calc.h"
#include "gui_guider.h"

LV_FONT_DECLARE(font)

/* 表达式缓冲区长度：界面只有 320px 宽，够用即可 */
#define CALC_EXPR_MAX     48

/* 按键矩阵布局 */
#define CALC_COL_NUM      5
#define CALC_KEY_NUM      20

/* 按键索引（与 calc_keymap 中的顺序一一对应） */
#define CALC_KEY_DEL      17
#define CALC_KEY_CLR      18
#define CALC_KEY_EQ       19

/* 静态资源 ---------------------------------------------------------------- */
static lv_obj_t     *calc_scr      = NULL;      /* 计算器屏幕（首次进入时创建，之后复用） */
static lv_obj_t     *calc_prev_scr = NULL;      /* 进入计算器之前的屏幕，返回时恢复       */
static lv_obj_t     *calc_lbl_expr = NULL;      /* 表达式显示                             */
static lv_obj_t     *calc_lbl_ret  = NULL;      /* 结果显示                               */

static char          calc_expr[CALC_EXPR_MAX] = {0};
static unsigned int  calc_len = 0;

static const char *calc_keymap[] = {
    "7", "8", "9", "(",  ")",  "\n",
    "4", "5", "6", "*",  "/",  "\n",
    "1", "2", "3", "+",  "-",  "\n",
    "0", ".", "DEL", "CLR", "=", ""
};

/* ------------------------------------------------------------------------------------------------
 * 界面刷新
 * ---------------------------------------------------------------------------------------------- */

static void calc_ui_refresh_expr(void)
{
    if (calc_lbl_expr == NULL)
    {
        return;
    }
    lv_label_set_text(calc_lbl_expr, (calc_len > 0U) ? calc_expr : "0");
}

static void calc_ui_do_calc(void)
{
    double value = 0.0;
    char   text[24];
    int    rc;

    if (calc_lbl_ret == NULL)
    {
        return;
    }

    rc = calc_eval(calc_expr, &value);
    if (rc == CALC_OK)
    {
        calc_format(value, text, sizeof(text));
    }
    else
    {
        /* 表达式非法 / 除零，直接把错误码文案显示出来 */
        const char *msg = calc_err_str(rc);
        unsigned int i = 0;
        while ((msg[i] != '\0') && (i < (sizeof(text) - 1U)))
        {
            text[i] = msg[i];
            i++;
        }
        text[i] = '\0';
    }
    lv_label_set_text(calc_lbl_ret, text);
}

/* ------------------------------------------------------------------------------------------------
 * 按键处理
 * ---------------------------------------------------------------------------------------------- */

static void calc_ui_key_event_cb(lv_event_t *e)
{
    lv_obj_t *obj;
    uint32_t  id;

    if (lv_event_get_code(e) != LV_EVENT_CLICKED)
    {
        return;
    }

    obj = lv_event_get_target(e);
    id  = (uint32_t)lv_btnmatrix_get_selected_btn(obj);
    if ((id == (uint32_t)LV_BTNMATRIX_BTN_NONE) || (id >= CALC_KEY_NUM))
    {
        return;
    }

    if (id < CALC_KEY_DEL)                       /* 数字 / 运算符 / 括号 */
    {
        if (calc_len < (CALC_EXPR_MAX - 1U))
        {
            calc_expr[calc_len] = calc_keymap[id][0];
            calc_len++;
            calc_expr[calc_len] = '\0';
        }
        calc_ui_refresh_expr();
    }
    else if (id == CALC_KEY_DEL)                 /* 退格 */
    {
        if (calc_len > 0U)
        {
            calc_len--;
            calc_expr[calc_len] = '\0';
        }
        calc_ui_refresh_expr();
    }
    else if (id == CALC_KEY_CLR)                 /* 清空 */
    {
        calc_len      = 0U;
        calc_expr[0]  = '\0';
        calc_ui_refresh_expr();
        if (calc_lbl_ret != NULL)
        {
            lv_label_set_text(calc_lbl_ret, "0");
        }
    }
    else                                         /* '=' 触发双栈求值 */
    {
        calc_ui_do_calc();
    }
}

static void calc_ui_back_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED)
    {
        return;
    }

    if ((calc_prev_scr != NULL) && lv_obj_is_valid(calc_prev_scr))
    {
        lv_scr_load(calc_prev_scr);
    }
    else
    {
        /* 原屏幕已被 LVGL 回收，退回主界面 */
        setup_scr_screen(&guider_ui);
        lv_scr_load(guider_ui.screen);
    }
    calc_prev_scr = NULL;
}

/* ------------------------------------------------------------------------------------------------
 * 计算器屏幕构建
 * ---------------------------------------------------------------------------------------------- */

static void calc_ui_build(void)
{
    lv_obj_t *btn;
    lv_obj_t *lbl;
    lv_obj_t *btnm;

    calc_scr = lv_obj_create(NULL);
    lv_obj_set_size(calc_scr, 320, 240);
    lv_obj_set_scrollbar_mode(calc_scr, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(calc_scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(calc_scr, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(calc_scr, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(calc_scr, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(calc_scr, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    /* --- 返回按钮 --- */
    btn = lv_btn_create(calc_scr);
    lv_obj_set_pos(btn, 0, 1);
    lv_obj_set_size(btn, 54, 27);
    lv_obj_set_style_pad_all(btn, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_radius(btn, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(btn, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x2195f6), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(btn, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_event_cb(btn, calc_ui_back_event_cb, LV_EVENT_ALL, NULL);

    lbl = lv_label_create(btn);
    lv_label_set_text(lbl, "<");
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);

    /* --- 表达式 --- */
    calc_lbl_expr = lv_label_create(calc_scr);
    lv_label_set_text(calc_lbl_expr, "0");
    lv_label_set_long_mode(calc_lbl_expr, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_pos(calc_lbl_expr, 58, 2);
    lv_obj_set_size(calc_lbl_expr, 260, 26);
    lv_obj_set_style_border_width(calc_lbl_expr, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(calc_lbl_expr, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(calc_lbl_expr, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(calc_lbl_expr, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(calc_lbl_expr, lv_color_hex(0x048de6), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(calc_lbl_expr, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(calc_lbl_expr, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    /* --- 计算结果 --- */
    calc_lbl_ret = lv_label_create(calc_scr);
    lv_label_set_text(calc_lbl_ret, "0");
    lv_label_set_long_mode(calc_lbl_ret, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_pos(calc_lbl_ret, 2, 30);
    lv_obj_set_size(calc_lbl_ret, 316, 28);
    lv_obj_set_style_border_width(calc_lbl_ret, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(calc_lbl_ret, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(calc_lbl_ret, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(calc_lbl_ret, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(calc_lbl_ret, lv_color_hex(0xff6500), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(calc_lbl_ret, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(calc_lbl_ret, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    /* --- 按键矩阵 --- */
    btnm = lv_btnmatrix_create(calc_scr);
    lv_btnmatrix_set_map(btnm, calc_keymap);
    lv_obj_set_pos(btnm, 1, 62);
    lv_obj_set_size(btnm, 318, 177);

    lv_obj_set_style_border_width(btnm, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(btnm, lv_color_hex(0xc9c9c9), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(btnm, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(btnm, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(btnm, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(btnm, LV_GRAD_DIR_NONE, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(btnm, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(btnm, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(btnm, 2, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_text_font(btnm, &lv_font_montserrat_20, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(btnm, lv_color_hex(0xffffff), LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(btnm, 255, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(btnm, lv_color_hex(0x2195f6), LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(btnm, LV_GRAD_DIR_NONE, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(btnm, 1, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(btnm, lv_color_hex(0xc9c9c9), LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(btnm, 4, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(btnm, 0, LV_PART_ITEMS | LV_STATE_DEFAULT);

    /* 按下时高亮为橙色，便于确认触摸是否命中 */
    lv_obj_set_style_bg_color(btnm, lv_color_hex(0xff6500), LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_text_color(btnm, lv_color_hex(0xffffff), LV_PART_ITEMS | LV_STATE_PRESSED);

    lv_obj_add_event_cb(btnm, calc_ui_key_event_cb, LV_EVENT_ALL, NULL);

    lv_obj_update_layout(calc_scr);
}

/* ------------------------------------------------------------------------------------------------
 * 对外接口
 * ---------------------------------------------------------------------------------------------- */

static void calc_ui_entry_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED)
    {
        return;
    }

    if (calc_scr == NULL)
    {
        calc_ui_build();
    }
    if (calc_scr == NULL)                        /* LVGL 堆不足时会创建失败 */
    {
        return;
    }

    calc_prev_scr = lv_scr_act();
    calc_len      = 0U;
    calc_expr[0]  = '\0';
    calc_ui_refresh_expr();
    if (calc_lbl_ret != NULL)
    {
        lv_label_set_text(calc_lbl_ret, "0");
    }

    lv_scr_load(calc_scr);
}

void calc_ui_attach(lv_obj_t *parent, const char *btn_text)
{
    lv_obj_t *btn;
    lv_obj_t *lbl;

    if (parent == NULL)
    {
        return;
    }

    btn = lv_btn_create(parent);
    lv_obj_set_pos(btn, 52, 201);
    lv_obj_set_size(btn, 65, 37);
    lv_obj_set_style_pad_all(btn, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_radius(btn, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(btn, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x2195f6), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(btn, LV_GRAD_DIR_NONE, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(btn, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(btn, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(btn, &font, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_event_cb(btn, calc_ui_entry_event_cb, LV_EVENT_ALL, NULL);

    lbl = lv_label_create(btn);
    lv_label_set_text(lbl, (btn_text != NULL) ? btn_text : "CALC");
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lbl, LV_PCT(100));
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_font(lbl, &font, LV_PART_MAIN | LV_STATE_DEFAULT);
}
