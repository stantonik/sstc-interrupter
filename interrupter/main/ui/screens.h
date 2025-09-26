#ifndef EEZ_LVGL_UI_SCREENS_H
#define EEZ_LVGL_UI_SCREENS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _objects_t {
    lv_obj_t *main;
    lv_obj_t *spark;
    lv_obj_t *con_state;
    lv_obj_t *note;
    lv_obj_t *state;
    lv_obj_t *prf_range;
    lv_obj_t *prf_txt;
    lv_obj_t *prf_val_txt;
    lv_obj_t *dc_range;
    lv_obj_t *dc_txt;
    lv_obj_t *dc_val_txt;
} objects_t;

extern objects_t objects;

enum ScreensEnum {
    SCREEN_ID_MAIN = 1,
};

void create_screen_main();
void tick_screen_main();

void tick_screen_by_id(enum ScreensEnum screenId);
void tick_screen(int screen_index);

void create_screens();


#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_SCREENS_H*/