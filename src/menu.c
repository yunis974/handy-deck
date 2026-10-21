#include "menu.h"

#include <stdio.h>
#include "display.h"
#include "ui.h"

#define ITEM_H        12  // hauteur d'une ligne du menu
#define VISIBLE_ITEMS 4   // 4 x 12 = 48 px = toute la zone bleue
#define LIST_W        121 // largeur de la liste (le reste = barre de défilement)

static int selected = 0;  // app sélectionnée
static int scroll = 0;    // index de la première app visible

int menu_on_input(input_event_t ev, int app_count)
{
    if (ev == INPUT_RIGHT) {
        selected++;
        if (selected >= app_count) {
            selected = 0;  // on reboucle au début
        }
    }
    if (ev == INPUT_LEFT) {
        selected--;
        if (selected < 0) {
            selected = app_count - 1;  // on reboucle à la fin
        }
    }

    // fait défiler la liste pour que la sélection reste visible
    if (selected < scroll) {
        scroll = selected;
    }
    if (selected >= scroll + VISIBLE_ITEMS) {
        scroll = selected - VISIBLE_ITEMS + 1;
    }

    if (ev == INPUT_CLICK || ev == INPUT_ENC_CLICK) {
        return selected;
    }
    return -1;
}

void menu_draw(const app_t *apps[], int app_count)
{
    char line[24];

    for (int i = 0; i < VISIBLE_ITEMS; i++) {
        int index = scroll + i;
        if (index >= app_count) {
            break;
        }
        int y = UI_APP_Y + i * ITEM_H;

        snprintf(line, sizeof(line), "%02d %s", index + 1, apps[index]->name);
        display_draw_text(4, y + 2, line);

        if (index == selected) {
            display_draw_text(LIST_W - 10, y + 2, "<");
            display_invert_rect(0, y, LIST_W, ITEM_H - 1);
            // coins coupés de la barre de sélection
            display_set_pixel(LIST_W - 1, y, false);
            display_set_pixel(LIST_W - 2, y, false);
            display_set_pixel(LIST_W - 1, y + 1, false);
            display_set_pixel(0, y + ITEM_H - 2, false);
            display_set_pixel(1, y + ITEM_H - 2, false);
            display_set_pixel(0, y + ITEM_H - 3, false);
        }
    }

    // barre de défilement : rail en pointillés + curseur plein
    int bar_x = DISPLAY_WIDTH - 3;
    for (int y = UI_APP_Y; y < UI_APP_Y + UI_APP_H; y += 2) {
        display_set_pixel(bar_x + 1, y, true);
    }
    if (app_count > 0) {
        int thumb_h = UI_APP_H / app_count;
        if (thumb_h < 4) {
            thumb_h = 4;
        }
        int thumb_y = UI_APP_Y + (UI_APP_H - thumb_h) * selected / (app_count > 1 ? app_count - 1 : 1);
        display_fill_rect(bar_x, thumb_y, 3, thumb_h, true);
    }
}
