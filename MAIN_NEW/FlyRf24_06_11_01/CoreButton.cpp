//--------------------------------------------------------------------------------------------------
#include "CoreButton.h"
#include "Settings.h"
//--------------------------------------------------------------------------------------------------
Button::Button()
{
    // buttonPin = POWER_ON_IN;
    pullUp = true;

    reset();
}
//--------------------------------------------------------------------------------------------------
void Button::begin(uint8_t _pin, bool _pullup, uint16_t _retentionInterval)
{
    buttonPin = _pin;
    pullUp = _pullup;

    if (pullUp)
        pinMode(buttonPin, INPUT_PULLUP); // подт€гиваем к питанию, если попросили
    else
        pinMode(buttonPin, INPUT);

    retentionInterval = _retentionInterval;

    reset();
}


//--------------------------------------------------------------------------------------------------
void Button::reset()
{

    state.atLeastOneStateChangesFound = false;

    state.click_down = false;
    state.click_up = false;
    state.doubleclick = false;
    state.timer = false;
    state.retention = false;

    state.clickCounter = 0;

    state.lastBounce = false;
    state.lastDoubleClick = false;
    state.lastTimer = false;
    state.lastRetention = false;

    lastMillis = millis();

    state.lastButtonState = readButtonState(buttonPin);
}
//--------------------------------------------------------------------------------------------------
void Button::update()
{
#ifdef USE_WATCHDOG_TIMER 
    Settings.reset_IWDG();      // —брос сторожевого таймера 
#endif 
// обновл€ем внутреннее состо€ние
    bool curBounce      = false;
    bool curDoubleClick = false;
    bool curTimer       = false;
    bool curRetention   = false;

    // сбрасываем все флаги
    state.click_down  = false;
    state.click_up    = false;
    state.doubleclick = false;
    state.timer       = false;
    state.retention   = false;

    uint32_t curMillis = millis();
    uint32_t millisDelta = curMillis - lastMillis;
    uint8_t curButtonState = readButtonState(buttonPin); // читаем текущее состо€ние


    // в зависимости от значени€ подт€жки вы€сн€ем состо€ние кнопки - нажата или нет
    bool isButtonPressed = pullUp ? !curButtonState : curButtonState;

    if (curButtonState != state.lastButtonState)    // состо€ние изменилось
    {
        state.atLeastOneStateChangesFound = true;   // было хот€ бы одно изменение в состо€нии (нужно дл€ того, чтобы не было событи€ "clicked", когда кнопку не нажимали ни разу)
        state.lastButtonState = curButtonState;     // сохран€ем его
        lastMillis = curMillis;                     // и врем€ последнего обновлени€
    }

    if (millisDelta > BOUNCE_INTERVAL)             // надо проверить на дребезг
        curBounce = true;

    if (millisDelta > DOUBLECLICK_INTERVAL)        // надо проверить на даблклик
        curDoubleClick = true;

    if (curDoubleClick != state.lastDoubleClick)   // состо€ние даблклика с момента последней проверки изменилось
    {
        state.lastDoubleClick = curDoubleClick;    // сохран€ем текущее
        if (state.lastDoubleClick)                 // провер€ем - если кнопка не нажата, то сбрасываем счЄтчик нажатий 
            state.clickCounter = 0;
    }

    if (curBounce != state.lastBounce)             // состо€ние проверки дребезга изменилось
    {
        state.lastBounce = curBounce;              // сохран€ем текущее

        if (isButtonPressed && curBounce)          // если кнопка была нажата в момент последнего замера и сейчас - значит, дребезг прошЄл и мы можем сохран€ть состо€ние
        {
            state.click_down = true;               // выставл€ем флаг, что кнопка нажата

            ++state.clickCounter;                  // увеличиваем счЄтчик кликов

            if (state.clickCounter == 2)           // если кликнули два раза
            {
                state.clickCounter = 0;            // сбрасываем счЄтчик кликов
                state.doubleclick = true;          // и выставл€ем флаг двойного нажати€

            }
        }
        state.click_up = !isButtonPressed && state.lastBounce && state.atLeastOneStateChangesFound; // кнопка отпущена тогда, когда последний замер и текущий - равны 1 (пин подт€нут к питанию!), и был хот€ бы один клик на кнопке
    }

    if (millisDelta > INACTIVITY_INTERVAL)      // пора провер€ть неактивность
        curTimer = true;

    if (curTimer != state.lastTimer)           // состо€ние неактивности изменилось с момента последнего замера?
    {
        state.lastTimer = curTimer;              // сохран€ем текущее
        state.timer = !isButtonPressed && state.lastTimer && state.atLeastOneStateChangesFound; // кнопка неактивна тогда, когда не была нажата с момента последнего опроса этого состо€ни€

    }

    if (millisDelta > retentionInterval)       // пора провер€ть удержание
        curRetention = true;

    if (curRetention != state.lastRetention)   // если состо€ние изменилось
    {
        state.lastRetention = curRetention;      // сохран€ем его
        state.retention = isButtonPressed && state.lastRetention && state.atLeastOneStateChangesFound; // и считаем кнопку удерживаемой, когда она нажата сейчас и была нажата до этого

    }

}
//--------------------------------------------------------------------------------------------------

