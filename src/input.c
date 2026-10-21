#include "input.h"

#include <stdint.h>
#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"

// ============================================================
//  File d'événements
// ============================================================
// Petit buffer circulaire. Il est rempli et vidé uniquement depuis la boucle principale
// (jamais depuis une interruption), donc pas besoin de protection.

#define QUEUE_SIZE 16

static input_event_t queue[QUEUE_SIZE];
static int queue_head = 0;   // index du prochain événement à lire
static int queue_count = 0;  // nombre d'événements en attente

static void queue_push(input_event_t ev)
{
    if (queue_count == QUEUE_SIZE) {
        return;  // file pleine : on perd l'événement
    }
    int index = (queue_head + queue_count) % QUEUE_SIZE;
    queue[index] = ev;
    queue_count++;
}

input_event_t input_get_event(void)
{
    if (queue_count == 0) {
        return INPUT_NONE;
    }
    input_event_t ev = queue[queue_head];
    queue_head = (queue_head + 1) % QUEUE_SIZE;
    queue_count--;
    return ev;
}

// ============================================================
//  Encodeur rotatif
// ============================================================
// L'encodeur sort 2 signaux A et B décalés d'un quart de période. On note l'état (A << 1) | B.
// En tournant, l'état suit toujours le même cycle :
//   sens +1 : 11 -> 01 -> 00 -> 10 -> 11
//   sens -1 : 11 -> 10 -> 00 -> 01 -> 11
// À chaque changement de A ou B, une interruption regarde (ancien état, nouvel état)
// dans ce tableau pour savoir si on a avancé (+1), reculé (-1) ou rien (0 = rebond ou saut invalide).
// Index du tableau = (ancien état << 2) | nouvel état.

static const DRAM_ATTR int8_t quad_table[16] = {
//  nouveau:  00  01  10  11
    /* 00 */   0, -1, +1,  0,
    /* 01 */  +1,  0,  0, -1,
    /* 10 */  -1,  0,  0, +1,
    /* 11 */   0, +1, -1,  0,
};

static volatile int encoder_steps = 0;  // pas accumulés par l'interruption, pas encore lus
static uint8_t encoder_state = 0;       // dernier état (A << 1) | B vu par l'interruption
static portMUX_TYPE encoder_lock = portMUX_INITIALIZER_UNLOCKED;

static void IRAM_ATTR encoder_isr(void *arg)
{
    int a = gpio_get_level(INPUT_PIN_ENC_A);
    int b = gpio_get_level(INPUT_PIN_ENC_B);
    uint8_t new_state = (a << 1) | b;
    uint8_t index = (encoder_state << 2) | new_state;

    portENTER_CRITICAL_ISR(&encoder_lock);
    encoder_steps += quad_table[index];
    portEXIT_CRITICAL_ISR(&encoder_lock);

    encoder_state = new_state;
}

static void update_encoder(void)
{
    // pas qui n'ont pas encore fait un cran complet (gardés d'un appel à l'autre)
    static int pending = 0;

    // On récupère les pas et on remet le compteur à 0. La section critique empêche
    // l'interruption de modifier encoder_steps entre la lecture et la remise à 0.
    portENTER_CRITICAL(&encoder_lock);
    int steps = encoder_steps;
    encoder_steps = 0;
    portEXIT_CRITICAL(&encoder_lock);

    pending += steps;
    while (pending >= INPUT_ENC_STEPS_PER_DETENT) {
        queue_push(INPUT_RIGHT);
        pending -= INPUT_ENC_STEPS_PER_DETENT;
    }
    while (pending <= -INPUT_ENC_STEPS_PER_DETENT) {
        queue_push(INPUT_LEFT);
        pending += INPUT_ENC_STEPS_PER_DETENT;
    }
}

// ============================================================
//  Boutons
// ============================================================
// Les boutons sont lus à chaque tour de boucle (toutes les ~20-50 ms), pas par interruption.
// Les rebonds d'un bouton durent quelques ms : en lisant moins souvent que ça, on ne peut
// pas voir deux appuis pour un seul. C'est un anti-rebond "gratuit".

typedef struct {
    int pin;
    input_event_t click_event;  // envoyé au relâchement si l'appui était court
    input_event_t long_event;   // envoyé quand l'appui dépasse INPUT_LONG_PRESS_MS (INPUT_NONE = désactivé)
    bool pressed;
    int64_t press_start_ms;
    bool long_sent;
} button_t;

static button_t buttons[] = {
    { .pin = INPUT_PIN_BUTTON, .click_event = INPUT_CLICK, .long_event = INPUT_LONG_PRESS },
#if INPUT_PIN_ENC_BUTTON >= 0
    { .pin = INPUT_PIN_ENC_BUTTON, .click_event = INPUT_ENC_CLICK, .long_event = INPUT_NONE },
#endif
};

#define BUTTON_COUNT (sizeof(buttons) / sizeof(buttons[0]))

static void update_button(button_t *btn, int64_t now_ms)
{
    bool down = (gpio_get_level(btn->pin) == 0);  // pull-up : appuyé = 0

    if (down && !btn->pressed) {
        // on vient d'appuyer
        btn->pressed = true;
        btn->press_start_ms = now_ms;
        btn->long_sent = false;
    } else if (down && btn->pressed) {
        // toujours appuyé : est-ce que ça devient un appui long ?
        if (btn->long_event != INPUT_NONE && !btn->long_sent) {
            if (now_ms - btn->press_start_ms >= INPUT_LONG_PRESS_MS) {
                queue_push(btn->long_event);
                btn->long_sent = true;
            }
        }
    } else if (!down && btn->pressed) {
        // on vient de relâcher : clic seulement si l'appui long n'a pas déjà été envoyé
        btn->pressed = false;
        if (!btn->long_sent) {
            queue_push(btn->click_event);
        }
    }
}

// ============================================================

void input_init(void)
{
    gpio_config_t enc_cfg = {
        .pin_bit_mask = (1ULL << INPUT_PIN_ENC_A) | (1ULL << INPUT_PIN_ENC_B),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_ANYEDGE,
    };
    ESP_ERROR_CHECK(gpio_config(&enc_cfg));

    for (int i = 0; i < BUTTON_COUNT; i++) {
        gpio_config_t btn_cfg = {
            .pin_bit_mask = 1ULL << buttons[i].pin,
            .mode         = GPIO_MODE_INPUT,
            .pull_up_en   = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type    = GPIO_INTR_DISABLE,
        };
        ESP_ERROR_CHECK(gpio_config(&btn_cfg));
    }

    // état de départ de l'encodeur, avant d'activer les interruptions
    encoder_state = (gpio_get_level(INPUT_PIN_ENC_A) << 1) | gpio_get_level(INPUT_PIN_ENC_B);

    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(INPUT_PIN_ENC_A, encoder_isr, NULL));
    ESP_ERROR_CHECK(gpio_isr_handler_add(INPUT_PIN_ENC_B, encoder_isr, NULL));
}

void input_update(void)
{
    int64_t now_ms = esp_timer_get_time() / 1000;

    update_encoder();
    for (int i = 0; i < BUTTON_COUNT; i++) {
        update_button(&buttons[i], now_ms);
    }
}

const char *input_event_name(input_event_t ev)
{
    switch (ev) {
    case INPUT_NONE:       return "NONE";
    case INPUT_LEFT:       return "LEFT";
    case INPUT_RIGHT:      return "RIGHT";
    case INPUT_CLICK:      return "CLICK";
    case INPUT_LONG_PRESS: return "LONG";
    case INPUT_ENC_CLICK:  return "ENC_CLICK";
    }
    return "?";
}
