#include "SettingsMain.h"
#include <malloc.h>
#include <stdlib.h>
#include <stdio.h>
#include <Arduino.h>
#include "Configuration_ESP32.h"
#include "Memory.h"               // ������ � ����������������� �������

//--------------------------------------------------------------------------------------------------------------------------------
SettingsClass SettingsMain;
//--------------------------------------------------------------------------------------------------------------------------------
SettingsClass::SettingsClass()
{


}

//--------------------------------------------------------------------------------------------------------------------------------
void SettingsClass::setup()
{

	pinMode(BUTTON_MAIL, INPUT_PULLUP);       // ��������� ������ "POINT"

}

void SettingsClass::update()
{
	

}



bool  SettingsClass::get_empty_buffer_request()
{
	return empty_buffer;
}

void SettingsClass::set_empty_buffer_request(bool buffer_request)
{
	empty_buffer = buffer_request;
}

//1
//--------------------------------------------------------------------------------------------------------------------------------
/* �������� ��������� �������� �� �������������� ��������� */
uint8_t SettingsClass::getCoutNotReadMessage()
{
	// �������� ���������� ���������� ���������
	uint8_t mess_count = MemRead(Count_NotRead_Message_ADDRESS);
	return mess_count;
}

/* ��������� � ������ ���������� �� �������������� ��������� */
void SettingsClass::setCoutNotReadMessage(uint8_t count)
{
	// ��������� ���������� ���������� ���������
	MemWrite(Count_NotRead_Message_ADDRESS, count);// ��������� ��������� �������� �� ������������ ���������� ������� 
	//MemCommit();
}

//2
//--------------------------------------------------------------------------------------------------------------------------------
uint8_t  SettingsClass::getCurrentCountMessage()
{
	// �������� ������� ����� ���������
	uint8_t current_count_mess = MemRead(Current_Counter_Message); // �������� ��������� �������� ��������
	return current_count_mess;
}

void SettingsClass::setCurrentCountMessage(uint8_t count_cur)
{
	// ��������� ������� ����� ���������
	MemWrite(Current_Counter_Message, count_cur);  // ��������� ������� ��������� ��������
	//MemCommit();
}

//2
//--------------------------------------------------------------------------------------------------------------------------------
uint8_t  SettingsClass::getNumber_from_Message()
{
	// �������� ������� ����� ���������
	uint8_t number_from_message = MemRead(Number_from_Message_ADDRESS); // �������� ��������� �������� ��������
	return number_from_message;
}

void SettingsClass::setNumber_from_Message(uint8_t count_cur)
{
	// ��������� ������� ����� ���������
	MemWrite(Number_from_Message_ADDRESS, count_cur);  // ��������� ������� ��������� ��������
	//MemCommit();
}



//--------------------------------------------------------------------------------------------------------------------------------
bool SettingsClass::getNewMessageFlag()
{
	// �������� ������� ����� ���������  MESSAGE_CONFIRMED_flag

	bool new_flag = MemRead(MESSAGE_CONFIRMED_flag);  // �������� ���� ������ ���������
	return new_flag;
}

void SettingsClass::setNewMessageFlag(bool new_flag)
{
	// ��������� ���� ������ ���������
	MemWrite(MESSAGE_CONFIRMED_flag, new_flag);      // ��������� ���� ������ ���������
	//MemCommit();

}

//--------------------------------------------------------------------------------------------------------------------------------
/*  ��������� ��������� ������� �������� ���������*/
//--------------------------------------------------------------------------------------------------------------------------------
uint8_t SettingsClass::getFlippingCountMessage()
{
	// �������� ������� ����� ���������
	uint8_t current_count_mess = MemRead(Flipping_Counter_Message); // �������� ������� ��������� ��������� ��������
	return current_count_mess;
}

void SettingsClass::setFlippingCountMessage(uint8_t count_cur)
{
	// ��������� ������� ����� ���������
	MemWrite(Flipping_Counter_Message, count_cur);      // ��������� ������� ��������� ��������
	//MemCommit();
}
//--------------------------------------------------------------------------------------------------------------------------------
void SettingsClass::SetButtonRetention(bool val) // ��������
{
	ButtonRetention = val;
}

//--------------------------------------------------------------------------------------------------------------------------------
void SettingsClass::saveVer(String ver)
{
	Current_version = ver;
}

String SettingsClass::getVer()
{
	return Current_version;
}

//--------------------------------------------------------------------------------------------------------------------------------
