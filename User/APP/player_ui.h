/**
 ****************************************************************************************************
 * @file        player_ui.h
 * @author      zhangguangmao
 * @version     V1.0
 * @date        2025-12-20
 * @brief       音乐播放界面：曲名 / 进度 / 时间 / 音量 / 上一曲 / 播放暂停 / 下一曲
 * @license     Copyright (c) 2025
 ****************************************************************************************************
 */

#ifndef __PLAYER_UI_H
#define __PLAYER_UI_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  在主界面上挂一个「音乐」入口按钮，并在首次调用时启动音频任务
 * @param  parent   : 按钮的父对象（一般传主界面 ui->screen）
 * @param  btn_text : 按钮文字（UTF-8，用调用方所在文件的字体渲染）
 * @retval 无
 */
void player_ui_attach(lv_obj_t *parent, const char *btn_text);

#ifdef __cplusplus
}
#endif

#endif /* __PLAYER_UI_H */
