#include "display.h"

#include <stdint.h>
#include <string.h>
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "font5x7.h"

static const char *TAG = "display";

static esp_lcd_panel_handle_t panel;

// Format natif du SSD1306 : chaque octet = 8 pixels verticaux, organisés en "pages" de 8 lignes.
static uint8_t framebuffer[DISPLAY_WIDTH * DISPLAY_HEIGHT / 8];

esp_err_t display_init(void)
{
    i2c_master_bus_handle_t bus;
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = DISPLAY_PIN_SDA,
        .scl_io_num = DISPLAY_PIN_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_config, &bus), TAG, "bus I2C");

    esp_lcd_panel_io_handle_t io;
    esp_lcd_panel_io_i2c_config_t io_config = {
        .dev_addr = DISPLAY_I2C_ADDR,
        .scl_speed_hz = 400 * 1000,
        .control_phase_bytes = 1,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .dc_bit_offset = 6,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(bus, &io_config, &io), TAG, "panel IO");

    esp_lcd_panel_ssd1306_config_t ssd1306_config = {
        .height = DISPLAY_HEIGHT,
    };
    esp_lcd_panel_dev_config_t panel_config = {
        .bits_per_pixel = 1,
        .reset_gpio_num = -1,
        .vendor_config = &ssd1306_config,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_ssd1306(io, &panel_config, &panel), TAG, "panel SSD1306");

    esp_lcd_panel_reset(panel);
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(panel), TAG,
                        "écran introuvable à 0x%02X (câblage ?)", DISPLAY_I2C_ADDR);
    esp_lcd_panel_disp_on_off(panel, true);
    esp_lcd_panel_mirror(panel, true, true); 
    
    display_clear();
    return display_flush();
}

void display_clear(void)
{
    memset(framebuffer, 0, sizeof(framebuffer));
}

void display_set_pixel(int x, int y, bool on)
{
    if (x < 0 || x >= DISPLAY_WIDTH || y < 0 || y >= DISPLAY_HEIGHT) {
        return;
    }
    uint8_t *byte = &framebuffer[x + (y / 8) * DISPLAY_WIDTH];
    uint8_t bit = 1 << (y % 8);
    if (on) {
        *byte |= bit;
    } else {
        *byte &= ~bit;
    }
}

void display_fill_rect(int x, int y, int w, int h, bool on)
{
    for (int j = y; j < y + h; j++) {
        for (int i = x; i < x + w; i++) {
            display_set_pixel(i, j, on);
        }
    }
}

void display_draw_rect(int x, int y, int w, int h)
{
    for (int i = x; i < x + w; i++) {
        display_set_pixel(i, y, true);
        display_set_pixel(i, y + h - 1, true);
    }
    for (int j = y; j < y + h; j++) {
        display_set_pixel(x, j, true);
        display_set_pixel(x + w - 1, j, true);
    }
}

void display_draw_text(int x, int y, const char *text)
{
    for (; *text; text++, x += 6) {
        char c = *text;
        if (c < FONT5X7_FIRST || c > FONT5X7_LAST) {
            c = '?';
        }
        const uint8_t *glyph = font5x7[c - FONT5X7_FIRST];
        for (int col = 0; col < 5; col++) {
            for (int row = 0; row < 7; row++) {
                if (glyph[col] & (1 << row)) {
                    display_set_pixel(x + col, y + row, true);
                }
            }
        }
    }
}

void display_draw_text_scaled(int x, int y, const char *text, int scale)
{
    for (; *text; text++, x += 6 * scale) {
        char c = *text;
        if (c < FONT5X7_FIRST || c > FONT5X7_LAST) {
            c = '?';
        }
        const uint8_t *glyph = font5x7[c - FONT5X7_FIRST];
        for (int col = 0; col < 5; col++) {
            for (int row = 0; row < 7; row++) {
                if (glyph[col] & (1 << row)) {
                    display_fill_rect(x + col * scale, y + row * scale, scale, scale, true);
                }
            }
        }
    }
}

int display_text_width(const char *text)
{
    return strlen(text) * 6;
}

void display_invert_rect(int x, int y, int w, int h)
{
    for (int j = y; j < y + h; j++) {
        for (int i = x; i < x + w; i++) {
            if (i < 0 || i >= DISPLAY_WIDTH || j < 0 || j >= DISPLAY_HEIGHT) {
                continue;
            }
            framebuffer[i + (j / 8) * DISPLAY_WIDTH] ^= 1 << (j % 8);
        }
    }
}

esp_err_t display_flush(void)
{
    return esp_lcd_panel_draw_bitmap(panel, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, framebuffer);
}
