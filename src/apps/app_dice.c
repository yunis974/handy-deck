#include <stdint.h>
#include <stdio.h>
#include "esp_timer.h"
#include "esp_random.h"
#include "app.h"
#include "display.h"
#include "ui.h"

static uint8_t number = 0;
static uint8_t max = 4;
static uint8_t display_num = 1;
static uint8_t lastNumber = 0;

static int64_t roll_start = 0;
static bool running = false;



static void start()
{

}

static void on_input(input_event_t ev)
{
    if (ev == INPUT_CLICK)
    {
        if (!running){
            running = true;
            lastNumber = number;
            roll_start = esp_timer_get_time();
            int64_t seed = esp_random();
            number = seed % (max + 1);
            if (number == 0) number = 1;
        }
    }
}

static void draw(void)
{
    char buf[8];
    if(running)
    {
        
        uint64_t now = esp_timer_get_time();
        if (now - roll_start > 2000000)
        {
            running = false;
            
            sprintf(buf,"%d",number);
        }
        else
        {
            display_num++;
            if(display_num > max)
            {
                display_num = 1;
            }
            sprintf(buf,"%d",display_num);
        }
    }
    else
    {
        sprintf(buf,"%d",number);
    }
   
    // carré centré dans la zone bleue
    int size = 32;
    int x = ((DISPLAY_WIDTH - size) / 2) - 35;
    int y = UI_APP_Y + (UI_APP_H - size) / 2;

    // 1) les chiffres en grand (scale 2 : 12 px par caractère dont 2 px d'espace à droite, 14 px de haut)
    int tw = display_text_width(buf) * 2 - 2;
    int tx = x + (size - tw) / 2;
    int ty = y + (size - 14) / 2;
    display_draw_text_scaled(tx, ty, buf, 2);

    // 2) on inverse tout le carré avec les coins coupés : fond allumé, chiffres éteints
    ui_invert_frame(x, y, size, size);

    char last_num_buf[20];
    sprintf(last_num_buf,"LAST NUMBER: %d", lastNumber);
    display_draw_text_scaled(
        ((DISPLAY_WIDTH - display_text_width(last_num_buf)) / 2) + 30,
        (UI_APP_Y + (UI_APP_H - size)/ 2) - 10,
        last_num_buf,
        1
    );



}



const app_t app_dice =  {
    .name = "DICE",
    .start = start,
    .on_input = on_input,
    .draw = draw,
    .stop = NULL,


};