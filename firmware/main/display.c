#include "display.h"
#include "config.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

static const char *TAG = "display";

/* ---- Sharp Memory LCD pin assignments (from config.h) ---- */
#define LCD_MOSI_GPIO       DISPLAY_MOSI_GPIO
#define LCD_SCLK_GPIO       DISPLAY_SCLK_GPIO
#define LCD_CS_GPIO         DISPLAY_CS_GPIO

/* ---- Display geometry ---- */
#define LCD_WIDTH           128
#define LCD_HEIGHT          64
#define LCD_LINES           (LCD_HEIGHT)
#define LCD_BYTES_PER_LINE  (LCD_WIDTH / 8)

/* ---- Sharp LCD protocol ---- */
#define SHARP_CMD_UPDATE    0x00   /* mode=0, VCOM bit added separately */
#define SHARP_TRAILER       0x00

static uint8_t framebuf[LCD_LINES][LCD_BYTES_PER_LINE];
static bool s_vcom = false;

/* ---- Low-level SPI bit-bang ---- */

static void lcd_spi_write_byte(uint8_t data) {
    for (int b = 7; b >= 0; b--) {
        gpio_set_level(LCD_SCLK_GPIO, 0);
        gpio_set_level(LCD_MOSI_GPIO, (data >> b) & 1);
        esp_rom_delay_us(1);
        gpio_set_level(LCD_SCLK_GPIO, 1);
        esp_rom_delay_us(1);
    }
}

static void lcd_cs_low(void)  { gpio_set_level(LCD_CS_GPIO, 0); }
static void lcd_cs_high(void) { gpio_set_level(LCD_CS_GPIO, 1); }

/* ---- Sharp LCD commands ---- */

static void sharp_send_command(void) {
    uint8_t cmd = SHARP_CMD_UPDATE;
    if (s_vcom) cmd |= 0x40;  /* VCOM bit in bit 6 */
    s_vcom = !s_vcom;
    lcd_spi_write_byte(cmd);
}

static void sharp_send_line(uint8_t line, const uint8_t *data) {
    /* Line address is 1-based, bit 7 = 0 */
    uint8_t addr = (line + 1) & 0x7F;
    lcd_spi_write_byte(addr);
    for (int i = 0; i < LCD_BYTES_PER_LINE; i++) {
        lcd_spi_write_byte(~data[i]);  /* Sharp: 0 = black, 1 = white. Invert for intuitive API */
    }
}

static void sharp_flush(void) {
    lcd_cs_low();
    sharp_send_command();
    for (int line = 0; line < LCD_LINES; line++) {
        sharp_send_line(line, framebuf[line]);
    }
    lcd_spi_write_byte(SHARP_TRAILER);
    lcd_cs_high();
    esp_rom_delay_us(6);  /* hold time */
}

/* ---- Framebuffer pixel ops ---- */

static void fb_clear(void) {
    memset(framebuf, 0xFF, sizeof(framebuf));  /* 0xFF = all white (Sharp: 0=black) */
}

static void fb_set_pixel(int x, int y, bool black) {
    if (x < 0 || x >= LCD_WIDTH || y < 0 || y >= LCD_HEIGHT) return;
    int byte_idx = x / 8;
    int bit_idx = 7 - (x % 8);  /* MSB = leftmost pixel */
    if (black)
        framebuf[y][byte_idx] &= ~(1 << bit_idx);   /* 0 = black */
    else
        framebuf[y][byte_idx] |= (1 << bit_idx);    /* 1 = white */
}

/* ---- 5x7 font (same as before) ---- */

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

static void fb_draw_char(int x, int y, char c) {
    if (c < 32 || c > 127) c = ' ';
    const uint8_t *glyph = font_5x7[c - 32];
    for (int col = 0; col < 5; col++) {
        for (int row = 0; row < 7; row++) {
            if (glyph[col] & (1 << row)) {
                fb_set_pixel(x + col, y + row, true);  /* true = black pixel */
            }
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

/* ---- Public API ---- */

void display_init(void) {
    ESP_LOGI(TAG, "Initializing Sharp Memory LCD on GPIO MOSI=%d SCLK=%d CS=%d",
             LCD_MOSI_GPIO, LCD_SCLK_GPIO, LCD_CS_GPIO);

    gpio_set_direction(LCD_MOSI_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_direction(LCD_SCLK_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_direction(LCD_CS_GPIO, GPIO_MODE_OUTPUT);

    gpio_set_level(LCD_MOSI_GPIO, 0);
    gpio_set_level(LCD_SCLK_GPIO, 0);
    lcd_cs_high();

    fb_clear();
    sharp_flush();  /* clear screen */

    ESP_LOGI(TAG, "Display initialized (MIP reflective, always-on image)");
}

void display_show_status(float dx, float dy, float dz,
                         uint8_t soc, int16_t rssi,
                         uint16_t error_mask, uint8_t confidence) {
    fb_clear();

    /* Title bar */
    fb_draw_line(0, 10, 127, 10);

    /* Direction vector */
    char buf[32];
    snprintf(buf, sizeof(buf), "Dir:%.3f %.3f %.3f", dx, dy, dz);
    fb_draw_text(0, 0, buf);

    /* Azimuth / Elevation */
    float elev = asinf(dz) * 57.2958f;
    float az = atan2f(dx, dy) * 57.2958f;
    if (az < 0) az += 360.0f;
    snprintf(buf, sizeof(buf), "AZ:%4.0f EL:%+3.0f", az, elev);
    fb_draw_text(0, 13, buf);

    /* Battery + Signal */
    snprintf(buf, sizeof(buf), "BAT:%3u%% %4ddBm", soc, rssi);
    fb_draw_text(0, 24, buf);

    /* Faults */
    if (error_mask == 0) {
        fb_draw_text(0, 36, "Status: OK");
    } else {
        snprintf(buf, sizeof(buf), "Fault:0x%04X", error_mask);
        fb_draw_text(0, 36, buf);
    }

    /* Confidence bar */
    snprintf(buf, sizeof(buf), "Conf:%u%%", (unsigned)(confidence * 100 / 255));
    fb_draw_text(0, 48, buf);
    int bar_w = (confidence * 100) / 255;
    if (bar_w > 100) bar_w = 100;
    for (int i = 0; i < bar_w; i++) fb_set_pixel(72 + i, 56, true);
    fb_draw_line(72, 55, 127, 55);
    fb_draw_line(72, 57, 127, 57);

    sharp_flush();
}

void display_show_calib(const char *status, int progress, int total) {
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

    sharp_flush();
}

void display_clear(void) {
    fb_clear();
    sharp_flush();
}
