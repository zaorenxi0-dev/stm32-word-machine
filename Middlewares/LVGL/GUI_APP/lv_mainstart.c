#include "LVGL/GUI_APP/lv_mainstart.h"
#include "lvgl.h"
#include <stdio.h>


/* 获取当前活动屏幕的宽高 */
#define scr_act_width()  lv_obj_get_width(lv_scr_act())
#define scr_act_height() lv_obj_get_height(lv_scr_act())
extern char buffer[1000];

    LV_FONT_DECLARE(font)//声明字体
	void lv_mainstart(void)
	{
    const char* str1 = "生活的代价是活着";
    lv_obj_t* label_dl_1 = lv_label_create(lv_scr_act());//新建lable
    lv_label_set_text(label_dl_1, buffer);//设置显示内容
    lv_obj_set_style_text_font(label_dl_1, &font, 0);//设置字体
    lv_obj_set_style_text_color(label_dl_1, lv_color_make(0xff, 0x00, 0x00), 0);//设置颜色
	}
