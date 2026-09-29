#include "demo_ui.h"

#include "esp_check.h"

static const char *TAG = "demo_ui";
static lv_obj_t *s_pages[2];

static void page_gesture(lv_event_t *event)
{
    lv_indev_t *indev = lv_indev_active();
    if (indev == NULL) return;

    const lv_dir_t direction = lv_indev_get_gesture_dir(indev);
    lv_obj_t *current = lv_event_get_current_target(event);
    const uint32_t current_index = current == s_pages[0] ? 0 :
                                   (current == s_pages[1] ? 1 : 2);
    if (current_index > 1) return;

    if (direction == LV_DIR_LEFT && current_index == 0) {
        lv_screen_load_anim(s_pages[1], LV_SCREEN_LOAD_ANIM_MOVE_LEFT, 220, 0, false);
    } else if (direction == LV_DIR_RIGHT && current_index == 1) {
        lv_screen_load_anim(s_pages[0], LV_SCREEN_LOAD_ANIM_MOVE_RIGHT, 220, 0, false);
    }
}

static lv_obj_t *make_label(lv_obj_t *parent, const char *text, const lv_font_t *font,
                            lv_color_t color, lv_align_t align, int32_t y)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_align(label, align, 0, y);
    lv_obj_remove_flag(label, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return label;
}

static void make_card(lv_obj_t *parent, int32_t y, const char *title,
                      const char *subtitle, lv_color_t accent)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, 320, 66);
    lv_obj_align(card, LV_ALIGN_TOP_MID, 0, y);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x202631), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_radius(card, 14, 0);
    lv_obj_set_style_pad_all(card, 8, 0);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *dot = lv_obj_create(card);
    lv_obj_set_size(dot, 12, 42);
    lv_obj_align(dot, LV_ALIGN_LEFT_MID, 2, 0);
    lv_obj_set_style_bg_color(dot, accent, 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(dot, 0, 0);
    lv_obj_set_style_radius(dot, 6, 0);
    lv_obj_remove_flag(dot, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *heading = lv_label_create(card);
    lv_label_set_text(heading, title);
    lv_obj_set_style_text_color(heading, lv_color_white(), 0);
    lv_obj_set_style_text_font(heading, &lv_font_montserrat_14, 0);
    lv_obj_align(heading, LV_ALIGN_TOP_LEFT, 24, 2);
    lv_obj_remove_flag(heading, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *caption = lv_label_create(card);
    lv_label_set_text(caption, subtitle);
    lv_obj_set_style_text_color(caption, lv_color_hex(0xAAB2C0), 0);
    lv_obj_set_style_text_font(caption, &lv_font_montserrat_14, 0);
    lv_obj_align(caption, LV_ALIGN_BOTTOM_LEFT, 24, -1);
    lv_obj_remove_flag(caption, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
}

static lv_obj_t *make_page(lv_display_t *display, uint32_t index)
{
    lv_obj_t *screen = index == 0 ? lv_display_get_screen_active(display) : lv_obj_create(NULL);
    if (screen == NULL) return NULL;
    if (index == 0) lv_obj_clean(screen);

    lv_obj_set_style_bg_color(screen, lv_color_hex(0x0C1017), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(screen, 0, 0);
    lv_obj_set_scroll_dir(screen, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_snap_y(screen, LV_SCROLL_SNAP_NONE);
    /* Keep scrolling tied to the finger; do not throw or bounce at the edges. */
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_add_event_cb(screen, page_gesture, LV_EVENT_GESTURE, NULL);

    const char *title = index == 0 ? "Touch Demo" : "More Cards";
    make_label(screen, title, &lv_font_montserrat_28, lv_color_white(), LV_ALIGN_TOP_MID, 35);
    make_label(screen, index == 0 ? "Swipe left for next page" : "Swipe right to go back",
               &lv_font_montserrat_14, lv_color_hex(0x8F9AAA), LV_ALIGN_TOP_MID, 82);

    static const char *page0_titles[] = {
        "Display", "Touch", "Smooth scroll", "Page flip", "RGB565"
    };
    static const char *page0_subtitles[] = {
        "466 x 466 AMOLED", "FT3168 capacitive panel", "Drag up and down",
        "Swipe left or right", "CO5300 over QSPI"
    };
    static const char *page1_titles[] = {
        "Page two", "Touch points", "Vertical motion", "Horizontal motion", "Ready"
    };
    static const char *page1_subtitles[] = {
        "Independent scroll position", "Single-point tracking", "LVGL scroll container",
        "Animated page transition", "Touch driver is active"
    };
    const char **titles = index == 0 ? page0_titles : page1_titles;
    const char **subtitles = index == 0 ? page0_subtitles : page1_subtitles;
    const lv_color_t accents[] = {
        lv_color_hex(0xFF5A67), lv_color_hex(0x33C98A), lv_color_hex(0x47A9FF),
        lv_color_hex(0xBE83FF), lv_color_hex(0xFFC857)
    };
    for (int i = 0; i < 5; i++) {
        make_card(screen, 122 + i * 78, titles[i], subtitles[i], accents[i]);
    }

    lv_obj_t *page_indicator = lv_label_create(screen);
    lv_label_set_text(page_indicator, index == 0 ? "Page 1 / 2" : "Page 2 / 2");
    lv_obj_set_style_text_color(page_indicator, lv_color_hex(0xD9E1EC), 0);
    lv_obj_set_style_text_font(page_indicator, &lv_font_montserrat_14, 0);
    lv_obj_align(page_indicator, LV_ALIGN_TOP_MID, 0, 525);
    lv_obj_remove_flag(page_indicator, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return screen;
}

esp_err_t demo_ui_init(lv_display_t *display)
{
    ESP_RETURN_ON_FALSE(display != NULL, ESP_ERR_INVALID_ARG, TAG, "display is NULL");

    lv_display_set_default(display);
    s_pages[0] = make_page(display, 0);
    ESP_RETURN_ON_FALSE(s_pages[0] != NULL, ESP_ERR_NO_MEM, TAG, "create first page");
    s_pages[1] = make_page(display, 1);
    if (s_pages[1] == NULL) {
        lv_obj_delete(s_pages[0]);
        s_pages[0] = NULL;
        return ESP_ERR_NO_MEM;
    }

    lv_screen_load(s_pages[0]);
    return ESP_OK;
}
