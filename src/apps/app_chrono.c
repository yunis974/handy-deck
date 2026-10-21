// Chronomètre : clic = démarrer / pause, tourner l'encodeur à l'arrêt = remise à zéro.

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "app.h"
#include "display.h"
#include "esp_timer.h"
#include "ui.h"

static bool running;
static int64_t start_us;    // moment du dernier démarrage
static int64_t elapsed_us;  // temps accumulé avant la dernière pause

static void start(void)
{
    running = false;
    elapsed_us = 0;
}

static void on_input(input_event_t ev)
{
    int64_t now = esp_timer_get_time();

    if (ev == INPUT_CLICK || ev == INPUT_ENC_CLICK) {
        if (running) {
            elapsed_us += now - start_us;
            running = false;
        } else {
            start_us = now;
            running = true;
        }
    }

    if ((ev == INPUT_LEFT || ev == INPUT_RIGHT) && !running) {
        elapsed_us = 0;
    }
}

static void draw(void)
{
    int64_t total_us = elapsed_us;
    if (running) {
        total_us += esp_timer_get_time() - start_us;
    }

    int centis = (int)((total_us / 10000) % 100);
    int seconds = (int)((total_us / 1000000) % 60);
    int minutes = (int)((total_us / 60000000) % 100);

    char text[16];
    snprintf(text, sizeof(text), "%02d:%02d.%02d", minutes, seconds, centis);
    display_draw_text_scaled((DISPLAY_WIDTH - display_text_width(text) * 2) / 2, UI_APP_Y + 10, text, 2);

    // petit carré "REC" qui clignote pendant que ça tourne
    if (running && (esp_timer_get_time() / 500000) % 2 == 0) {
        display_fill_rect(DISPLAY_WIDTH - 6, UI_APP_Y + 2, 4, 4, true);
    }

    if (running) {
        ui_draw_text_centered(UI_APP_Y + 38, "CLICK = PAUSE");
    } else {
        ui_draw_text_centered(UI_APP_Y + 38, "CLICK=GO ROT=RESET");
    }
}

const app_t app_chrono = {
    .name = "CHRONO",
    .start = start,
    .on_input = on_input,
    .draw = draw,
    .stop = NULL,
};
