#include <Arduino.h>
#include "Button.h"



static void onButtonPressDownCb(void *button_handle, void *usr_data) {
    Serial.println("Button pressed down");
}
static void onButtonPressDownRepeatCb(void *button_handle, void *usr_data)
{
    Serial.println("Button press down repeat");
}
static void onButtonPressUpCb(void *button_handle, void *usr_data) {
    Serial.println("Button press Up");
}
static void onButtonSingleClickCb(void *button_handle, void *usr_data) {
    Serial.println("Button single click");
}
static void onButtonSingleClickRepeatCb(void *button_handle, void *usr_data)
{
    Serial.println("Button single click repeat");
}

static void onButtonDoubleClickEventCb(void *button_handle, void *usr_data)
{
    Serial.println("Button Double click repeat");
}

static void onButtonLongPressStartEventCb(void *button_handle, void *usr_data)
{
    Serial.println("Button LongPressStart click repeat");
}
static void onButtonLongPressHoldEventCb(void *button_handle, void *usr_data)
{
    Serial.println("Button LongPressHold click repeat");
}




void setup()
{
    // put your setup code here, to run once:
    Serial.begin(115200);

    // initializing a button
    Button *btn = new Button(GPIO_NUM_48, false);

    btn->attachPressDownEventCb(&onButtonPressDownCb, NULL);
    btn->attachPressUpEventCb(&onButtonPressUpCb, NULL);
    btn->attachPressDownEventCb(&onButtonPressDownRepeatCb, NULL);
    btn->attachSingleClickEventCb(&onButtonSingleClickCb,NULL);
    btn->attachSingleClickEventCb(&onButtonSingleClickRepeatCb,NULL);
    btn->attachDoubleClickEventCb(&onButtonDoubleClickEventCb,NULL);

    btn->attachLongPressStartEventCb(onButtonLongPressStartEventCb,NULL);
    btn->attachLongPressHoldEventCb(onButtonLongPressHoldEventCb, NULL);
    
    btn->unregisterPressDownEventCb(&onButtonPressDownCb);
    btn->detachSingleClickEvent();

    
}

void loop()
{
    delay(10);
}
