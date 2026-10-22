#include <stdbool.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "display.h"
#include "input.h"
#include "menu.h"
#include "ui.h"
#include "apps/apps.h"

// Pas de LED : le GPIO8 (LED intégrée) sert de SDA pour l'écran.

#define FRAME_MS    20  // pause entre deux images (+ ~25 ms d'envoi I2C de l'écran)
#define LOG_INPUTS  1   // 1 = affiche chaque événement d'entrée dans le moniteur série

static const char *TAG = "main";

// Liste des apps du menu, dans l'ordre d'affichage.
static const app_t *apps[] = {
    &app_input_test,
    &app_chrono,
    &app_sysinfo,
    &app_dice,
};
#define APP_COUNT ((int)(sizeof(apps) / sizeof(apps[0])))

static int current_app = -1;  // index de l'app ouverte, -1 = on est dans le menu

static void open_app(int index)
{
    ESP_LOGI(TAG, "ouverture de %s", apps[index]->name);
    current_app = index;
    if (apps[index]->start != NULL) {
        apps[index]->start();
    }
}

static void close_app(void)
{
    ESP_LOGI(TAG, "fermeture de %s", apps[current_app]->name);
    if (apps[current_app]->stop != NULL) {
        apps[current_app]->stop();
    }
    current_app = -1;
}

static void handle_event(input_event_t ev)
{
    if (LOG_INPUTS) {
        ESP_LOGI(TAG, "input: %s", input_event_name(ev));
    }

    if (current_app < 0) {
        int chosen = menu_on_input(ev, APP_COUNT);
        if (chosen >= 0) {
            open_app(chosen);
        }
        return;
    }

    // dans une app : l'appui long est réservé au système pour revenir au menu
    if (ev == INPUT_LONG_PRESS) {
        close_app();
        return;
    }
    if (apps[current_app]->on_input != NULL) {
        apps[current_app]->on_input(ev);
    }
}

void app_main(void)
{
    ESP_ERROR_CHECK(display_init());
    input_init();
    ESP_LOGI(TAG, "prêt, %d apps", APP_COUNT);

    ui_boot_animation();

    while (true) {
        // 1) entrées
        input_update();
        input_event_t ev = input_get_event();
        while (ev != INPUT_NONE) {
            handle_event(ev);
            ev = input_get_event();
        }

        // 2) dessin
        display_clear();
        if (current_app < 0) {
            ui_draw_statusbar("MENU");
            menu_draw(apps, APP_COUNT);
        } else {
            ui_draw_statusbar(apps[current_app]->name);
            apps[current_app]->draw();
        }
        display_flush();

        vTaskDelay(pdMS_TO_TICKS(FRAME_MS));
    }
}
