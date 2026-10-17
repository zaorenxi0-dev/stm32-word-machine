/**
 ****************************************************************************************************
 * @file        calc_ui.h
 * @author      zhangguangmao
 * @version     V1.0
 * @date        2025-12-20
 * @brief       计算器界面：按键矩阵 + 表达式/结果显示，按键结果交给 calc.c 的双栈算法求值
 * @license     Copyright (c) 2025
 ****************************************************************************************************
 */

#ifndef __CALC_UI_H
#define __CALC_UI_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  在主界面上挂一个「计算器」入口按钮
 * @param  parent   : 按钮的父对象（一般传主界面 ui->screen）
 * @param  btn_text : 按钮文字（UTF-8，用调用方所在文件的字体渲染）
 * @retval 无
 * @note   计算器屏幕在第一次点击入口时才创建，之后复用，避免占用 LVGL 堆
 */
void calc_ui_attach(lv_obj_t *parent, const char *btn_text);

#ifdef __cplusplus
}
#endif

#endif /* __CALC_UI_H */
