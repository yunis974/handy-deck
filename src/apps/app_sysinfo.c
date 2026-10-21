// Infos système : utile pour surveiller la mémoire quand on ajoute des apps.

#include <stdint.h>
#include <stdio.h>
#include "app.h"
#include "display.h"
#include "esp_chip_info.h"
#include "esp_idf_version.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "sdkconfig.h"
#include "ui.h"

static void draw(void)
{
    char line[32];
    int y = UI_APP_Y + 2;

    int64_t seconds = esp_timer_get_time() / 1000000;
    snprintf(line, sizeof(line), "UPTIME %02d:%02d:%02d",
             (int)(seconds / 3600), (int)((seconds / 60) % 60), (int)(seconds % 60));
    display_draw_text(0, y, line);
    y += 9;

    snprintf(line, sizeof(line), "HEAP   %u B", (unsigned)esp_get_free_heap_size());
    display_draw_text(0, y, line);
    y += 9;

    snprintf(line, sizeof(line), "MIN    %u B", (unsigned)esp_get_minimum_free_heap_size());
    display_draw_text(0, y, line);
    y += 9;

    esp_chip_info_t chip;
    esp_chip_info(&chip);
    snprintf(line, sizeof(line), "CPU    C3 r%d.%d %dMHz",
             chip.revision / 100, chip.revision % 100, CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ);
    display_draw_text(0, y, line);
    y += 9;

    snprintf(line, sizeof(line), "IDF    %s", esp_get_idf_version());
    display_draw_text(0, y, line);
}

const app_t app_sysinfo = {
    .name = "SYSINFO",
    .start = NULL,
    .on_input = NULL,
    .draw = draw,
    .stop = NULL,
};
