#pragma once

#include <stdbool.h>
#include "esp_err.h"

// OLED SSD1306 128x64 en I2C
#define DISPLAY_WIDTH    128
#define DISPLAY_HEIGHT   64
#define DISPLAY_PIN_SDA  8
#define DISPLAY_PIN_SCL  9
#define DISPLAY_I2C_ADDR 0x3C

esp_err_t display_init(void);

// Les fonctions de dessin modifient un buffer en RAM ; display_flush() l'envoie à l'écran.
void display_clear(void);
void display_set_pixel(int x, int y, bool on);
void display_fill_rect(int x, int y, int w, int h, bool on);
void display_draw_rect(int x, int y, int w, int h);
// Police 5x7, 6 px par caractère ; (x, y) = coin haut-gauche du texte.
void display_draw_text(int x, int y, const char *text);
// Même chose en grossi : scale = 2 -> caractères de 10x14, 12 px par caractère.
void display_draw_text_scaled(int x, int y, const char *text, int scale);
// Largeur en pixels d'un texte avec la police normale.
int display_text_width(const char *text);
// Inverse les pixels d'une zone (allumé <-> éteint) : pratique pour surligner.
void display_invert_rect(int x, int y, int w, int h);

esp_err_t display_flush(void);
