#pragma once

#include "app.h"

// Traite un événement dans le menu. Renvoie l'index de l'app à ouvrir, ou -1.
int menu_on_input(input_event_t ev, int app_count);

void menu_draw(const app_t *apps[], int app_count);
