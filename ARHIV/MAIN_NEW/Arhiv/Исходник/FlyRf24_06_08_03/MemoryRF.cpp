#include "MemoryRF.h"
#include "Configuration_ESP32.h"  // Основные настройки программы
#include "EEPROM.h"

EEPROMClass mem("mail");




//--------------------------------------------------------------------------------------------------------------------------------
void MemInit()
{
   // uint8_t  mail[Max_Count_Block_Message][128];             // Массив для сообщений почты
    if (!mem.begin((Max_Count_Block_Message* Number_of_bytes_block)+400)) //2560
    {
        Serial.println("Failed to initialise MAIL_DATA");
        Serial.println("Restarting...");
        delay(1000);
        // ESP.restart();
    }
    else
    {
         Serial.println("Initialise MAIL_DATA");
    }
}

//--------------------------------------------------------------------------------------------------------------------------------
void MemClear()
{
 //   LCD_Class* dc = LCDScreen->getDC();
 //
 //   if (!dc)
 //   {
 //       return;
 //   }


 //   uint32_t len = 0;

 //   len = 16384; // Settings.GetMemorySize();

 //   Settings.displayBacklight(true);                                 // Включить подсветку дисплея

 //   dc->clear();
	//DBG("Max address - ");
	//DBGLN(len);

 //   int step_clear = len / 20;
 //   byte step_cursor = 0;
 //   char str_clear[1] = {0xFE};
 //   int step_reset = 2048;                                       // Интералы сброса таймера ничего не деланья

 //   char str[20];
 //   itoa(len, str, 10);                                           // Записать в строку номер сообщения в памяти
 //   dc->clear();                                                  // Стереть экран
 //   dc->setCursor(0, 0);                                          // Установить курсор в начало экрана
 //   dc->print("Start setting clear.");
 //   dc->setCursor(0, 1);
 //   dc->print("Size - ");
 //   dc->print(str);

 //   dc->powerMode(SSD1311_LCD_OFF);
 //   dc->cursor_on = false;                       // 
 //   dc->cursor_blinking = true;                      // 
 //   dc->cursor_direction = SSD1311_DIRECTION_RIGHT;
 //   dc->BDC = false;
 //   dc->setEntryMode();
 //   dc->powerMode(SSD1311_LCD_ON);

 //   uint32_t address = 0;
 //   DBGLN(F("Start setting clear."));
 //   for (address = 0; address < len; address++)
 //   {
 //       mem->write(address, 0x00);
 //       #ifdef USE_WATCHDOG_TIMER
 //       Settings.reset_IWDG();      // Сброс сторожевого таймера 
 //       #endif
 //       if ((address % step_clear) == 0)
 //       {
 //           itoa(address, str, 10);                              // Записать в строку номер сообщения в памяти
 //           dc->setCursor(14, 1);                                // Установить курсор в начало экрана
 //           dc->print(str);         
 //           dc->setCursor(step_cursor, 2);
 //           step_cursor++;
 //           dc->setCursor(step_cursor - 1, 2);
 //           dc->print("X");
 //           dc->setCursor(step_cursor - 1, 2);
 //           DBGLN(str);
 //        }
 //       //step_reset таймер
 //       if ((address % step_reset) == 0) // периодически сбрасываем таймер ничего не деланья
 //       {
 //           LCDScreen->resetIdleTimer();

 //       }

 //   } // for

 // 

	////DBGLN("End..");

 //   dc->powerMode(SSD1311_LCD_OFF);
 //   dc->cursor_on = false;                      // 
 //   dc->cursor_blinking = false;                      // 
 //   dc->cursor_direction = SSD1311_DIRECTION_RIGHT;
 //   dc->BDC = false;
 //   dc->setEntryMode();
 //   dc->powerMode(SSD1311_LCD_ON);
 //   MemWrite(0, MEM_CONTROL_BYTE);                              // Запись контрольного бита, означающего инициализацию памяти
 //   MemWrite(All_Count_Text_Message_ADDRESS, 0);                // адрес хранения счетчика общего количества записей
 //   MemWrite(Count_NotRead_Message_ADDRESS, 0);                 // адрес хранения счетчика не прочитанного количества записей
 //   MemWrite(Current_Counter_Message, 0);                       // адрес хранения текущего счетчика количества записей
 //   MemWrite(Flipping_Counter_Message, 0);                      // адрес хранения счетчика листания записей
 //  // MemWrite(TmpCount_Flip_Message_ADDRESS, 0);                 // адрес хранения временного счетчика количества записей
 //   MemWrite(Response_message_block_ADDRESS, 0);                // начальный адрес хранения счетчика ответных сообщений
 //   MemWrite(Current_Counter_confirmation, 0);                  // сохраним новый номер сохраненного подтверждения.
 //   dc->setCursor(0, 3);                                        // Установить курсор в начало экрана
 //   dc->print("End..");                                       // Отобразить новое сообщение
 //   DBGLN(F("EEPROM clearance END"));
 //   delay(1000);
 //   CommandHandler.Stm32_SoftReset();

}


//--------------------------------------------------------------------------------------------------------------------------------
void MemWriteChars(unsigned int address, char* data, int length)
{
    mem.writeBytes(address,data, length);
}
//--------------------------------------------------------------------------------------------------------------------------------
void MemReadChars(unsigned int address, char* data, int n)
{
    mem.readBytes(address, data, n);
}
//--------------------------------------------------------------------------------------------------------------------------------
void MemWrite(unsigned int address, byte data)
{
    mem.writeByte(address, data);
}
//--------------------------------------------------------------------------------------------------------------------------------
void MemWrite(unsigned int address, byte* data, int n)
{
    mem.writeBytes(address, data, n);
}
//--------------------------------------------------------------------------------------------------------------------------------
void MemWriteInt(unsigned int address, unsigned int data)
{
    mem.writeInt(address, data);
}
//--------------------------------------------------------------------------------------------------------------------------------
void MemWriteLong(unsigned int address, unsigned long data)
{
    mem.writeLong(address, data);
}
//--------------------------------------------------------------------------------------------------------------------------------
void MemWriteFloat(unsigned int address, float data)
{
    mem.writeFloat(address, data);
}
//--------------------------------------------------------------------------------------------------------------------------------
void MemWriteDouble(unsigned int address, double data)
{
    mem.writeDouble(address, data);
}
//--------------------------------------------------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------------------------------------------------
void MemRead(unsigned int address, byte* data, int n)
{
    mem.readBytes(address, data, n);
}
//--------------------------------------------------------------------------------------------------------------------------------
unsigned int MemReadInt(unsigned int address)
{
    mem.readInt(address);
}
//--------------------------------------------------------------------------------------------------------------------------------
unsigned long MemReadLong(unsigned int address)
{
    mem.readLong(address);
}
//--------------------------------------------------------------------------------------------------------------------------------
float MemReadFloat(unsigned int address)
{
    mem.readFloat(address);
}
//--------------------------------------------------------------------------------------------------------------------------------
double MemReadDouble(unsigned int address)
{
   mem.readDouble(address);
}
//--------------------------------------------------------------------------------------------------------------------------------
uint8_t MemRead(unsigned int address)
{
    return mem.readByte(address);
}

//--------------------------------------------------------------------------------------------------------------------------------
String MemReadString(unsigned int address)
{
    return mem.readString(address);
}
//--------------------------------------------------------------------------------------------------------------------------------
void MemCommit()
{
    mem.commit();
}

//--------------------------------------------------------------------------------------------------------------------------------

/*
    uint8_t readByte(int address);
    int8_t readChar(int address);
    uint8_t readUChar(int address);
    int16_t readShort(int address);
    uint16_t readUShort(int address);
    int32_t readInt(int address);
    uint32_t readUInt(int address);
    int32_t readLong(int address);
    uint32_t readULong(int address);
    int64_t readLong64(int address);
    uint64_t readULong64(int address);
    float_t readFloat(int address);
    double_t readDouble(int address);
    bool readBool(int address);
    size_t readString(int address, char* value, size_t maxLen);
    String readString(int address);
    size_t readBytes(int address, void * value, size_t maxLen);
    template <class T> T readAll (int address, T &);

    size_t writeByte(int address, uint8_t value);
    size_t writeChar(int address, int8_t value);
    size_t writeUChar(int address, uint8_t value);
    size_t writeShort(int address, int16_t value);
    size_t writeUShort(int address, uint16_t value);
    size_t writeInt(int address, int32_t value);
    size_t writeUInt(int address, uint32_t value);
    size_t writeLong(int address, int32_t value);
    size_t writeULong(int address, uint32_t value);
    size_t writeLong64(int address, int64_t value);
    size_t writeULong64(int address, uint64_t value);
    size_t writeFloat(int address, float_t value);
    size_t writeDouble(int address, double_t value);
    size_t writeBool(int address, bool value);
    size_t writeString(int address, const char* value);
    size_t writeString(int address, String value);
    size_t writeBytes(int address, const void* value, size_t len);
    template <class T> T writeAll (int address, const T &);




*/

