#pragma once

// Écran bicolore : les 16 premières lignes sont jaunes (barre de statut), le reste est bleu (apps).
#define UI_STATUSBAR_H 16
#define UI_APP_Y       16
#define UI_APP_H       48

// Barre de statut en haut (zone jaune) : nom de l'app à gauche, temps écoulé depuis l'allumage à droite.
void ui_draw_statusbar(const char *title);

// Cadre style cyberpunk : rectangle avec les coins haut-gauche et bas-droite coupés en biais.
void ui_draw_frame(int x, int y, int w, int h);

// Inverse les pixels dans la forme du cadre (mêmes coins coupés) : cadre plein, texte dedans en négatif.
// Dessiner le texte d'abord, puis appeler cette fonction.
void ui_invert_frame(int x, int y, int w, int h);

// Texte centré horizontalement sur l'écran.
void ui_draw_text_centered(int y, const char *text);

// Animation de démarrage (bloquante, ~1,5 s).
void ui_boot_animation(void);
