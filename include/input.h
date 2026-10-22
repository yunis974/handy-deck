#pragma once

#include <stdbool.h>

// Câblage : tout est en pull-up interne, donc un contact fermé = niveau 0.
#define INPUT_PIN_ENC_A      0
#define INPUT_PIN_ENC_B      1
#define INPUT_PIN_BUTTON     21
#define INPUT_PIN_ENC_BUTTON -1   // pas encore câblé : mettre le numéro de GPIO ici le jour venu

// Nombre de pas "quadrature" pour un cran ressenti de l'encodeur.
// La plupart des encodeurs (EC11, KY-040) en font 4.
// Si un cran fait avancer de 2 dans le menu -> doubler la valeur ; s'il faut 2 crans pour avancer de 1 -> la diviser par 2.
// Si le sens de rotation est inversé : échanger INPUT_PIN_ENC_A et INPUT_PIN_ENC_B.
#define INPUT_ENC_STEPS_PER_DETENT 4

// Durée (ms) au-delà de laquelle un appui devient un appui long.
#define INPUT_LONG_PRESS_MS 600

typedef enum {
    INPUT_NONE = 0,
    INPUT_LEFT,        // encodeur : un cran à gauche
    INPUT_RIGHT,       // encodeur : un cran à droite
    INPUT_CLICK,       // bouton GPIO21 : appui court (envoyé au relâchement)
    INPUT_LONG_PRESS,  // bouton GPIO21 : appui long (le système l'utilise pour revenir au menu)
    INPUT_ENC_CLICK,   // bouton de l'encodeur (quand il sera câblé)
} input_event_t;

void input_init(void);

// À appeler à chaque tour de la boucle principale : lit les boutons et l'encodeur
// et range les événements dans une petite file d'attente.
void input_update(void);

// Retire le prochain événement de la file. Renvoie INPUT_NONE quand la file est vide.
input_event_t input_get_event(void);

// Nom lisible d'un événement, pratique pour les logs.
const char *input_event_name(input_event_t ev);
