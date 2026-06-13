#include "display.h"
#include "config.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

static const char *TAG = "display";

/* SSD1306 I2C address */
#define DISPLAY_ADDR         0x3C
#define DISPLAY_I2C_NUM      I2C_NUM_0  /* share I2C1 with ADS1115 #1 */

/* Display dimensions */
#define DISPLAY_WIDTH        128
#define DISPLAY_HEIGHT       64
#define DISPLAY_PAGES        (DISPLAY_HEIGHT / 8)

static uint8_t framebuf[DISPLAY_WIDTH * DISPLAY_PAGES];
static bool display_active = false;

/* ---- Low-level SSD1306 commands ---- */

static void ssd1306_write_cmd(uint8_t cmd) {
    i2c_cmd_handle_t handle = i2c_cmd_link_create();
    i2c_master_start(handle);
    i2c_master_write_byte(handle, (DISPLAY_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(handle, 0x00, true);  /* command mode */
    i2c_master_write_byte(handle, cmd, true);
    i2c_master_stop(handle);
    i2c_master_cmd_begin(DISPLAY_I2C_NUM, handle, pdMS_TO_TICKS(10));
    i2c_cmd_link_delete(handle);
}

static void ssd1306_write_data(const uint8_t *data, size_t len) {
    i2c_cmd_handle_t handle = i2c_cmd_link_create();
    i2c_master_start(handle);
    i2c_master_write_byte(handle, (DISPLAY_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(handle, 0x40, true);  /* data mode */
    i2c_master_write(handle, data, len, true);
    i2c_master_stop(handle);
    i2c_master_cmd_begin(DISPLAY_I2C_NUM, handle, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(handle);
}

static void ssd1306_set_pos(uint8_t page, uint8_t col) {
    ssd1306_write_cmd(0xB0 | (page & 0x07));    /* page address */
    ssd1306_write_cmd(0x00 | (col & 0x0F));      /* lower column */
    ssd1306_write_cmd(0x10 | ((col >> 4) & 0x0F)); /* upper column */
}

/* ---- Pixel drawing (5x7 font) ---- */

static const uint8_t font_5x7[96][5] = {
    {0x00,0x00,0x00,0x00,0x00}, /* space */
    {0x00,0x00,0x5F,0x00,0x00}, /* ! */
    {0x00,0x07,0x00,0x07,0x00}, /* " */
    {0x14,0x7F,0x14,0x7F,0x14}, /* # */
    {0x24,0x2A,0x7F,0x2A,0x12}, /* $ */
    {0x23,0x13,0x08,0x64,0x62}, /* % */
    {0x36,0x49,0x55,0x22,0x50}, /* & */
    {0x00,0x05,0x03,0x00,0x00}, /* ' */
    {0x00,0x1C,0x22,0x41,0x00}, /* ( */
    {0x00,0x41,0x22,0x1C,0x00}, /* ) */
    {0x08,0x2A,0x1C,0x2A,0x08}, /* * */
    {0x08,0x08,0x3E,0x08,0x08}, /* + */
    {0x00,0x50,0x30,0x00,0x00}, /* , */
    {0x08,0x08,0x08,0x08,0x08}, /* - */
    {0x00,0x60,0x60,0x00,0x00}, /* . */
    {0x20,0x10,0x08,0x04,0x02}, /* / */
    {0x3E,0x51,0x49,0x45,0x3E}, /* 0 */
    {0x00,0x42,0x7F,0x40,0x00}, /* 1 */
    {0x42,0x61,0x51,0x49,0x46}, /* 2 */
    {0x21,0x41,0x45,0x4B,0x31}, /* 3 */
    {0x18,0x14,0x12,0x7F,0x10}, /* 4 */
    {0x27,0x45,0x45,0x45,0x39}, /* 5 */
    {0x3C,0x4A,0x49,0x49,0x30}, /* 6 */
    {0x01,0x71,0x09,0x05,0x03}, /* 7 */
    {0x36,0x49,0x49,0x49,0x36}, /* 8 */
    {0x06,0x49,0x49,0x29,0x1E}, /* 9 */
    {0x00,0x36,0x36,0x00,0x00}, /* : */
    {0x00,0x56,0x36,0x00,0x00}, /* ; */
    {0x00,0x08,0x14,0x22,0x41}, /* < */
    {0x14,0x14,0x14,0x14,0x14}, /* = */
    {0x41,0x22,0x14,0x08,0x00}, /* > */
    {0x02,0x01,0x51,0x09,0x06}, /* ? */
    {0x32,0x49,0x79,0x41,0x3E}, /* @ */
    {0x7E,0x11,0x11,0x11,0x7E}, /* A */
    {0x7F,0x49,0x49,0x49,0x36}, /* B */
    {0x3E,0x41,0x41,0x41,0x22}, /* C */
    {0x7F,0x41,0x41,0x22,0x1C}, /* D */
    {0x7F,0x49,0x49,0x49,0x41}, /* E */
    {0x7F,0x09,0x09,0x01,0x01}, /* F */
    {0x3E,0x41,0x41,0x51,0x32}, /* G */
    {0x7F,0x08,0x08,0x08,0x7F}, /* H */
    {0x00,0x41,0x7F,0x41,0x00}, /* I */
    {0x20,0x40,0x41,0x3F,0x01}, /* J */
    {0x7F,0x08,0x14,0x22,0x41}, /* K */
    {0x7F,0x40,0x40,0x40,0x40}, /* L */
    {0x7F,0x02,0x04,0x02,0x7F}, /* M */
    {0x7F,0x04,0x08,0x10,0x7F}, /* N */
    {0x3E,0x41,0x41,0x41,0x3E}, /* O */
    {0x7F,0x09,0x09,0x09,0x06}, /* P */
    {0x3E,0x41,0x51,0x21,0x5E}, /* Q */
    {0x7F,0x09,0x19,0x29,0x46}, /* R */
    {0x46,0x49,0x49,0x49,0x31}, /* S */
    {0x01,0x01,0x7F,0x01,0x01}, /* T */
    {0x3F,0x40,0x40,0x40,0x3F}, /* U */
    {0x1F,0x20,0x40,0x20,0x1F}, /* V */
    {0x7F,0x20,0x18,0x20,0x7F}, /* W */
    {0x63,0x14,0x08,0x14,0x63}, /* X */
    {0x03,0x04,0x78,0x04,0x03}, /* Y */
    {0x61,0x51,0x49,0x45,0x43}, /* Z */
    {0x00,0x00,0x7F,0x41,0x41}, /* [ */
    {0x02,0x04,0x08,0x10,0x20}, /* \ */
    {0x41,0x41,0x7F,0x00,0x00}, /* ] */
    {0x04,0x02,0x01,0x02,0x04}, /* ^ */
    {0x40,0x40,0x40,0x40,0x40}, /* _ */
    {0x00,0x01,0x02,0x04,0x00}, /* ` */
    {0x20,0x54,0x54,0x54,0x78}, /* a */
    {0x7F,0x48,0x44,0x44,0x38}, /* b */
    {0x38,0x44,0x44,0x44,0x20}, /* c */
    {0x38,0x44,0x44,0x48,0x7F}, /* d */
    {0x38,0x54,0x54,0x54,0x18}, /* e */
    {0x08,0x7E,0x09,0x01,0x02}, /* f */
    {0x08,0x14,0x54,0x54,0x3C}, /* g */
    {0x7F,0x08,0x04,0x04,0x78}, /* h */
    {0x00,0x44,0x7D,0x40,0x00}, /* i */
    {0x20,0x40,0x44,0x3D,0x00}, /* j */
    {0x00,0x7F,0x10,0x28,0x44}, /* k */
    {0x00,0x41,0x7F,0x40,0x00}, /* l */
    {0x7C,0x04,0x18,0x04,0x78}, /* m */
    {0x7C,0x08,0x04,0x04,0x78}, /* n */
    {0x38,0x44,0x44,0x44,0x38}, /* o */
    {0x7C,0x14,0x14,0x14,0x08}, /* p */
    {0x08,0x14,0x14,0x18,0x7C}, /* q */
    {0x7C,0x08,0x04,0x04,0x08}, /* r */
    {0x48,0x54,0x54,0x54,0x20}, /* s */
    {0x04,0x3F,0x44,0x40,0x20}, /* t */
    {0x3C,0x40,0x40,0x20,0x7C}, /* u */
    {0x1C,0x20,0x40,0x20,0x1C}, /* v */
    {0x3C,0x40,0x30,0x40,0x3C}, /* w */
    {0x44,0x28,0x10,0x28,0x44}, /* x */
    {0x0C,0x50,0x50,0x50,0x3C}, /* y */
    {0x44,0x64,0x54,0x4C,0x44}, /* z */
};

static void fb_set_pixel(int x, int y, bool on) {
    if (x < 0 || x >= DISPLAY_WIDTH || y < 0 || y >= DISPLAY_HEIGHT) return;
    int page = y / 8;
    int bit = y % 8;
    if (on)
        framebuf[x + page * DISPLAY_WIDTH] |= (1 << bit);
    else
        framebuf[x + page * DISPLAY_WIDTH] &= ~(1 << bit);
}

static void fb_draw_char(int x, int y, char c) {
    if (c < 32 || c > 127) c = ' ';
    const uint8_t *glyph = font_5x7[c - 32];
    for (int col = 0; col < 5; col++) {
        for (int row = 0; row < 7; row++) {
            fb_set_pixel(x + col, y + row, glyph[col] & (1 << row));
        }
    }
}

static void fb_draw_text(int x, int y, const char *text) {
    int cx = x;
    while (*text) {
        if (*text == '\n') { cx = x; y += 9; text++; continue; }
        fb_draw_char(cx, y, *text);
        cx += 6;
        text++;
    }
}

static void fb_draw_line(int x0, int y0, int x1, int y1) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;
    while (1) {
        fb_set_pixel(x0, y0, true);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static void fb_clear(void) {
    memset(framebuf, 0, sizeof(framebuf));
}

static void fb_flush(void) {
    for (int page = 0; page < DISPLAY_PAGES; page++) {
        ssd1306_set_pos(page, 0);
        ssd1306_write_data(&framebuf[page * DISPLAY_WIDTH], DISPLAY_WIDTH);
    }
}

/* ---- Public API ---- */

void display_init(void) {
    ESP_LOGI(TAG, "Initializing SSD1306 OLED on I2C addr 0x%02X", DISPLAY_ADDR);

    /* Init sequence */
    ssd1306_write_cmd(0xAE);  /* display off */
    ssd1306_write_cmd(0xD5);  /* clock div */
    ssd1306_write_cmd(0x80);
    ssd1306_write_cmd(0xA8);  /* multiplex */
    ssd1306_write_cmd(0x3F);  /* 64 */
    ssd1306_write_cmd(0xD3);  /* display offset */
    ssd1306_write_cmd(0x00);
    ssd1306_write_cmd(0x40);  /* start line */
    ssd1306_write_cmd(0x8D);  /* charge pump */
    ssd1306_write_cmd(0x14);  /* enable */
    ssd1306_write_cmd(0x20);  /* memory mode */
    ssd1306_write_cmd(0x00);  /* horizontal */
    ssd1306_write_cmd(0xA1);  /* seg remap */
    ssd1306_write_cmd(0xC8);  /* com scan direction */
    ssd1306_write_cmd(0xDA);  /* com pins */
    ssd1306_write_cmd(0x12);
    ssd1306_write_cmd(0x81);  /* contrast */
    ssd1306_write_cmd(0xCF);
    ssd1306_write_cmd(0xD9);  /* pre-charge */
    ssd1306_write_cmd(0xF1);
    ssd1306_write_cmd(0xDB);  /* VCOM detect */
    ssd1306_write_cmd(0x40);
    ssd1306_write_cmd(0xA4);  /* display all on resume */
    ssd1306_write_cmd(0xA6);  /* normal display */
    ssd1306_write_cmd(0x2E);  /* deactivate scroll */
    ssd1306_write_cmd(0xAF);  /* display on */

    fb_clear();
    fb_flush();
    display_active = true;

    ESP_LOGI(TAG, "Display initialized");
}

void display_show_status(float dx, float dy, float dz,
                         uint8_t soc, int16_t rssi,
                         uint16_t error_mask, uint8_t confidence) {
    if (!display_active) display_wake();

    fb_clear();

    /* Title bar */
    fb_draw_line(0, 10, 127, 10);

    /* Direction vector */
    char buf[32];
    snprintf(buf, sizeof(buf), "Dir: %.3f %.3f %.3f", dx, dy, dz);
    fb_draw_text(0, 0, buf);

    /* Azimuth / Elevation */
    float elev = asinf(dz) * 57.2958f;
    float az = atan2f(dx, dy) * 57.2958f;
    if (az < 0) az += 360.0f;
    snprintf(buf, sizeof(buf), "AZ:%4.0f EL:%+3.0f", az, elev);
    fb_draw_text(0, 13, buf);

    /* Battery */
    snprintf(buf, sizeof(buf), "BAT: %3u%% %4ddBm", soc, rssi);
    fb_draw_text(0, 24, buf);

    /* Faults */
    if (error_mask == 0) {
        fb_draw_text(0, 36, "Status: OK");
    } else {
        snprintf(buf, sizeof(buf), "Fault: 0x%04X", error_mask);
        fb_draw_text(0, 36, buf);
    }

    /* Confidence bar */
    snprintf(buf, sizeof(buf), "Confidence: %u%%", confidence);
    fb_draw_text(0, 48, buf);
    int bar_w = (confidence * 100) / 255;
    if (bar_w > 100) bar_w = 100;
    for (int i = 0; i < bar_w; i++) fb_set_pixel(72 + i, 56, true);
    fb_draw_line(72, 55, 127, 55);
    fb_draw_line(72, 57, 127, 57);

    fb_flush();
}

void display_show_calib(const char *status, int progress, int total) {
    if (!display_active) display_wake();

    fb_clear();
    fb_draw_text(0, 0, "Calibration");
    fb_draw_text(0, 12, status);

    char buf[32];
    snprintf(buf, sizeof(buf), "%d / %d", progress, total);
    fb_draw_text(0, 24, buf);

    /* Progress bar */
    int bar_w = (progress * 100) / (total > 0 ? total : 1);
    if (bar_w > 100) bar_w = 100;
    for (int i = 0; i < bar_w; i++) {
        for (int y = 38; y < 48; y++) fb_set_pixel(i + 14, y, true);
    }
    fb_draw_line(13, 37, 113, 37);
    fb_draw_line(13, 49, 113, 49);

    fb_flush();
}

void display_sleep(void) {
    if (!display_active) return;
    ssd1306_write_cmd(0xAE);  /* display off */
    ssd1306_write_cmd(0x8D);  /* charge pump */
    ssd1306_write_cmd(0x10);  /* disable */
    display_active = false;
    ESP_LOGI(TAG, "Display sleep");
}

void display_wake(void) {
    if (display_active) return;
    ssd1306_write_cmd(0x8D);  /* charge pump */
    ssd1306_write_cmd(0x14);  /* enable */
    ssd1306_write_cmd(0xAF);  /* display on */
    display_active = true;
}
