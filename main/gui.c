#include "main.h"

#include "lvgl.h"
#include "lvgl_helpers.h"

#include "gui.h"


/* DEFINES */
#define LV_TICK_PERIOD_MS   25
#define SCR_WIDTH           320
#define SCR_HEIGHT          240

/* STATIC VARIABLES */
static lv_obj_t* main_page = NULL;
static lv_obj_t* sensor_txt = NULL;
static lv_disp_buf_t disp_buf;

/* STATIC PROTOTYPES */
static void lv_tick_cb(void *arg);
static void gui_create_main_page(void);

/**
 * @brief GUI task function: create GUI components
 * 
 * @param pvParameter 
 */
void gui_task_fcn(void *pvParameter) {    
    /* initialize LVGL and underlying hardware */
    lv_init();
    lvgl_driver_init();

    /* prepare display buffer from heap */
    lv_disp_drv_t disp_drv;
    lv_color_t* px_buf = heap_caps_malloc(DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(px_buf != NULL);
    uint32_t size_in_px = (SCR_WIDTH*SCR_HEIGHT)/10; //DISP_BUF_SIZE;
    lv_disp_buf_init(&disp_buf, px_buf, NULL, size_in_px);
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res  = SCR_WIDTH;
    disp_drv.ver_res  = SCR_HEIGHT;
    disp_drv.buffer   = &disp_buf;    
    disp_drv.flush_cb = disp_driver_flush;
    disp_drv.buffer = &disp_buf;
    lv_disp_drv_register(&disp_drv);

    /* Create and start a periodic timer interrupt to call lv_tick_inc */
    const esp_timer_create_args_t tick_timer_args = {
        .callback = &lv_tick_cb,
        .name = "periodic_gui"
    };
    esp_timer_handle_t tick_timer;
    esp_timer_create(&tick_timer_args, &tick_timer);
    esp_timer_start_periodic(tick_timer, LV_TICK_PERIOD_MS * 1000);

    gui_create_main_page();

    while (1) {         
        if (xSemaphoreTake(label_txt_sem, pdMS_TO_TICKS(LV_TICK_PERIOD_MS)) == pdPASS ) {
            lv_label_set_text(sensor_txt, label_txt);
            xSemaphoreGive(label_txt_sem);
        }        
        lv_task_handler();        
    }

    /* A task should NEVER return */
    free(px_buf);
    vTaskDelete(NULL);
}

/**
 * @brief timer callback for generate LVGL tick
 * 
 * @param arg 
 */
static void lv_tick_cb(void *arg) {
    lv_tick_inc(LV_TICK_PERIOD_MS);
}

/**
 * @brief create main page for GUI
 * 
 */
static void gui_create_main_page() {
    // show init screen
    main_page = lv_win_create(lv_scr_act(), NULL);
    lv_win_set_title(main_page, "BLE2MQTT");
    // create text label
    sensor_txt = lv_label_create(main_page, NULL);
    lv_label_set_long_mode(sensor_txt, LV_LABEL_LONG_BREAK);
    lv_obj_set_width(sensor_txt, SCR_WIDTH-30);
    lv_label_set_recolor(sensor_txt, true);
    lv_label_set_text(sensor_txt, "Booting");
}
