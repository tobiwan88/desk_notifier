/*
 * Example render task: cat_eyes -> two GC9A01 panels via esp_lcd (ESP-IDF 5.x/6.x).
 * NOT compiled by the component. Copy into your main/ (or merge into your existing
 * render task / compositor) and adapt.
 *
 * Assumes your board code already created both panels on one SPI bus with
 * separate CS pins, e.g. with esp_lcd_new_panel_io_spi() + esp_lcd_new_panel_gc9a01().
 * IMPORTANT: when creating each panel IO, set
 *     io_config.on_color_trans_done = cat_eyes_on_trans_done;
 * so the task knows when a DMA strip buffer is free again.
 */
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "cat_eyes.h"

#define STRIP_ROWS   24                 /* 240*24*2 = 11.5 KB per buffer */
#define N_BUFS       3                  /* render one while two are on the wire */
#define FRAME_MS     33                 /* ~30 fps */

static const char *TAG = "cat_eyes";

/* ---- commands from other tasks (HTTP handler etc.) ---- */
typedef enum { CMD_STYLE, CMD_IRIS, CMD_PLAY, CMD_AUTO, CMD_BLINK, CMD_LOOK } cat_cmd_kind_t;
typedef struct { cat_cmd_kind_t kind; int value; float gx, gy; uint32_t hold_ms; } cat_cmd_t;

static QueueHandle_t s_cmd_q;
static SemaphoreHandle_t s_buf_free;
static uint16_t *s_buf[N_BUFS];
static esp_lcd_panel_handle_t s_panel[2];

/* Thread-safe entry point for other tasks. Returns false if the queue is full. */
bool cat_eyes_post(const cat_cmd_t *cmd) { return s_cmd_q && xQueueSend(s_cmd_q, cmd, 0) == pdTRUE; }

/* Register as io_config.on_color_trans_done for BOTH panel IOs. */
bool cat_eyes_on_trans_done(esp_lcd_panel_io_handle_t io, esp_lcd_panel_io_event_data_t *ev, void *ctx)
{
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(s_buf_free, &woken);
    return woken == pdTRUE;
}

static void apply(const cat_cmd_t *c)
{
    switch (c->kind) {
    case CMD_STYLE: cat_eyes_set_style((cat_style_t)c->value); break;
    case CMD_IRIS:  cat_eyes_set_iris((cat_iris_t)c->value); break;
    case CMD_PLAY:  cat_eyes_play((cat_anim_t)c->value); break;
    case CMD_AUTO:  cat_eyes_set_auto(c->value != 0); break;
    case CMD_BLINK: cat_eyes_blink(); break;
    case CMD_LOOK:  cat_eyes_look(c->gx, c->gy, c->hold_ms); break;
    }
}

static void render_task(void *arg)
{
    int next = 0;
    TickType_t wake = xTaskGetTickCount();
    uint32_t stat_frames = 0; int64_t stat_us = 0;
    for (;;) {
        cat_cmd_t c;
        while (xQueueReceive(s_cmd_q, &c, 0) == pdTRUE) apply(&c);

        int64_t t0 = esp_timer_get_time();
        cat_eyes_tick((uint32_t)(t0 / 1000));
        for (int eye = 0; eye < 2; eye++) {
            int y0, y1;
            if (!cat_eyes_dirty_rows(eye, &y0, &y1)) continue;
            for (int y = y0; y < y1; y += STRIP_ROWS) {
                int h = (y1 - y < STRIP_ROWS) ? y1 - y : STRIP_ROWS;
                xSemaphoreTake(s_buf_free, portMAX_DELAY);       /* wait for a free DMA buffer */
                uint16_t *b = s_buf[next]; next = (next + 1) % N_BUFS;
                cat_eyes_render_strip(eye, y, h, b);
                esp_lcd_panel_draw_bitmap(s_panel[eye], 0, y, CAT_EYES_W, y + h, b); /* async */
            }
        }
        stat_us += esp_timer_get_time() - t0;
        if (++stat_frames == 150) {   /* every ~5 s: how much of the frame budget we use */
            ESP_LOGI(TAG, "avg frame %lld us (budget %d us), anim=%s",
                     stat_us / stat_frames, FRAME_MS * 1000, cat_eyes_anim_name(cat_eyes_current_anim()));
            stat_frames = 0; stat_us = 0;
        }
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(FRAME_MS));
    }
}

void cat_eyes_task_start(esp_lcd_panel_handle_t left, esp_lcd_panel_handle_t right)
{
    s_panel[0] = left; s_panel[1] = right;
    s_cmd_q = xQueueCreate(8, sizeof(cat_cmd_t));
    s_buf_free = xSemaphoreCreateCounting(N_BUFS, N_BUFS);
    for (int i = 0; i < N_BUFS; i++) {
        s_buf[i] = heap_caps_malloc(CAT_EYES_W * STRIP_ROWS * sizeof(uint16_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
        assert(s_buf[i]);
    }
    cat_eyes_init(esp_random());
    cat_eyes_set_swap_bytes(true);          /* GC9A01 wants big-endian RGB565 */
    cat_eyes_set_style(CAT_STYLE_ANIME);    /* pick your default */
    xTaskCreate(render_task, "cat_eyes", 4096, NULL, 5, NULL);
}

/* ------------------------------------------------------------------------
 * Optional: hook into an existing POST /api/eye handler (cJSON), e.g.
 *   {"style":"halloween","iris":"green","play":"wink","auto":true,"look":[0.3,-0.1,800]}
 * ------------------------------------------------------------------------ */
#if 0
#include "cJSON.h"
void cat_eyes_handle_json(const cJSON *j)
{
    cat_cmd_t c = {0}; const cJSON *v;
    if ((v = cJSON_GetObjectItem(j, "style")) && cJSON_IsString(v) && (c.value = cat_eyes_style_from_name(v->valuestring)) >= 0) { c.kind = CMD_STYLE; cat_eyes_post(&c); }
    if ((v = cJSON_GetObjectItem(j, "iris"))  && cJSON_IsString(v) && (c.value = cat_eyes_iris_from_name(v->valuestring))  >= 0) { c.kind = CMD_IRIS;  cat_eyes_post(&c); }
    if ((v = cJSON_GetObjectItem(j, "play"))  && cJSON_IsString(v)) {
        if (strcmp(v->valuestring, "blink") == 0) { c.kind = CMD_BLINK; cat_eyes_post(&c); }
        else if ((c.value = cat_eyes_anim_from_name(v->valuestring)) >= 0) { c.kind = CMD_PLAY; cat_eyes_post(&c); }
    }
    if ((v = cJSON_GetObjectItem(j, "auto"))  && cJSON_IsBool(v)) { c.kind = CMD_AUTO; c.value = cJSON_IsTrue(v); cat_eyes_post(&c); }
    if ((v = cJSON_GetObjectItem(j, "look"))  && cJSON_IsArray(v) && cJSON_GetArraySize(v) >= 2) {
        c.kind = CMD_LOOK; c.gx = (float)cJSON_GetArrayItem(v, 0)->valuedouble; c.gy = (float)cJSON_GetArrayItem(v, 1)->valuedouble;
        c.hold_ms = cJSON_GetArraySize(v) > 2 ? (uint32_t)cJSON_GetArrayItem(v, 2)->valueint : 1000; cat_eyes_post(&c);
    }
}
#endif
