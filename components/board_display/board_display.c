#include "board_display.h"
#include "sdkconfig.h"
#include "wifi_service.h"
#if defined(CONFIG_LISP_PERF_TRACE) && CONFIG_LISP_PERF_TRACE
#include "lisp_perf.h"
#endif

#if CONFIG_BOARD_DISPLAY_ENABLE

#if !CONFIG_BOARD_WROVER_KIT_V41_CONFIRMED
#error "Confirm the PCB v4.1 silkscreen and official LCD wiring before enabling the display"
#endif

#include <stdatomic.h>
#include <stdlib.h>

#include "display_probe_logic.h"
#if CONFIG_BOARD_DISPLAY_DMA_PIPELINE
#include "display_pipeline.h"
#endif
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_app_desc.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_lcd_ili9341.h"
#include "esp_lcd_io_spi.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

/* Only the confirmed ESP-WROVER-KIT v4.1 mapping. No SD, JTAG or PSRAM pins. */
enum {
    LCD_RESET = 18,
    LCD_SCLK = 19,
    LCD_DC = 21,
    LCD_CS = 22,
    LCD_MOSI = 23,
    LCD_MISO = 25,
    LCD_BACKLIGHT = 5,
    STRIP_BYTES = DISPLAY_WIDTH * DISPLAY_STRIP_ROWS * 2,
    DISPLAY_STACK_BYTES = 6144,
};
#define LCD_HOST SPI2_HOST
static const char *TAG = "board_display";
static atomic_bool started = false;
static atomic_uint last_frame_tick = 0;
static portMUX_TYPE lifecycle_lock = portMUX_INITIALIZER_UNLOCKED;
/* Zero initialized to STARTING before task creation. */
static display_lifecycle_t lifecycle;
static bool display_app_idle(void);
display_lifecycle_state_t board_display_status(void)
{
    portENTER_CRITICAL(&lifecycle_lock);
    display_lifecycle_state_t state = lifecycle.state;
    portEXIT_CRITICAL(&lifecycle_lock);
    return state;
}
bool board_display_request(bool on)
{
    bool app_idle = on || display_app_idle();
    portENTER_CRITICAL(&lifecycle_lock);
    bool accepted = display_lifecycle_request(&lifecycle, on, app_idle);
    portEXIT_CRITICAL(&lifecycle_lock);
    return accepted;
}
bool board_display_sd_acquire(void)
{
    portENTER_CRITICAL(&lifecycle_lock);
    bool accepted = display_lifecycle_sd_acquire(&lifecycle);
    portEXIT_CRITICAL(&lifecycle_lock);
    return accepted;
}
bool board_display_sd_release(void)
{
    portENTER_CRITICAL(&lifecycle_lock);
    bool accepted = display_lifecycle_sd_release(&lifecycle);
    portEXIT_CRITICAL(&lifecycle_lock);
    return accepted;
}
static void display_complete(display_lifecycle_state_t state)
{
    portENTER_CRITICAL(&lifecycle_lock);
    display_lifecycle_complete(&lifecycle, state);
    portEXIT_CRITICAL(&lifecycle_lock);
}
bool board_display_service_healthy(void)
{
    bool recent = board_display_is_healthy();
    portENTER_CRITICAL(&lifecycle_lock);
    bool healthy = display_lifecycle_health(&lifecycle, recent);
    portEXIT_CRITICAL(&lifecycle_lock);
    return healthy;
}
#if CONFIG_LISP_REPL_ENABLE
static portMUX_TYPE terminal_lock = portMUX_INITIALIZER_UNLOCKED;
static lcd_terminal_t terminal_snapshot;
esp_err_t board_display_terminal_update(const lcd_terminal_t *terminal)
{
    if (terminal == NULL || terminal->input_cursor >= LCD_TERMINAL_COLUMNS)
        return ESP_ERR_INVALID_ARG;
    portENTER_CRITICAL(&terminal_lock);
    terminal_snapshot = *terminal;
    portEXIT_CRITICAL(&terminal_lock);
    return ESP_OK;
}
#else
esp_err_t board_display_terminal_update(const lcd_terminal_t *terminal)
{
    (void)terminal;
    return ESP_ERR_NOT_SUPPORTED;
}
#endif

bool board_display_is_healthy(void)
{
    unsigned last = atomic_load(&last_frame_tick);
    return board_display_status() == DISPLAY_ON && last != 0 &&
           (TickType_t)(xTaskGetTickCount() - last) <= pdMS_TO_TICKS(3000);
}

#if CONFIG_LISP_APP_ENABLE
static lisp_app_canvas_t app_canvas;
static bool app_canvas_ready; /* Set before task startup, immutable afterwards. */
static uint16_t *app_pixels;  /* Fixed lifetime, never reclaimed after task startup. */
static bool app_begin(void *user)
{
    (void)user;
    if (!app_canvas_ready || !board_display_is_healthy())
        return false;
    portENTER_CRITICAL(&terminal_lock);
    bool begun = lisp_app_canvas_begin(&app_canvas);
    portEXIT_CRITICAL(&terminal_lock);
    return begun;
}
static uint16_t *app_draw(void *user)
{
    (void)user;
    if (!app_canvas_ready || !board_display_is_healthy())
        return NULL;
    portENTER_CRITICAL(&terminal_lock);
    lisp_app_canvas_t snapshot = app_canvas;
    portEXIT_CRITICAL(&terminal_lock);
    /* Only the owner can publish. With no pending slot the LCD cannot change
     * these front/back indices; its front is immutable. Copy OUTSIDE the lock. */
    return lisp_app_canvas_draw(&snapshot);
}
static bool app_publish(void *user)
{
    (void)user;
    portENTER_CRITICAL(&terminal_lock);
    bool published = lisp_app_canvas_publish(&app_canvas);
    portEXIT_CRITICAL(&terminal_lock);
    return published;
}
static void app_activate(void *user)
{
    (void)user;
    portENTER_CRITICAL(&terminal_lock);
    lisp_app_canvas_activate(&app_canvas);
    portEXIT_CRITICAL(&terminal_lock);
}
static void app_restore(void *user)
{
    (void)user;
    portENTER_CRITICAL(&terminal_lock);
    lisp_app_canvas_restore(&app_canvas);
    portEXIT_CRITICAL(&terminal_lock);
}
static lisp_app_display_state_t app_state(void *user)
{
    (void)user;
    if (!app_canvas_ready || !board_display_is_healthy())
        return LISP_APP_DISPLAY_FAULT;
    portENTER_CRITICAL(&terminal_lock);
    lisp_app_display_state_t state = lisp_app_canvas_state(&app_canvas);
    portEXIT_CRITICAL(&terminal_lock);
    return state;
}
bool board_display_app_backend(lisp_app_display_t *backend)
{
    if (!backend || !app_canvas_ready)
        return false;
    *backend =
        (lisp_app_display_t){NULL, app_begin, app_draw, app_publish, app_activate, app_restore, app_state};
    return true;
}
static bool display_app_idle(void)
{
    if (!app_canvas_ready)
        return true;
    portENTER_CRITICAL(&terminal_lock);
    bool idle = lisp_app_canvas_state(&app_canvas) == LISP_APP_DISPLAY_TERMINAL;
    portEXIT_CRITICAL(&terminal_lock);
    return idle;
}
#else
bool board_display_app_backend(lisp_app_display_t *backend)
{
    (void)backend;
    return false;
}
static bool display_app_idle(void)
{
    return true;
}
#endif

typedef struct {
    esp_lcd_panel_io_handle_t io;
    esp_lcd_panel_handle_t panel;
    SemaphoreHandle_t transfer_done;
    uint8_t *strip;
#if CONFIG_BOARD_DISPLAY_DMA_PIPELINE
    uint8_t *next_strip;
#endif
    bool bus_initialized;
    bool backlight_configured;
    bool dma_uncertain;
} display_context_t;

static bool color_transfer_done(esp_lcd_panel_io_handle_t io, esp_lcd_panel_io_event_data_t *event,
                                void *user)
{
    (void)io;
    (void)event;
    display_context_t *context = user;
    BaseType_t higher_priority_woken = pdFALSE;
    xSemaphoreGiveFromISR(context->transfer_done, &higher_priority_woken);
    return higher_priority_woken == pdTRUE;
}

static esp_err_t reset_lcd(void)
{
    ESP_RETURN_ON_ERROR(gpio_set_level(LCD_RESET, 0), TAG, "reset low failed");
    vTaskDelay(pdMS_TO_TICKS(20));
    ESP_RETURN_ON_ERROR(gpio_set_level(LCD_RESET, 1), TAG, "reset high failed");
    vTaskDelay(pdMS_TO_TICKS(120));
    return ESP_OK;
}

static esp_err_t create_io(display_context_t *context, unsigned frequency)
{
    const esp_lcd_panel_io_spi_config_t config = {
        .cs_gpio_num = LCD_CS,
        .dc_gpio_num = LCD_DC,
        .spi_mode = 0,
        .pclk_hz = frequency,
        .trans_queue_depth = 1,
        .on_color_trans_done = color_transfer_done,
        .user_ctx = context,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    return esp_lcd_new_panel_io_spi(LCD_HOST, &config, &context->io);
}

static esp_err_t initialize_bus(display_context_t *context)
{
    /* Preload the OFF level before enabling the active-low backlight output. */
    ESP_RETURN_ON_ERROR(gpio_set_level(LCD_BACKLIGHT, 1), TAG, "backlight off failed");
    ESP_RETURN_ON_ERROR(gpio_set_level(LCD_RESET, 1), TAG, "reset preload failed");
    const gpio_config_t outputs = {
        .pin_bit_mask = (1ULL << LCD_BACKLIGHT) | (1ULL << LCD_RESET),
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&outputs), TAG, "LCD output config failed");
    context->backlight_configured = true;
    const spi_bus_config_t bus = {
        .sclk_io_num = LCD_SCLK,
        .mosi_io_num = LCD_MOSI,
        .miso_io_num = LCD_MISO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = STRIP_BYTES,
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(LCD_HOST, &bus, SPI_DMA_CH_AUTO), TAG, "SPI bus failed");
    context->bus_initialized = true;
    return create_io(context, 1000000);
}

static esp_err_t read_id_sample(display_context_t *context, display_id_sample_t *sample, unsigned round)
{
    for (unsigned i = 0; i < 3; i++) {
        /* ST7789V datasheet section 8.4.5: these single-byte reads have no dummy bit. */
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_rx_param(context->io, (int)(0xda + i), &sample->rdid[i], 1), TAG,
                            "RDID read failed");
    }
    for (unsigned i = 0; i < 2; i++) {
        /* ILI9341 indexed read: D9 selects ID4 bytes 2/3 (93/41), then read D3. */
        uint8_t index = (uint8_t)(0x12 + i);
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(context->io, 0xd9, &index, 1), TAG,
                            "ID4 read index failed");
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_rx_param(context->io, 0xd3, &sample->id4[i], 1), TAG,
                            "ID4 read failed");
    }
    ESP_LOGI(TAG, "ID read %u: RDID=%02x %02x %02x ID4(indexed)=%02x %02x", round, (unsigned)sample->rdid[0],
             (unsigned)sample->rdid[1], (unsigned)sample->rdid[2], (unsigned)sample->id4[0],
             (unsigned)sample->id4[1]);
    return ESP_OK;
}

static esp_err_t identify_lcd(display_context_t *context, display_model_t *hint)
{
    display_id_sample_t samples[2] = {0};
    for (unsigned i = 0; i < 2; i++) {
        ESP_RETURN_ON_ERROR(reset_lcd(), TAG, "probe reset failed");
        ESP_RETURN_ON_ERROR(read_id_sample(context, &samples[i], i + 1), TAG, "ID read failed");
    }
    *hint = display_probe_identify(&samples[0], &samples[1]);
    ESP_LOGI(TAG, "ID signature=%s; controller and glass still require visual confirmation",
             display_probe_model_name(*hint));
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_del(context->io), TAG, "probe IO deletion failed");
    context->io = NULL;
    /* Conservative 10 MHz pixel clock, distinct from the 1 MHz ID read. */
    return create_io(context, 10000000);
}

static esp_err_t begin_display(display_context_t *context)
{
    ESP_RETURN_ON_ERROR(gpio_set_level(LCD_BACKLIGHT, 1), TAG, "backlight off failed");
    if (context->panel != NULL) {
        ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(context->panel, false), TAG, "display off failed");
        ESP_RETURN_ON_ERROR(esp_lcd_panel_del(context->panel), TAG, "panel deletion failed");
        context->panel = NULL;
    }
    ESP_RETURN_ON_ERROR(reset_lcd(), TAG, "display reset failed");
    const esp_lcd_panel_dev_config_t config = {
        .reset_gpio_num = -1, /* The task owns the hardware reset, including during ID probing. */
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .data_endian = LCD_RGB_DATA_ENDIAN_BIG,
        .bits_per_pixel = 16,
    };
    esp_err_t error = esp_lcd_new_panel_ili9341(context->io, &config, &context->panel);
    ESP_RETURN_ON_ERROR(error, TAG, "panel creation failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(context->panel), TAG, "panel initialization failed");
    /* Confirmed ILI9341: fixed 320x240 landscape, no periodic direction changes. */
    ESP_RETURN_ON_ERROR(esp_lcd_panel_swap_xy(context->panel, true), TAG, "swap XY failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_mirror(context->panel, false, false), TAG, "mirror failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(context->panel, true), TAG, "display on failed");
    ESP_LOGI(TAG, "trial=A driver=ILI9341 id_match=yes; fixed 320x240 landscape, BGR, RGB565 BE, "
                  "SPI=10MHz; no controller cycling");
    ESP_LOGW(TAG, "SPI write success does NOT prove a visible image; operator photo/video required");
    return ESP_OK;
}

#if CONFIG_BOARD_DISPLAY_DMA_PIPELINE
typedef struct {
    display_context_t *context;
#if CONFIG_LISP_REPL_ENABLE
    const lcd_terminal_t *view;
    const uint16_t *pixels;
#else
    const display_probe_view_t *view;
#endif
#if defined(CONFIG_LISP_PERF_TRACE) && CONFIG_LISP_PERF_TRACE
    lisp_perf_token_t timing;
    uint64_t render_us, transfer_us, transfer_started;
#endif
} pipeline_frame_t;

static int pipeline_render(void *user, uint8_t *buffer, unsigned first_row, unsigned rows)
{
    pipeline_frame_t *frame = user;
#if defined(CONFIG_LISP_PERF_TRACE) && CONFIG_LISP_PERF_TRACE
    uint64_t render_started = frame->timing.active ? lisp_perf_now_us() : 0;
#endif
#if CONFIG_LISP_REPL_ENABLE
    bool rendered = frame->pixels
                        ? lisp_app_render_strip(frame->pixels, buffer, STRIP_BYTES, first_row, rows)
                        : lcd_terminal_render_strip(frame->view, buffer, STRIP_BYTES, first_row, rows);
#else
    bool rendered = display_probe_render_strip(buffer, STRIP_BYTES, first_row, rows, frame->view);
#endif
#if defined(CONFIG_LISP_PERF_TRACE) && CONFIG_LISP_PERF_TRACE
    if (frame->timing.active)
        frame->render_us += lisp_perf_now_us() - render_started;
#endif
    return rendered ? ESP_OK : ESP_ERR_INVALID_ARG;
}

static int pipeline_submit(void *user, const uint8_t *buffer, unsigned first_row, unsigned rows)
{
    pipeline_frame_t *frame = user;
#if defined(CONFIG_LISP_PERF_TRACE) && CONFIG_LISP_PERF_TRACE
    if (frame->timing.active)
        frame->transfer_started = lisp_perf_now_us();
#endif
    return esp_lcd_panel_draw_bitmap(frame->context->panel, 0, (int)first_row, DISPLAY_WIDTH,
                                     (int)(first_row + rows), buffer);
}

static int pipeline_wait(void *user)
{
    pipeline_frame_t *frame = user;
    if (xSemaphoreTake(frame->context->transfer_done, pdMS_TO_TICKS(2000)) != pdTRUE)
        return ESP_ERR_TIMEOUT;
#if defined(CONFIG_LISP_PERF_TRACE) && CONFIG_LISP_PERF_TRACE
    if (frame->timing.active)
        frame->transfer_us += lisp_perf_now_us() - frame->transfer_started;
#endif
    return ESP_OK;
}

#if CONFIG_LISP_REPL_ENABLE
static esp_err_t draw_frame(display_context_t *context, const lcd_terminal_t *view, const uint16_t *pixels)
#else
static esp_err_t draw_frame(display_context_t *context, const display_probe_view_t *view)
#endif
{
    pipeline_frame_t frame = {.context = context, .view = view};
#if CONFIG_LISP_REPL_ENABLE
    frame.pixels = pixels;
#endif
#if defined(CONFIG_LISP_PERF_TRACE) && CONFIG_LISP_PERF_TRACE
    frame.timing = pixels ? lisp_perf_begin() : (lisp_perf_token_t){0};
#endif
    const display_pipeline_ops_t ops = {&frame, pipeline_render, pipeline_submit, pipeline_wait};
    display_pipeline_result_t outcome = display_pipeline_run(&ops, context->strip, context->next_strip,
                                                             STRIP_BYTES, DISPLAY_HEIGHT, DISPLAY_STRIP_ROWS);
    if (outcome.status != DISPLAY_PIPELINE_OK) {
        context->dma_uncertain |= outcome.dma_uncertain;
        return outcome.status == DISPLAY_PIPELINE_INVALID ? ESP_ERR_INVALID_STATE : (esp_err_t)outcome.error;
    }
#if defined(CONFIG_LISP_PERF_TRACE) && CONFIG_LISP_PERF_TRACE
    lisp_perf_add(LISP_PERF_LCD_RENDER, frame.timing, frame.render_us);
    /* Submit-to-observed-completion includes any next-strip rendering that overlaps DMA. */
    lisp_perf_add(LISP_PERF_LCD_TRANSFER, frame.timing, frame.transfer_us);
    lisp_perf_end(LISP_PERF_LCD_FRAME, frame.timing);
#endif
    return ESP_OK;
}
#else
#if CONFIG_LISP_REPL_ENABLE
static esp_err_t draw_frame(display_context_t *context, const lcd_terminal_t *view, const uint16_t *pixels)
#else
static esp_err_t draw_frame(display_context_t *context, const display_probe_view_t *view)
#endif
{
#if defined(CONFIG_LISP_PERF_TRACE) && CONFIG_LISP_PERF_TRACE
    lisp_perf_token_t timing = pixels ? lisp_perf_begin() : (lisp_perf_token_t){0};
    uint64_t render_us = 0, transfer_us = 0;
#endif
    for (unsigned y = 0; y < DISPLAY_HEIGHT; y += DISPLAY_STRIP_ROWS) {
#if defined(CONFIG_LISP_PERF_TRACE) && CONFIG_LISP_PERF_TRACE
        uint64_t section_started = timing.active ? lisp_perf_now_us() : 0;
#endif
#if CONFIG_LISP_REPL_ENABLE
        bool rendered =
            pixels ? lisp_app_render_strip(pixels, context->strip, STRIP_BYTES, y, DISPLAY_STRIP_ROWS)
                   : lcd_terminal_render_strip(view, context->strip, STRIP_BYTES, y, DISPLAY_STRIP_ROWS);
#else
        bool rendered = display_probe_render_strip(context->strip, STRIP_BYTES, y, DISPLAY_STRIP_ROWS, view);
#endif
        if (!rendered) {
            return ESP_ERR_INVALID_ARG;
        }
#if defined(CONFIG_LISP_PERF_TRACE) && CONFIG_LISP_PERF_TRACE
        if (timing.active) {
            uint64_t now = lisp_perf_now_us();
            render_us += now - section_started;
            section_started = now;
        }
#endif
        esp_err_t error = esp_lcd_panel_draw_bitmap(context->panel, 0, (int)y, DISPLAY_WIDTH,
                                                    (int)(y + DISPLAY_STRIP_ROWS), context->strip);
        if (error != ESP_OK || xSemaphoreTake(context->transfer_done, pdMS_TO_TICKS(2000)) != pdTRUE) {
            /* Never free/reuse a buffer or ISR context while DMA completion is uncertain. */
            context->dma_uncertain = true;
            return error != ESP_OK ? error : ESP_ERR_TIMEOUT;
        }
#if defined(CONFIG_LISP_PERF_TRACE) && CONFIG_LISP_PERF_TRACE
        if (timing.active)
            transfer_us += lisp_perf_now_us() - section_started;
#endif
    }
#if defined(CONFIG_LISP_PERF_TRACE) && CONFIG_LISP_PERF_TRACE
    lisp_perf_add(LISP_PERF_LCD_RENDER, timing, render_us);
    lisp_perf_add(LISP_PERF_LCD_TRANSFER, timing, transfer_us);
    lisp_perf_end(LISP_PERF_LCD_FRAME, timing);
#endif
    return ESP_OK;
}
#endif /* CONFIG_BOARD_DISPLAY_DMA_PIPELINE */

/* SD card detect shares GPIO21 with LCD D/C. The PCB has a pull-up and
 * card switch; require several high samples before driving LCD pins. This
 * cannot protect against insertion after sampling: callers must not insert
 * or remove a card while LCD is active. */
static esp_err_t card_absent(bool *absent)
{
    const gpio_config_t input = {
        .pin_bit_mask = 1ULL << LCD_DC,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };
    esp_err_t error = gpio_config(&input);
    if (error != ESP_OK)
        return error;
    *absent = true;
    for (unsigned i = 0; i < 5; i++) {
        if (gpio_get_level(LCD_DC) == 0)
            *absent = false;
        vTaskDelay(pdMS_TO_TICKS(2) ? pdMS_TO_TICKS(2) : 1);
    }
    return ESP_OK;
}
static int shutdown_step(void *user, display_release_step_t step)
{
    display_context_t *context = user;
    esp_err_t error;
    switch (step) {
    case DISPLAY_RELEASE_BACKLIGHT:
        return gpio_set_level(LCD_BACKLIGHT, 1);
    case DISPLAY_RELEASE_PANEL_OFF:
        return esp_lcd_panel_disp_on_off(context->panel, false);
    case DISPLAY_RELEASE_PANEL_DELETE:
        error = esp_lcd_panel_del(context->panel);
        if (error == ESP_OK)
            context->panel = NULL;
        return error;
    case DISPLAY_RELEASE_IO_DELETE:
        error = esp_lcd_panel_io_del(context->io);
        if (error == ESP_OK)
            context->io = NULL;
        return error;
    case DISPLAY_RELEASE_BUS_FREE:
        error = spi_bus_free(LCD_HOST);
        if (error == ESP_OK)
            context->bus_initialized = false;
        return error;
    case DISPLAY_RELEASE_PINS:
        /* Keep RESET and CS inactive and backlight off; only D/C is handed
         * to SD detection. SPI bus teardown resets its own pins. */
        error = gpio_set_level(LCD_CS, 1);
        if (error != ESP_OK)
            return error;
        error = gpio_set_direction(LCD_CS, GPIO_MODE_OUTPUT);
        if (error != ESP_OK)
            return error;
        error = gpio_set_level(LCD_RESET, 1);
        if (error != ESP_OK)
            return error;
        return gpio_set_direction(LCD_DC, GPIO_MODE_INPUT);
    default:
        return ESP_ERR_INVALID_ARG;
    }
}

static void release_resources(display_context_t *context)
{
    if (context->backlight_configured) {
        (void)gpio_set_level(LCD_BACKLIGHT, 1);
    }
    if (context->dma_uncertain) {
        ESP_LOGE(TAG, "DMA or teardown uncertain; resources quarantined until reset, no retry");
        return;
    }
    if (context->panel != NULL) {
        (void)esp_lcd_panel_del(context->panel);
    }
    if (context->io != NULL && esp_lcd_panel_io_del(context->io) != ESP_OK) {
        ESP_LOGE(TAG, "IO cleanup failed; retaining ISR context and buffers until reset");
        return;
    }
    if (context->bus_initialized) {
        (void)spi_bus_free(LCD_HOST);
    }
    vSemaphoreDelete(context->transfer_done);
    heap_caps_free(context->strip);
#if CONFIG_BOARD_DISPLAY_DMA_PIPELINE
    heap_caps_free(context->next_strip);
#endif
    free(context);
}

static void display_task(void *argument)
{
    display_context_t *context = argument;
    display_model_t hint = DISPLAY_UNKNOWN;
    display_model_t plan[2];
    esp_err_t error = ESP_OK;
    bool absent = false;
    while (error == ESP_OK) {
        display_lifecycle_state_t state = board_display_status();
        if (state == DISPLAY_OFF || state == DISPLAY_SD_BLOCKED) {
            vTaskDelay(pdMS_TO_TICKS(10) ? pdMS_TO_TICKS(10) : 1);
            continue;
        }
        if (state != DISPLAY_STARTING) {
            error = ESP_ERR_INVALID_STATE;
            break;
        }
        error = card_absent(&absent);
        if (error != ESP_OK)
            break;
        if (!absent) {
            display_complete(DISPLAY_SD_BLOCKED);
            ESP_LOGW(TAG, "SD card detected; LCD start blocked");
            continue;
        }
        error = initialize_bus(context);
        if (error == ESP_OK) {
            error = identify_lcd(context, &hint);
        }
        size_t count = display_probe_plan(hint, plan);
        if (error == ESP_OK && (count != 1 || plan[0] != DISPLAY_ILI9341)) {
            ESP_LOGE(TAG, "Expected confirmed ILI9341 ID; stopping LCD, no alternate-driver trial");
            error = ESP_ERR_NOT_SUPPORTED;
        }
        if (error == ESP_OK) {
            error = begin_display(context);
        }
        uint32_t total_frames = 0;
        TickType_t last_frame = xTaskGetTickCount();
        while (error == ESP_OK) {
            if (board_display_status() == DISPLAY_STOPPING)
                break;
#if CONFIG_LISP_REPL_ENABLE
            /* Anchor each frame independently: an SPI overrun must not accumulate
             * periodic catch-up debt when the next frame is ready. */
            last_frame = xTaskGetTickCount();
            lcd_terminal_t view;
            const uint16_t *pixels = NULL;
#if CONFIG_LISP_APP_ENABLE
            lisp_app_frame_t app_frame = {0};
            bool frame_acquired = false;
#endif
            portENTER_CRITICAL(&terminal_lock);
            view = terminal_snapshot;
#if CONFIG_LISP_APP_ENABLE
            if (app_canvas_ready) {
                frame_acquired = lisp_app_canvas_acquire(&app_canvas, &app_frame);
                pixels = app_frame.pixels;
            }
#endif
            portEXIT_CRITICAL(&terminal_lock);
#if CONFIG_LISP_APP_ENABLE
            if (app_canvas_ready && !frame_acquired) {
                error = ESP_ERR_INVALID_STATE;
                break;
            }
#endif
#else
            wifi_scan_snapshot_t wifi = wifi_scan_initial();
            (void)wifi_service_get_snapshot(&wifi);
            wifi_link_snapshot_t link = wifi_link_initial();
            (void)wifi_service_get_link_snapshot(&link);
            const display_probe_view_t view = {
                .model = DISPLAY_ILI9341,
                .id_match = true,
                .uptime_s = (uint64_t)(esp_timer_get_time() / 1000000),
                .free_heap_bytes = esp_get_free_heap_size(),
                .firmware = esp_app_get_description()->version,
                .wifi = wifi,
                .link = link,
            };
#endif
#if CONFIG_LISP_REPL_ENABLE
            error = draw_frame(context, &view, pixels);
#if CONFIG_LISP_APP_ENABLE
            if (frame_acquired) {
                portENTER_CRITICAL(&terminal_lock);
                lisp_app_canvas_complete(&app_canvas, &app_frame, error == ESP_OK);
                portEXIT_CRITICAL(&terminal_lock);
            }
#endif
#else
            error = draw_frame(context, &view);
#endif
            if (error != ESP_OK) {
                break;
            }
            if (total_frames == 0) {
                error = gpio_set_level(LCD_BACKLIGHT, 0);
            }
            if (error == ESP_OK) {
                atomic_store(&last_frame_tick, (unsigned)xTaskGetTickCount());
                if (total_frames == 0)
                    display_complete(DISPLAY_ON);
            }
            total_frames++;
#if CONFIG_LISP_REPL_ENABLE
            if (total_frames % 50 == 1) {
#else
            if (total_frames % 10 == 1) {
#endif
                ESP_LOGI(TAG, "driver=%s submitted_frames=%lu heap=%lu internal=%u stack_low=%u bytes",
                         display_probe_model_name(DISPLAY_ILI9341), (unsigned long)total_frames,
                         (unsigned long)esp_get_free_heap_size(),
                         (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                         (unsigned)uxTaskGetStackHighWaterMark(NULL));
            }
#if CONFIG_LISP_REPL_ENABLE
            xTaskDelayUntil(&last_frame, pdMS_TO_TICKS(100));
#else
            xTaskDelayUntil(&last_frame, pdMS_TO_TICKS(1000));
#endif
        }
        atomic_store(&last_frame_tick, 0);
        if (error != ESP_OK)
            break;
        const display_release_ops_t ops = {context, shutdown_step};
        int shutdown_error = display_lifecycle_shutdown(&ops, context->dma_uncertain);
        if (shutdown_error != 0) {
            error = shutdown_error > 0 ? (esp_err_t)shutdown_error : ESP_ERR_INVALID_STATE;
            /* Some resources may already have been removed. Do not attempt
             * cleanup again; owner, DMA buffers and any remaining handles stay
             * quarantined until reset. */
            context->dma_uncertain = true;
            break;
        }
        context->backlight_configured = false;
        display_complete(DISPLAY_OFF);
        ESP_LOGI(TAG, "LCD stopped; GPIO21 available for SD ownership");
    }
    atomic_store(&last_frame_tick, 0);
    display_complete(DISPLAY_FAULT);
    ESP_LOGE(TAG, "LCD diagnostic stopped: %s; heartbeat remains independent", esp_err_to_name(error));
    release_resources(context);
    vTaskDelete(NULL);
}

esp_err_t board_display_start(void)
{
    bool expected = false;
    if (!atomic_compare_exchange_strong(&started, &expected, true)) {
        return ESP_ERR_INVALID_STATE;
    }
#if CONFIG_LISP_REPL_ENABLE
    lcd_terminal_init(&terminal_snapshot);
#endif
    display_context_t *context = calloc(1, sizeof(*context));
    if (context == NULL) {
        atomic_store(&started, false);
        return ESP_ERR_NO_MEM;
    }
    context->strip = heap_caps_malloc(STRIP_BYTES, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
#if CONFIG_BOARD_DISPLAY_DMA_PIPELINE
    context->next_strip = heap_caps_malloc(STRIP_BYTES, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
#endif
#if CONFIG_LISP_APP_ENABLE
    app_pixels = heap_caps_aligned_calloc(8, 2 * LISP_APP_PIXELS, sizeof(uint16_t),
                                          MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (app_pixels)
        app_canvas_ready =
            lisp_app_canvas_init(&app_canvas, app_pixels, app_pixels + LISP_APP_PIXELS, LISP_APP_PIXELS);
    if (!app_canvas_ready)
        ESP_LOGE(TAG, "Application canvases unavailable; terminal remains usable, no allocation retry");
#endif
    context->transfer_done = xSemaphoreCreateBinary();
    if (context->strip == NULL || context->transfer_done == NULL ||
#if CONFIG_BOARD_DISPLAY_DMA_PIPELINE
        context->next_strip == NULL ||
#endif
        xTaskCreate(display_task, "lcd_diagnostic", DISPLAY_STACK_BYTES, context, 1, NULL) != pdPASS) {
        if (context->transfer_done != NULL) {
            vSemaphoreDelete(context->transfer_done);
        }
        heap_caps_free(context->strip);
#if CONFIG_BOARD_DISPLAY_DMA_PIPELINE
        heap_caps_free(context->next_strip);
#endif
#if CONFIG_LISP_APP_ENABLE
        heap_caps_free(app_pixels);
        app_pixels = NULL;
        app_canvas_ready = false;
#endif
        free(context);
        atomic_store(&started, false);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

#else
bool board_display_request(bool on)
{
    (void)on;
    return false;
}
display_lifecycle_state_t board_display_status(void)
{
    return DISPLAY_FAULT;
}
bool board_display_sd_acquire(void)
{
    return false;
}
bool board_display_sd_release(void)
{
    return false;
}
bool board_display_service_healthy(void)
{
    return false;
}
bool board_display_app_backend(lisp_app_display_t *backend)
{
    (void)backend;
    return false;
}
esp_err_t board_display_terminal_update(const lcd_terminal_t *terminal)
{
    (void)terminal;
    return ESP_ERR_NOT_SUPPORTED;
}
bool board_display_is_healthy(void)
{
    return false;
}
esp_err_t board_display_start(void)
{
    return ESP_ERR_NOT_SUPPORTED;
}
#endif
