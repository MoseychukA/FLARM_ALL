#include "ServiceMain.h"
#include <malloc.h>
#include <stdlib.h>
#include <stdio.h>
#include <Arduino.h>
#include "Configuration_ESP32.h"
#include <soc/adc_channel.h>
#include <driver/adc.h>
#include "EEPROMRF.h"

//--------------------------------------------------------------------------------------------------------------------------------
ServiceClass service;
//--------------------------------------------------------------------------------------------------------------------------------
ServiceClass::ServiceClass()
{


}

//--------------------------------------------------------------------------------------------------------------------------------
void ServiceClass::setup()
{
	pinMode(BUTTON_MAIL, INPUT_PULLUP);       // Настроить кнопку "POINT"
}

void ServiceClass::update()
{


}

//--------------------------------------------------------------------------------------------------------------------------------
bool ServiceClass::getNewMessageFlag()
{
	return new_msg_flag;  // получить флаг нового сообщения
}

void ServiceClass::setNewMessageFlag(bool new_flag)
{
	new_msg_flag = new_flag; 	// Сохранить флаг нового сообщения
}


//--------------------------------------------------------------------------------------------------------------------------------
void ServiceClass::saveVer(String ver)
{
	Current_version += ver;
}

String ServiceClass::getVer()
{
	//Serial.print("***version Service - ");
 //   Serial.println(Current_version);
	return Current_version;
}

//--------------------------------------------------------------------------------------------------------------------------------
void ServiceClass::saveMSG(String msg_save)
{
	Current_MSG = msg_save;
}

String ServiceClass::getMSG()
{
	return Current_MSG;
}

void ServiceClass::setMail_on(bool On_Off)
{
	MailOn = On_Off;
}

bool ServiceClass::getMail_on()
{
	return MailOn;
}

void ServiceClass::setMessageRead(bool On_Message)
{
	MessageRead = On_Message;
}

bool ServiceClass::getMessageRead()
{
	return MessageRead;
}

void ServiceClass::setAllow_flashing(bool On_Off)
{
	flashing = On_Off;
}

bool ServiceClass::getAllow_flashing()
{
	return flashing;
}

void ServiceClass::setMailOn(bool On_Off)
{
	flags_MailOn = On_Off;
}

bool ServiceClass::getMailOn()
{
	return flags_MailOn;
}

void ServiceClass::set_confirm_message(bool On_Off)
{
	confirm_message = On_Off;
}

bool ServiceClass::get_confirm_message()
{
	return confirm_message;
}


void ServiceClass::setClearMessage(bool On_Off)
{
	ClearMessage = On_Off;
}

bool ServiceClass::get_ClearMessage()
{
	return ClearMessage;
}


void ServiceClass::setClearButton(bool On_Off)
{
	ClearButton = On_Off;
}

bool ServiceClass::get_ClearButton()
{
	return ClearButton;
}

void ServiceClass::set_count_buttton(uint8_t button)
{
	count_buttton = button;
}

uint8_t ServiceClass::get_count_buttton()
{
	return count_buttton;
}

void ServiceClass::set_num_buttton(uint8_t button)
{
	num_buttton = button;
}

uint8_t ServiceClass::get_num_buttton()
{
	return num_buttton;
}

//--------------------------------------------------------------------------------------------------------------------------------

bool array_countMax = false;
int sum_filtre = 0;
uint8_t array_count = 0;
uint8_t array_size = 50;
int dimension_array[50];


float ServiceClass::battery_read()
{

    long sum = 0;                  // sum of samples taken
    float voltage = 0.0;           // calculated voltage
    float output = 0.0;            // output value
    int val_voltage = 0;
    const float battery_max = settings->akk_max;// 4.2; // maximum voltage of battery
    const float battery_min = settings->akk_min;// 3.3; // minimum voltage of battery before shutdown

    float R1 = 330000.0;             // resistance of R1 (330K)
    float R2 = 430000.0;             // resistance of R2 (430K)
    int count_measurement = 40;      // количество измерений 
    float correction_factor = settings->akk_koef;// 3.3;   //?? поправочный коеффициент db

    for (int i = 0; i < count_measurement; i++)
    {
        sum += adc1_get_raw(ADC1_CHANNEL_2);
        delayMicroseconds(1000);
    }
    // calculate the voltage
    voltage = sum / (float)count_measurement;
    voltage = (voltage * 1.1) / 4096.0 * correction_factor;// 3.3; //for internal 1.1v reference
    voltage = voltage / (R2 / (R1 + R2));// *correction_factor;

    voltage = roundf(voltage * 100);

    /*Первичное заполнение фильтра при старте*/

    if (array_countMax == false)
    {
        for (int i = 0; i < array_size; i++)
        {
            dimension_array[i] = voltage;
        }

        array_count = array_size;          // 
        array_countMax = true;             //Разрешить выдавать данные об уровне напряжения аккумулятора
    }
    else
    {
        /*Массив заполнен первичными данными. Основной рачет напряжения*/
        array_count = array_size;          // 
        for (int i = 0; i < array_size; i++)
        {
            dimension_array[array_count] = dimension_array[array_count - 1];
            array_count--;
            if (array_count == 0)
            {
                dimension_array[array_count] = voltage;
            }
        }
        array_count = array_size;
        for (int i = 0; i < array_size; i++)       //формируем первичные (заполняем массив) данные об уровне напряжения аккумулятора
        {
            sum_filtre += dimension_array[i]; // Вычисление суммы
        }
        val_voltage = sum_filtre / array_size;
    }

    sum_filtre = 0;                                         //
    voltage = (float)val_voltage / 100;
    output = ((voltage - battery_min) / (battery_max - battery_min)) * 100;

    if (output < 100)
    {
        if (output < 0)
            output = 0;

        return output;
    }
    else
        return 100.0f;
}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void ServiceClass::set_GNSS_on_off(bool GNSSOn_Off)
{
    GNSS_On_Off = GNSSOn_Off;

}
bool ServiceClass::get_GNSS_on_off()
{

    return GNSS_On_Off;
}

void ServiceClass::set_time_hour(uint8_t hour)
{
    hour_m = hour;
}

uint8_t ServiceClass::get_time_hour()
{
    return hour_m;
}

void ServiceClass::set_time_minute(uint8_t minute)
{
    minute_m = minute;
}

uint8_t ServiceClass::get_time_minute()
{
    return minute_m;
}

//--------------------------------------------------------------------------------------------------------------------------------

