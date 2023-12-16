// FSMC_LowLevel.h - низкоуровневый драйвер FSMC для LCD

// Инициализация FSMC
void FSMC_Init ( void );

// Запись в регистр LCD
void FSMC_LcdWriteReg ( uint8_t LCD_Reg, uint16_t LCD_RegValue );

// Чтение регистра LCD
uint16_t FSMC_LcdReadReg ( uint8_t LCD_Reg );

// Запись команды в LCD
void FSMC_LcdWriteCmd ( uint16_t val );

// чтение команды из LCD
uint16_t FSMC_LcdReadCmd ( void );

// чтение данных из LCD
uint16_t FSMC_LcdReadData ( void );

// Запись данных в LCD
void FSMC_LcdWriteData ( uint16_t val );

// Передача блока данных по DMA в LCD
// count - длина данных в байтах, д.б. кратна 4
void FSMC_TransferDataDMAToLCD ( void *buffer, uint32_t count, bool isRamAddressIncrement );

// Передача блока данных по DMA из LCD
// count - длина данных в байтах, д.б. кратна 4
void FSMC_TransferDataDMAFromLCD ( void *buffer, uint32_t count );

