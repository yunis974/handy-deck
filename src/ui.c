#include "ui.h"

#include <stdint.h>
#include <stdio.h>
#include "display.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void ui_draw_statusbar(const char *title)
{
    // --- étiquette du titre : texte "en négatif" dans un bloc plein qui finit en biais ---
    int title_w = display_text_width(title);
    display_draw_text(3, 3, title);
    display_invert_rect(0, 1, title_w + 5, 11);  // le fond devient allumé, le texte éteint
    // bout en biais : chaque ligne un peu plus courte que celle du dessus
    for (int row = 0; row < 11; row++) {
        display_fill_rect(title_w + 5, 1 + row, 10 - row, 1, true);
    }

    // --- horloge : temps depuis l'allumage, le ":" clignote chaque seconde ---
    int64_t seconds = esp_timer_get_time() / 1000000;
    int hours = (int)(seconds / 3600);
    int minutes = (int)((seconds / 60) % 60);
    char clock[8];
    if (seconds % 2 == 0) {
        snprintf(clock, sizeof(clock), "%02d:%02d", hours, minutes);
    } else {
        snprintf(clock, sizeof(clock), "%02d %02d", hours, minutes);
    }
    display_draw_text(DISPLAY_WIDTH - display_text_width(clock), 3, clock);

    // (place libre entre le titre et l'horloge pour de futures icônes : BT, etc.)

    // --- ligne de séparation, plus épaisse sous le titre ---
    display_fill_rect(0, 14, DISPLAY_WIDTH, 1, true);
    display_fill_rect(0, 15, title_w + 15, 1, true);
}

void ui_draw_frame(int x, int y, int w, int h)
{
    int c = 3;  // taille du coin coupé

    display_fill_rect(x + c, y, w - c, 1, true);          // haut (commence après le coin)
    display_fill_rect(x, y + h - 1, w - c, 1, true);      // bas (s'arrête avant le coin)
    display_fill_rect(x, y + c, 1, h - c, true);          // gauche
    display_fill_rect(x + w - 1, y, 1, h - c, true);      // droite

    for (int i = 0; i <= c; i++) {
        display_set_pixel(x + i, y + c - i, true);                  // biais haut-gauche
        display_set_pixel(x + w - 1 - i, y + h - 1 - c + i, true);  // biais bas-droite
    }
}

void ui_invert_frame(int x, int y, int w, int h)
{
    int c = 3;  // même coin coupé que ui_draw_frame

    // ligne par ligne, en raccourcissant les lignes du haut à gauche et du bas à droite
    for (int row = 0; row < h; row++) {
        int left = (row < c) ? c - row : 0;
        int right = (row > h - 1 - c) ? row - (h - 1 - c) : 0;
        display_invert_rect(x + left, y + row, w - left - right, 1);
    }
}

void ui_draw_text_centered(int y, const char *text)
{
    int x = (DISPLAY_WIDTH - display_text_width(text)) / 2;
    display_draw_text(x, y, text);
}

void ui_boot_animation(void)
{
    const char *title = "HANDYDECK";
    int title_x = (DISPLAY_WIDTH - display_text_width(title) * 2) / 2;

    // 1) le titre "glitch" : il tremble et des bandes de l'écran s'inversent au hasard
    for (int frame = 0; frame < 16; frame++) {
        display_clear();
        if (frame < 10) {
            int jitter = (int)(esp_random() % 7) - 3;
            display_draw_text_scaled(title_x + jitter, 24, title, 2);
            int band_y = 18 + (int)(esp_random() % 30);
            int band_h = 1 + (int)(esp_random() % 4);
            display_invert_rect(0, band_y, DISPLAY_WIDTH, band_h);
        } else {
            display_draw_text_scaled(title_x, 24, title, 2);
        }
        display_flush();
        vTaskDelay(pdMS_TO_TICKS(40));
    }

    // 2) le titre monte dans la zone jaune, puis des lignes de "boot" s'affichent une par une
    const char *lines[] = { "> SYS.CORE", "> IO.LINK", "> NEURAL.UI" };
    int line_count = 3;
    for (int step = 0; step <= line_count; step++) {
        display_clear();
        display_draw_text_scaled(title_x, 1, title, 2);

        for (int i = 0; i < step; i++) {
            display_draw_text(0, 20 + i * 10, lines[i]);
            display_draw_text(DISPLAY_WIDTH - display_text_width("[OK]"), 20 + i * 10, "[OK]");
        }

        // barre de progression
        display_draw_rect(0, 58, DISPLAY_WIDTH, 6);
        display_fill_rect(2, 60, (DISPLAY_WIDTH - 4) * step / line_count, 2, true);

        display_flush();
        vTaskDelay(pdMS_TO_TICKS(250));
    }
    vTaskDelay(pdMS_TO_TICKS(300));
}
