#pragma once

#include "input.h"

// Une "app" = un fichier src/apps/app_xxx.c qui définit une variable app_t.
//
// Pour ajouter une app :
//   1. copier src/apps/app_input_test.c en src/apps/app_truc.c et renommer la variable en app_truc
//   2. ajouter  extern const app_t app_truc;  dans src/apps/apps.h
//   3. ajouter  &app_truc  dans le tableau apps[] de main.c
//
// Cycle de vie :
//   - start()    une fois à l'ouverture depuis le menu (remettre ses variables à zéro ici)
//   - on_input() pour chaque événement (rotation, clic...). L'appui long n'arrive jamais :
//                le système le garde pour revenir au menu.
//   - draw()     à chaque image (~20 fois/s). L'écran est déjà effacé et la barre de statut dessinée :
//                dessiner seulement dans la zone UI_APP_Y .. UI_APP_Y + UI_APP_H (voir ui.h).
//                Les apps qui ont besoin du temps (chrono...) peuvent aussi faire leurs calculs ici.
//   - stop()     une fois quand on revient au menu
// start, on_input et stop peuvent être NULL si l'app n'en a pas besoin. draw est obligatoire.

typedef struct {
    const char *name;  // affiché dans le menu et la barre de statut (11 caractères max pour que ça tienne)
    void (*start)(void);
    void (*on_input)(input_event_t ev);
    void (*draw)(void);
    void (*stop)(void);
} app_t;
