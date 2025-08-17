/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "lvgl.h"

void example_lvgl_demo_ui(lv_disp_t *disp)
{
    lv_obj_t *scr = lv_display_get_screen_active(disp);
    lv_obj_t *label = lv_label_create(scr);
    //lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS); /*Just plain text */
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS); /*Just plain text */
    lv_label_set_text(label, "Hello World!");
    lv_obj_set_width(label, lv_display_get_physical_horizontal_resolution(disp));
    //lv_obj_set_style_text_color(label, lv_color_white(), 0); // ensure visible on black bg
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 0);
}