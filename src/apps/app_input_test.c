// App de test des entrées : affiche un compteur piloté par l'encodeur et le dernier événement reçu.
// C'est aussi l'exemple le plus simple à copier pour faire une nouvelle app.

#include <stdio.h>
#include "app.h"
#include "display.h"
#include "ui.h"

static int counter;
static int clicks;
static input_event_t last_event;

static void start(void)
{
    counter = 0;
    clicks = 0;
    last_event = INPUT_NONE;
}

static void on_input(input_event_t ev)
{
    last_event = ev;

    if (ev == INPUT_RIGHT) {
        counter++;
    }
    if (ev == INPUT_LEFT) {
        counter--;
    }
    if (ev == INPUT_CLICK || ev == INPUT_ENC_CLICK) {
        clicks++;
    }
}

static void draw(void)
{
    char line[24];

    ui_draw_frame(0, UI_APP_Y + 1, DISPLAY_WIDTH, 30);

    snprintf(line, sizeof(line), "%d", counter);
    display_draw_text_scaled(8, UI_APP_Y + 9, line, 2);

    snprintf(line, sizeof(line), "CLK:%d", clicks);
    display_draw_text(DISPLAY_WIDTH - 6 - display_text_width(line), UI_APP_Y + 6, line);
    snprintf(line, sizeof(line), "EV:%s", input_event_name(last_event));
    display_draw_text(DISPLAY_WIDTH - 6 - display_text_width(line), UI_APP_Y + 18, line);

    ui_draw_text_centered(UI_APP_Y + 38, "HOLD = EXIT");
}

const app_t app_input_test = {
    .name = "INPUT TEST",
    .start = start,
    .on_input = on_input,
    .draw = draw,
    .stop = NULL,
};
