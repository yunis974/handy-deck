# HANDYDECK — Handbook

Guide pratique pour utiliser, modifier et étendre le firmware du HANDYDECK.

---

## Sommaire

1. [Le matériel](#1-le-matériel)
2. [Compiler, flasher, déboguer](#2-compiler-flasher-déboguer)
3. [Utiliser le HANDYDECK](#3-utiliser-le-handydeck)
4. [Comment le firmware tourne](#4-comment-le-firmware-tourne)
5. [Les fichiers du projet](#5-les-fichiers-du-projet)
6. [Créer une app (pas à pas)](#6-créer-une-app-pas-à-pas)
7. [Référence : les entrées (`input.h`)](#7-référence--les-entrées-inputh)
8. [Référence : l'écran (`display.h`)](#8-référence--lécran-displayh)
9. [Référence : le style (`ui.h`)](#9-référence--le-style-uih)
10. [Recettes](#10-recettes)
11. [Personnaliser le système](#11-personnaliser-le-système)
12. [Pièges à connaître](#12-pièges-à-connaître)

---

## 1. Le matériel

| Élément | Détail | GPIO |
|---|---|---|
| Carte | ESP32-C3 Super Mini (4 Mo de flash, USB Serial/JTAG) | — |
| Écran | OLED SSD1306 128x64 en I2C, adresse `0x3C` | SDA = 8, SCL = 9 |
| Encodeur | Rotatif, signaux A et B | A = 0, B = 1 |
| Bouton | Bouton principal (clic / appui long) | 21 |
| Bouton encodeur | Pas encore câblé | à définir |

Tous les boutons et l'encodeur utilisent le **pull-up interne** : le contact relie la pin à la masse (GND), donc *appuyé = niveau 0*.

### L'écran bicolore

```
y = 0  ┌────────────────────────────────┐
       │  ZONE JAUNE : barre de statut  │  16 lignes (0 → 15)
y = 16 ├────────────────────────────────┤
       │                                │
       │  ZONE BLEUE : les apps         │  48 lignes (16 → 63)
       │                                │
y = 63 └────────────────────────────────┘
       x = 0                      x = 127
```

La couleur est **physique** (elle dépend de la ligne de l'écran). Le code ne fait qu'allumer ou éteindre des pixels.

---

## 2. Compiler, flasher, déboguer

Depuis le dossier du projet :

```sh
pio run                 # compiler
pio run -t upload       # compiler + flasher
pio device monitor      # voir les logs (Ctrl+C pour quitter)
pio run -t upload && pio device monitor   # les deux d'un coup
```

**Les warnings sont des erreurs** : ESP-IDF compile avec `-Werror`. Un buffer `snprintf` trop petit, une variable non utilisée, etc. bloquent la compilation. Lis le message, il indique le fichier et la ligne.

### Logs

Dans le code :

```c
#include "esp_log.h"
static const char *TAG = "mon_app";

ESP_LOGI(TAG, "valeur = %d", valeur);   // info
ESP_LOGW(TAG, "attention");             // warning (jaune)
ESP_LOGE(TAG, "erreur !");              // erreur (rouge)
```

`printf(...)` marche aussi.

Dans `main.c`, `#define LOG_INPUTS 1` affiche chaque événement (rotation, clic...) dans le moniteur. Mets `0` pour couper.

### Si le flash ne marche pas

- Débranche, maintiens **BOOT**, rebranche, relâche. Puis relance `pio run -t upload`.
- `Permission denied` sur le port : vérifie que ton utilisateur est dans le groupe `uucp` et que les règles udev PlatformIO sont installées.

---

## 3. Utiliser le HANDYDECK

| Action | Dans le menu | Dans une app |
|---|---|---|
| Tourner l'encodeur | Déplacer la sélection | Dépend de l'app |
| Clic court (GPIO21) | Ouvrir l'app sélectionnée | Dépend de l'app |
| Appui long (≥ 0,6 s) | Rien | **Retour au menu** |

L'appui long est **réservé au système** : une app ne le reçoit jamais. C'est ce qui garantit qu'on peut toujours sortir d'une app.

### Les apps fournies

| App | Utilité |
|---|---|
| **INPUT TEST** | Compteur piloté par l'encodeur, nombre de clics, dernier événement. Pour vérifier le câblage. Sert aussi de modèle. |
| **CHRONO** | Chronomètre. Clic = start / pause. À l'arrêt, tourner = remise à zéro. |
| **SYSINFO** | Temps d'allumage, mémoire libre (actuelle et minimum), puce, version d'ESP-IDF. |

---

## 4. Comment le firmware tourne

Tout se passe dans **une seule boucle**, dans `app_main()` (`src/main.c`) :

```
démarrage :  display_init()  →  input_init()  →  ui_boot_animation()

boucle (≈ 20 fois par seconde) :
  ┌─► 1. input_update()          lit encodeur + boutons, remplit la file d'événements
  │   2. pour chaque événement :
  │        - dans le menu  → menu_on_input()
  │        - appui long    → close_app()   (retour menu)
  │        - sinon         → app->on_input(ev)
  │   3. display_clear()              efface le buffer
  │   4. ui_draw_statusbar(titre)     dessine la zone jaune
  │   5. menu_draw() ou app->draw()   dessine la zone bleue
  │   6. display_flush()              envoie l'image à l'écran (~25 ms en I2C)
  └── 7. vTaskDelay(FRAME_MS)         pause de 20 ms
```

Trois idées clés :

- **On redessine tout à chaque image.** Une app ne « modifie » pas l'écran, elle le redessine entièrement à partir de ses variables. Pour changer ce qui s'affiche, on change une variable, et la prochaine image en tient compte.
- **Le dessin se fait en RAM.** Les fonctions `display_*` écrivent dans un buffer de 1 Ko. Rien n'apparaît tant que `display_flush()` n'est pas appelé, et c'est `main.c` qui s'en charge.
- **Rien ne doit bloquer.** Si `draw()` ou `on_input()` attend (boucle infinie, gros `vTaskDelay`), tout le HANDYDECK se fige, boutons compris.

---

## 5. Les fichiers du projet

```
include/                  les en-têtes (.h)
├── app.h             définition d'une app (app_t)
├── input.h           encodeur + boutons → événements
├── display.h         pilote écran + fonctions de dessin
├── font5x7.h         police 5x7 (ASCII 32 → 126)
├── ui.h              style : barre de statut, cadre, animation de boot
├── menu.h            le lanceur d'apps
└── apps/
    └── apps.h        déclare toutes les apps
src/                      le code (.c)
├── main.c            boucle principale + liste des apps (apps[])
├── input.c
├── display.c
├── ui.c
├── menu.c
└── apps/
    ├── app_input_test.c  ← le modèle à copier
    ├── app_chrono.c
    └── app_sysinfo.c
```

Tous les `.c` dans `src/` (sous-dossiers compris) sont compilés automatiquement. Pas besoin de toucher au `CMakeLists.txt` quand tu ajoutes un fichier. Les `.h` vont dans `include/`.

---

## 6. Créer une app (pas à pas)

Exemple : une app **DICE** qui lance un dé.

### Étape 1 : créer le fichier `src/apps/app_dice.c`

```c
// Lance un dé : clic = nouveau lancer, tourner = changer le nombre de faces.

#include <stdio.h>
#include "app.h"
#include "display.h"
#include "esp_random.h"
#include "ui.h"

// Les variables de l'app. "static" = visibles seulement dans ce fichier.
static int faces;
static int result;

static void start(void)
{
    // appelé à chaque ouverture : on remet tout à zéro ici
    faces = 6;
    result = 0;
}

static void on_input(input_event_t ev)
{
    if (ev == INPUT_CLICK) {
        result = 1 + (int)(esp_random() % faces);
    }
    if (ev == INPUT_RIGHT) {
        faces++;
    }
    if (ev == INPUT_LEFT) {
        if (faces > 2) {
            faces--;
        }
    }
}

static void draw(void)
{
    char text[16];

    snprintf(text, sizeof(text), "D%d", faces);
    display_draw_text(2, UI_APP_Y + 2, text);

    if (result > 0) {
        snprintf(text, sizeof(text), "%d", result);
        display_draw_text_scaled(50, UI_APP_Y + 14, text, 3);
    }

    ui_draw_text_centered(UI_APP_Y + 40, "CLICK = ROLL");
}

const app_t app_dice = {
    .name = "DICE",
    .start = start,
    .on_input = on_input,
    .draw = draw,
    .stop = NULL,   // rien à nettoyer en sortant
};
```

### Étape 2 : la déclarer dans `include/apps/apps.h`

```c
extern const app_t app_dice;
```

### Étape 3 : l'ajouter au menu dans `src/main.c`

```c
static const app_t *apps[] = {
    &app_input_test,
    &app_chrono,
    &app_sysinfo,
    &app_dice,        // ← ici. L'ordre du tableau = l'ordre dans le menu.
};
```

C'est tout. Compile et flashe.

### Le contrat d'une app (`app_t`)

| Champ | Quand c'est appelé | Obligatoire ? |
|---|---|---|
| `name` | Texte du menu et de la barre de statut. **11 caractères max**, en majuscules pour le style. | oui |
| `start()` | Une fois, à l'ouverture depuis le menu. | non (`NULL`) |
| `on_input(ev)` | Une fois par événement : rotation, clic... Jamais pour l'appui long. | non (`NULL`) |
| `draw()` | À chaque image (~20/s). L'écran est déjà effacé et la barre de statut déjà dessinée. | **oui** |
| `stop()` | Une fois, quand on revient au menu. | non (`NULL`) |

**Règles d'or :**
- Dessine seulement entre `y = UI_APP_Y` (16) et `y = 63`. Au-dessus, c'est la barre de statut.
- Remets tes variables à zéro dans `start()`, pas à leur déclaration. Sinon l'app garde l'état de la fois précédente (ça peut aussi être voulu, voir [Recettes](#garder-létat-entre-deux-ouvertures)).
- Pas de `vTaskDelay` ni d'attente dans `draw()` / `on_input()`.
- Tout ce qui est interne à l'app est déclaré `static`. Seule la variable `app_xxx` est publique, ce qui évite les conflits de noms entre apps.

---

## 7. Référence : les entrées (`input.h`)

### Les événements

| Événement | Source | Envoyé quand... |
|---|---|---|
| `INPUT_LEFT` | encodeur | un cran vers la gauche |
| `INPUT_RIGHT` | encodeur | un cran vers la droite |
| `INPUT_CLICK` | bouton GPIO21 | **au relâchement**, si l'appui a duré moins de 0,6 s |
| `INPUT_LONG_PRESS` | bouton GPIO21 | dès que l'appui dépasse 0,6 s (réservé au système) |
| `INPUT_ENC_CLICK` | bouton encodeur | au relâchement (quand il sera câblé) |

Une rotation rapide donne **plusieurs** `INPUT_RIGHT` dans la même image. `on_input()` est alors appelée plusieurs fois de suite, et il n'y a rien à gérer de spécial.

### Les réglages (en haut de `input.h`)

| Define | Rôle |
|---|---|
| `INPUT_PIN_ENC_A`, `INPUT_PIN_ENC_B` | Pins de l'encodeur. **Échange-les si le sens est inversé.** |
| `INPUT_PIN_BUTTON` | Pin du bouton principal |
| `INPUT_PIN_ENC_BUTTON` | Pin du bouton de l'encodeur (`-1` = pas câblé) |
| `INPUT_ENC_STEPS_PER_DETENT` | Pas électriques par cran. 4 pour la plupart des encodeurs. Si un cran avance de 2 → mets 2 × la valeur. S'il faut 2 crans pour avancer de 1 → mets la moitié. |
| `INPUT_LONG_PRESS_MS` | Durée d'un appui long (600 ms) |

### Comment ça marche dedans (pour déboguer)

- **Encodeur :** une interruption se déclenche à chaque changement de A ou de B. Elle compare l'ancien et le nouvel état (A,B) dans `quad_table` et ajoute +1, -1 ou 0 à `encoder_steps`. `input_update()` récupère ces pas et les convertit en crans → événements.
- **Boutons :** ils sont lus à chaque tour de boucle, sans interruption. Comme on lit toutes les ~45 ms et qu'un rebond dure quelques ms, on ne peut jamais compter deux appuis pour un seul.
- **File d'événements :** un buffer circulaire de 16 cases. S'il est plein, les nouveaux événements sont perdus (ça n'arrive que si la boucle est bloquée).

### Ajouter un bouton

1. Dans `input.h` : ajoute un `#define INPUT_PIN_BTN2 x` et un nouvel événement dans l'enum, par exemple `INPUT_BTN2_CLICK`.
2. Dans `input.c` : ajoute une ligne au tableau `buttons[]` :
   ```c
   { .pin = INPUT_PIN_BTN2, .click_event = INPUT_BTN2_CLICK, .long_event = INPUT_NONE },
   ```
3. Dans `input_event_name()` : ajoute le `case` pour avoir un nom lisible dans les logs.

L'initialisation des GPIO se fait automatiquement pour tout le tableau.

---

## 8. Référence : l'écran (`display.h`)

Repère : `(0,0)` en haut à gauche, `x` vers la droite (0 → 127), `y` vers le bas (0 → 63). Les fonctions acceptent des coordonnées hors écran : ces pixels sont simplement ignorés.

| Fonction | Effet |
|---|---|
| `display_clear()` | Éteint tout le buffer (déjà fait par `main.c` avant chaque image) |
| `display_set_pixel(x, y, on)` | Allume (`true`) ou éteint (`false`) un pixel |
| `display_fill_rect(x, y, w, h, on)` | Rectangle plein. Avec `h = 1`, ça fait une ligne horizontale. Avec `w = 1`, une ligne verticale. |
| `display_draw_rect(x, y, w, h)` | Contour de rectangle |
| `display_invert_rect(x, y, w, h)` | Inverse les pixels d'une zone (allumé ↔ éteint) |
| `display_draw_text(x, y, txt)` | Texte en police 5x7. `(x, y)` = coin haut-gauche. 6 px par caractère → 21 caractères par ligne. |
| `display_draw_text_scaled(x, y, txt, s)` | Texte agrandi `s` fois. `s = 2` → 12 px par caractère, 14 px de haut. |
| `display_text_width(txt)` | Largeur en pixels d'un texte normal. Multiplie par `s` pour un texte agrandi. |
| `display_flush()` | Envoie le buffer à l'écran (`main.c` s'en charge) |

### Tailles utiles

| Taille du texte | Caractère | Max par ligne | Lignes dans la zone bleue |
|---|---|---|---|
| normal (×1) | 6 × 8 px | 21 | 6 (avec 8 px d'interligne) |
| ×2 | 12 × 16 px | 10 | 3 |
| ×3 | 18 × 24 px | 7 | 2 |

### Texte en négatif (blanc sur fond allumé)

Dessine d'abord le texte, **puis** inverse la zone :

```c
display_draw_text(10, 30, "ALERT");
display_invert_rect(8, 29, display_text_width("ALERT") + 3, 9);
```

### Afficher des nombres

Utilise `snprintf` dans un buffer **assez grand**. Le compilateur refuse un buffer qui pourrait être trop petit.

```c
char text[24];
snprintf(text, sizeof(text), "TEMP %d C", temp);
display_draw_text(0, UI_APP_Y, text);
```

### Dessiner une petite icône

Une icône, c'est des pixels :

```c
// coeur 7x6
static const char *heart[] = {
    ".XX.XX.",
    "XXXXXXX",
    "XXXXXXX",
    ".XXXXX.",
    "..XXX..",
    "...X...",
};

static void draw_heart(int x, int y)
{
    for (int row = 0; row < 6; row++) {
        for (int col = 0; col < 7; col++) {
            if (heart[row][col] == 'X') {
                display_set_pixel(x + col, y + row, true);
            }
        }
    }
}
```

C'est pas la méthode la plus compacte en mémoire, mais on voit l'icône directement dans le code.

---

## 9. Référence : le style (`ui.h`)

| Élément | Valeur / fonction |
|---|---|
| `UI_STATUSBAR_H` | 16 (hauteur de la zone jaune) |
| `UI_APP_Y` | 16 (premier `y` de la zone des apps) |
| `UI_APP_H` | 48 (hauteur de la zone des apps) |
| `ui_draw_statusbar(titre)` | Barre de statut (appelée par `main.c`) |
| `ui_draw_frame(x, y, w, h)` | Cadre avec coins haut-gauche et bas-droite coupés en biais |
| `ui_draw_text_centered(y, txt)` | Texte centré horizontalement |
| `ui_boot_animation()` | Animation de démarrage (bloquante, appelée une fois) |

### Les codes visuels cyberpunk du projet

Pour que toutes les apps aient le même look :
- **MAJUSCULES** partout, mots courts, style code : `SYS.CORE`, `IO.LINK`.
- **Coins coupés** plutôt que coins carrés (`ui_draw_frame`, barre de sélection du menu).
- **Négatif** (texte éteint sur fond allumé) pour ce qui est actif ou sélectionné.
- **Pointillés** pour les éléments secondaires (rail de la barre de défilement).
- **Préfixes** `>` pour les lignes de log, `[OK]` / `[!!]` pour les états, numéros à 2 chiffres (`01`, `02`).
- **Clignotement** pour ce qui est « vivant » (le `:` de l'horloge, le carré REC du chrono).

### Faire clignoter quelque chose

Pas besoin de timer, on regarde l'heure à chaque image :

```c
#include "esp_timer.h"

// vrai pendant 0,5 s, faux pendant 0,5 s
if ((esp_timer_get_time() / 500000) % 2 == 0) {
    display_fill_rect(120, UI_APP_Y + 2, 4, 4, true);
}
```

`esp_timer_get_time()` renvoie le temps depuis l'allumage en **microsecondes** (`int64_t`).

---

## 10. Recettes

### Une liste avec sélection dans une app

```c
static const char *items[] = { "ALPHA", "BRAVO", "CHARLIE" };
#define ITEM_COUNT 3
static int sel;

static void start(void)
{
    sel = 0;
}

static void on_input(input_event_t ev)
{
    if (ev == INPUT_RIGHT && sel < ITEM_COUNT - 1) {
        sel++;
    }
    if (ev == INPUT_LEFT && sel > 0) {
        sel--;
    }
    if (ev == INPUT_CLICK) {
        // faire quelque chose avec items[sel]
    }
}

static void draw(void)
{
    for (int i = 0; i < ITEM_COUNT; i++) {
        int y = UI_APP_Y + 2 + i * 10;
        display_draw_text(4, y, items[i]);
        if (i == sel) {
            display_invert_rect(0, y - 1, 80, 9);
        }
    }
}
```

S'il y a plus de 4-5 éléments, inspire-toi de la variable `scroll` dans `src/menu.c`.

### Plusieurs écrans dans une app

Utilise une variable « écran courant » et des `if` :

```c
#define SCREEN_MAIN     0
#define SCREEN_SETTINGS 1
static int screen;

static void on_input(input_event_t ev)
{
    if (screen == SCREEN_MAIN) {
        if (ev == INPUT_CLICK) {
            screen = SCREEN_SETTINGS;
        }
    } else if (screen == SCREEN_SETTINGS) {
        if (ev == INPUT_CLICK) {
            screen = SCREEN_MAIN;
        }
    }
}

static void draw(void)
{
    if (screen == SCREEN_MAIN) {
        // ...
    } else if (screen == SCREEN_SETTINGS) {
        // ...
    }
}
```

### Faire quelque chose à intervalle régulier

Dans `draw()`, on compare avec le dernier passage :

```c
static int64_t last_tick_us;

static void start(void)
{
    last_tick_us = esp_timer_get_time();
}

static void draw(void)
{
    int64_t now = esp_timer_get_time();
    if (now - last_tick_us >= 1000000) {   // toutes les secondes
        last_tick_us = now;
        // ... action ...
    }
    // ... dessin ...
}
```

### Garder l'état entre deux ouvertures

Ne remets simplement pas la variable à zéro dans `start()`. Initialise-la à sa déclaration :

```c
static int high_score = 0;   // gardé tant que la carte reste allumée
```

Pour garder une valeur **après extinction**, il faudra la mémoire NVS d'ESP-IDF (`nvs_flash`). Ce n'est pas encore en place.

### Une animation

Calcule la position à partir du temps, pas d'un compteur d'images. Comme ça, la vitesse ne change pas si la boucle ralentit :

```c
static void draw(void)
{
    int64_t ms = esp_timer_get_time() / 1000;
    int x = (ms / 20) % DISPLAY_WIDTH;   // avance d'1 px toutes les 20 ms
    display_fill_rect(x, UI_APP_Y + 20, 4, 4, true);
}
```

### Utiliser une GPIO dans une app

```c
#include "driver/gpio.h"

#define MY_PIN 3

static void start(void)
{
    gpio_reset_pin(MY_PIN);
    gpio_set_direction(MY_PIN, GPIO_MODE_OUTPUT);
}

static void stop(void)
{
    gpio_set_level(MY_PIN, 0);   // tout éteindre en sortant
}
```

Pins libres sur le Super Mini : **2, 3, 4, 5, 6, 7, 10, 20**. Évite 8 et 9 (écran), 0 et 1 (encodeur), 21 (bouton), 18 et 19 (USB).

---

## 11. Personnaliser le système

| Je veux... | Où | Quoi |
|---|---|---|
| Changer l'ordre des apps | `src/main.c` | L'ordre du tableau `apps[]` |
| Changer la vitesse de la boucle | `src/main.c` | `FRAME_MS` (moins = plus fluide, mais plus de consommation) |
| Couper les logs d'entrées | `src/main.c` | `LOG_INPUTS 0` |
| Changer la durée de l'appui long | `include/input.h` | `INPUT_LONG_PRESS_MS` |
| Changer l'animation de démarrage | `src/ui.c` | `ui_boot_animation()` : titre, tableau `lines[]`, délais. Pour la supprimer, retire l'appel dans `main.c`. |
| Changer la barre de statut | `src/ui.c` | `ui_draw_statusbar()` |
| Changer l'aspect du menu | `src/menu.c` | `ITEM_H`, `VISIBLE_ITEMS`, et le bloc `if (index == selected)` |
| Retourner l'écran | `src/display.c` | `esp_lcd_panel_mirror(panel, true, true)` dans `display_init()` |
| Changer les pins de l'écran | `include/display.h` | `DISPLAY_PIN_SDA`, `DISPLAY_PIN_SCL` |

### Ajouter une icône dans la barre de statut

Dans `ui_draw_statusbar()`, il y a un commentaire `place libre ... pour de futures icônes`. L'espace libre est entre la fin du titre (`title_w + 15`) et l'horloge (`x = 98`). Par exemple :

```c
// petit indicateur à gauche de l'horloge
display_draw_rect(88, 4, 6, 6);
```

Pour qu'une app ou un module puisse l'allumer ou l'éteindre, ajoute une variable globale dans `ui.c` et une fonction `ui_set_xxx(bool)` dans `ui.h`.

### Changer le comportement de l'appui long

C'est dans `handle_event()` de `src/main.c` :

```c
if (ev == INPUT_LONG_PRESS) {
    close_app();
    return;
}
```

Si une app doit un jour recevoir l'appui long elle-même, c'est ici qu'il faut ajouter une exception.

---

## 12. Pièges à connaître

- **GPIO8 = LED intégrée = SDA de l'écran.** On ne peut pas utiliser la LED. Elle peut clignoter faiblement pendant les envois à l'écran, c'est normal.
- **GPIO9 = bouton BOOT = SCL de l'écran.** Ne pas appuyer sur BOOT pendant le fonctionnement normal.
- **GPIO 2, 8, 9 sont des « strapping pins ».** Leur niveau au démarrage décide du mode de boot. Évite d'y brancher un bouton qui pourrait être appuyé à l'allumage.
- **L'USB du C3 est limité.** Il peut faire port série et débogage JTAG, mais pas clavier ni hôte USB. Un téléphone Android avec une app de terminal série peut quand même lui parler (pour envoyer l'heure par exemple). Pas un iPhone.
- **L'horloge de la barre de statut n'est pas une vraie heure.** C'est le temps depuis l'allumage. Il faudra l'envoyer par série ou BLE pour avoir l'heure réelle.
- **`display_flush()` prend ~25 ms.** C'est le transfert I2C de 1 Ko. C'est lui qui limite la fluidité, pas le dessin.
- **Variables globales des apps :** toujours `static`. Deux apps qui ont toutes les deux une variable `counter` non `static` = erreur de compilation « multiple definition ».
- **`int64_t` dans `printf`/`snprintf` :** convertis en `int` (`(int)valeur`) ou utilise `%lld` avec `(long long)valeur`, sinon le compilateur refuse.
- **Surveille la mémoire** avec l'app SYSINFO quand tu ajoutes des apps. `MIN` est la plus petite mémoire libre vue depuis le démarrage : si elle descend sous ~20 Ko, ça devient risqué.
