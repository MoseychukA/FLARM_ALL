#include "iarduino_GPS_ATGM336.h"																									//
																																	//
//		СОХРАНЕНИЕ НАСТРОЕК В FLASH ПАМЯТЬ МОДУЛЯ:																					//	Возвращаемое значение:	отсутствует.
void	iarduino_GPS_ATGM336::save(void){																							//	Параметры:				отсутствуют.
			if(_flgTypeSerial){																										//	Если модуль был инициализирован, то ...
				_printCommand("00");																								//	Команда $PCAS00*КОНТРОЛЬНАЯ_СУММА<CR><LF>
			}																														//
}																																	//	
																																	//
//		УСТАНОВКА ЧАСТОТЫ ОБНОВЛЕНИЯ ДАННЫХ:																						//	Возвращаемое значение:	отсутствует.
void	iarduino_GPS_ATGM336::updaterate(uint8_t i){																				//	Параметр:				i - частота обновления данных от 1 до 10 раз в секунду.
			if(_flgTypeSerial){																										//	Если модуль был инициализирован, то ...
				if(i<1 || i>10){return;}																							//	Запрещаем выводить данные чаще 10 раз в секунду и не выводить их вообще.
				char str[8] = "02,\0\0\0\0"; uint16_t j = 1000/i; uint8_t k = 3;													//	Определяем строку (str) для хранения команды, период (j) в миллисекундах и позицию (k) для подстановки символов в строку.
				if(j/1000){str[k]=j%10000/1000+'0'; k++;}																			//	Указываем старший   символ периода, если он есть.
				if(j/100 ){str[k]=j%1000 /100 +'0'; k++;}																			//	Указываем следующий символ периода, если он есть.
				if(j/10  ){str[k]=j%100  /10  +'0'; k++;}																			//	Указываем следующий символ периода, если он есть.
				if(j/1   ){str[k]=j%10   /1   +'0'; k++;}																			//	Указываем следующий символ периода, если он есть.
				_printCommand(str);																									//	Команда $PCAS02,ПЕРИОД*КОНТРОЛЬНАЯ_СУММА<CR><LF> - установка периода от 100 до 1000 мс.
			}																														//
}																																	//
																																	//
//		УСТАНОВКА СОСТАВА ПАКЕТА NMEA:																								//	Возвращаемое значение:	отсутствует.
void	iarduino_GPS_ATGM336::composition(uint8_t i1, uint8_t i2, uint8_t i3, uint8_t i4, uint8_t i5, uint8_t i6, uint8_t i7, uint8_t i8, uint8_t i9, uint8_t i10, uint8_t i11){	//	Параметры:	i1...i10 - флаги наличия идентификаторов GGA,GLL... в пакетах NMEA.
			if(_flgTypeSerial){																										//	Если модуль был инициализирован, то ...
				char  str[30] = "03,0,0,0,0,0,0,0,0,0,0,0,,0,0";																	//	Определяем строку (str) для хранения команды указания периодичности вывода идентификаторов в сообщениях NMEA.
				uint8_t i[11] = {i1,i2,i3,i4,i5,i6,i7,i8,i9,i10,i11};																//	Определяем массив полученных параметров данной функции.
				str[ 3]       = memchr(i, NMEA_GGA, 11)?'1':'0';																	//	Определяем 3  символ строки (str) как '0' или '1'.
				str[ 5]       = memchr(i, NMEA_GLL, 11)?'1':'0';																	//	Определяем 5  символ строки (str) как '0' или '1'.
				str[ 7]       = memchr(i, NMEA_GSA, 11)?'1':'0';																	//	Определяем 7  символ строки (str) как '0' или '1'.
				str[ 9]       = memchr(i, NMEA_GSV, 11)?'1':'0';																	//	Определяем 9  символ строки (str) как '0' или '1'.
				str[11]       = memchr(i, NMEA_RMC, 11)?'1':'0';																	//	Определяем 11 символ строки (str) как '0' или '1'.
				str[13]       = memchr(i, NMEA_VTG, 11)?'1':'0';																	//	Определяем 13 символ строки (str) как '0' или '1'.
				str[15]       = memchr(i, NMEA_ZDA, 11)?'1':'0';																	//	Определяем 15 символ строки (str) как '0' или '1'.
				str[17]       = memchr(i, NMEA_ANT, 11)?'1':'0';																	//	Определяем 17 символ строки (str) как '0' или '1'.
				str[19]       = memchr(i, NMEA_DHV, 11)?'1':'0';																	//	Определяем 19 символ строки (str) как '0' или '1'.
			//	str[21]       =           //   LPS          '0'           															//	Определяем 21 символ строки (str) как '0' (идентификатор LPS не поддерживается модулем).
				str[23]       = memchr(i, NMEA_TXT, 11)?'1':'0';																	//	Определяем 23 символ строки (str) как '0' или '1'.
			//	str[23]       =           //   ?																					//	                                          (идентификатор неизвестен).
			//	str[26]       =           //   UTC          '0'																		//	Определяем 26 символ строки (str) как '0' (идентификатор UTS не поддерживается модулем).
				str[28]       = memchr(i, NMEA_GST, 11)?'1':'0';																	//	Определяем 28 символ строки (str) как '0' или '1'.
				_printCommand(str);																									//	Команда $PCAS03,GGA,GLL,GSA,GSV,RMC,VTG,ZDA,ANT,DHV,LPS,?,?,UTC,GST*КОНТРОЛЬНАЯ_СУММА<CR><LF> - установка периодичности вывода идентификаторов в сообщениях NMEA.
			}																														//
}																																	//
																																	//
//		ВЫБОР ИСПОЛЬЗУЕМЫХ НАВИГАЦИОННЫХ СИСТЕМ:																					//	Возвращаемое значение:	отсутствует.
void	iarduino_GPS_ATGM336::system(uint8_t i1, uint8_t i2, uint8_t i3){															//	Параметры:				i1...i3 - флаги использования навигационных систем (GPS_GL - Glonass / GPS_BD - Beidu / GPS_GP - GPS).
			if(_flgTypeSerial){																										//	Если модуль был инициализирован, то ...
				if( i1>7 || i2>7 || i3>7 ){return;}																					//
				char str[5] = "04,0"; str[3] = (uint8_t)( ( (1<<i1) | (1<<i2) | (1<<i3) ) & 0x0F )+'0';								//	Определяем строку (str) для хранения команды выбора навигационных систем.
				_printCommand(str);																									//	Команда $PCAS04,БАЙТ_ФЛАГОВ*КОНТРОЛЬНАЯ_СУММА<CR><LF> - выбор навигационных систем.
			}																														//
}																																	//
																																	//
//		ВЫБОР ВЕРСИИ NMEA:																											//	Возвращаемое значение:	отсутствует.
void	iarduino_GPS_ATGM336::version(float i){																						//	Параметр:				i - версия NMEA.
			if(_flgTypeSerial){																										//	Если модуль был инициализирован, то ...
				if(i==2.2f){ i=9; }else if(i==4.0f){ i=5; }else if(i==4.1f){ i=2; }else{return;}									//
				char str[5] = "05,0"; str[3] = i+'0';																				//	Определяем строку (str) для хранения команды выбора версии NMEA.
				_printCommand(str);																									//	Команда $PCAS05,ВЕРСИЯ*КОНТРОЛЬНАЯ_СУММА<CR><LF> - выбор версии NMEA.
			}																														//
}																																	//
																																	//
//		ПЕРЕЗАГРУЗКА:																												//	Возвращаемое значение:	отсутствует.
void	iarduino_GPS_ATGM336::reset(uint8_t i){																						//	Параметр:				i - тип старта после перезагрузки (GPS_HOT_START / GPS_WARM_START / GPS_COLD_START / GPS_FACTORY_SET).
			if(_flgTypeSerial){																										//	Если модуль был инициализирован, то ...
				if( i>3 ){ return; }																								//
				char str[5] = "10,0"; str[3] = i+'0';																				//	Определяем строку (str) для хранения команды выбора версии NMEA.
				_printCommand(str);																									//	Команда $PCAS05,ТИП_СТАРТА*КОНТРОЛЬНАЯ_СУММА<CR><LF> - выбор версии NMEA.
			}																														//
}																																	//
																																	//
//		ТИП МОДЕЛИ:																													//	Возвращаемое значение:	отсутствует.
void	iarduino_GPS_ATGM336::model(uint8_t i){																						//	Параметр:				i - тип модели (GPS_PORTABLE / GPS_STATIC / GPS_WALKING / GPS_VEHICLE / GPS_VOYAGE / GPS_AVIATION_1G / GPS_AVIATION_2G / GPS_AVIATION_4G).
			if(_flgTypeSerial){																										//	Если модуль был инициализирован, то ...
				if( i>7 ){ return; }																								//
				char str[5] = "11,0"; str[3] = i+'0';																				//	Определяем строку (str) для хранения команды выбора динамической модели навигационной платформы.
				_printCommand(str);																									//	Команда $PCAS05,МОДЕЛЬ*КОНТРОЛЬНАЯ_СУММА<CR><LF> - выбор версии NMEA.
			}																														//
}																																	//
																																	//
//		ОЖИДАНИЕ ЛЮБОГО ОПРАТОРА NMEA:																								//	Возвращаемое значение:	флаг получения идентификатора NMEA из буфера UART.
bool	iarduino_GPS_ATGM336::_waitNMEA(void){																						//	Параметры:				отсутствуют.
		//	Определяем временные переменные:																						//
			char     _tmpChar;																										//	Объявляем  переменную для временного хранения символа.			(прочитанного из строки).
			char     _strOperator[6];																								//	Объявляем  строку для хранения названия идентификатора NMEA.			("GNGGA", "GNGLL", "GNRMC" и т.д.).
			uint32_t _timeOut = millis() + 1500;																					//	Определяем переменную для хранения времени выхода из функции.
			uint8_t  _valPos = 6;																									//	Определяем переменную для хранения позиции символа в названии идентификатора.
		//	Подготовка к чтению:																									//
			while(!_SerialReady()){;}																								//	Ждём завершение инициализации шины UART.
			_SerialFlush();																											//	Чистим буфер UART.
		//	Выполняем чтение данных:																								//
			while( millis() < _timeOut ){																							//	Читаем данные пока время «millis()» не достигнет значения «_timeOut».
			//	Если в буфере UART есть данные от модуля:																			//
				if(_SerialAvailable()){																								//
				//	Проверяем наличие идентификаторов NMEA:																			//
					_tmpChar=_SerialRead();																							//	Читаем очередной символ из буфера UATR.
					if(_tmpChar=='$'){																								//	Если обнаружено начало строки.
						_valPos = 0;																								//	Сбрасываем позицию символа в названии идентификатора.
					}else{																											//
						if(_valPos< 5){ _strOperator[_valPos]=_tmpChar; _valPos++; _strOperator[_valPos]=0; }						//	Читаем очередной символ названия идентификатора и добавляем символ конца строки.
						if(_valPos==5){ _valPos++;																					//	Запрещаем дальнейшую проверку до получения символа '$'.
						/* --GGA */ if( _strOperator[2]=='G' && _strOperator[3]=='G' && _strOperator[4]=='A' ){return true;}		//	Если строка «_strOperator» состоит их символов "--GGA" (первые 2 символа могут быть любыми), то возвращаем положительный результат.
						/* --GLL */ if( _strOperator[2]=='G' && _strOperator[3]=='L' && _strOperator[4]=='L' ){return true;}		//	Если строка «_strOperator» состоит их символов "--GLL" (первые 2 символа могут быть любыми), то возвращаем положительный результат.
						/* --RMC */ if( _strOperator[2]=='R' && _strOperator[3]=='M' && _strOperator[4]=='C' ){return true;}		//	Если строка «_strOperator» состоит их символов "--RMC" (первые 2 символа могут быть любыми), то возвращаем положительный результат.
						/* --VTG */ if( _strOperator[2]=='V' && _strOperator[3]=='T' && _strOperator[4]=='G' ){return true;}		//	Если строка «_strOperator» состоит их символов "--VTG" (первые 2 символа могут быть любыми), то возвращаем положительный результат.
						/* --ZDA */ if( _strOperator[2]=='Z' && _strOperator[3]=='D' && _strOperator[4]=='A' ){return true;}		//	Если строка «_strOperator» состоит их символов "--ZDA" (первые 2 символа могут быть любыми), то возвращаем положительный результат.
						/* --GSA */ if( _strOperator[2]=='G' && _strOperator[3]=='S' && _strOperator[4]=='A' ){return true;}		//	Если строка «_strOperator» состоит их символов "--GSA" (первые 2 символа могут быть любыми), то возвращаем положительный результат.
						/* --GSV */ if( _strOperator[2]=='G' && _strOperator[3]=='S' && _strOperator[4]=='V' ){return true;}		//	Если строка «_strOperator» состоит их символов "--GSV" (первые 2 символа могут быть любыми), то возвращаем положительный результат.
						/* --TXT */ if( _strOperator[2]=='T' && _strOperator[3]=='X' && _strOperator[4]=='T' ){return true;}		//	Если строка «_strOperator» состоит их символов "--TXT" (первые 2 символа могут быть любыми), то возвращаем положительный результат.
						}																											//
					}																												//
				}																													//
			}																														//
			return false;																											//
}																																	//
//		ОТПРАВКА КОМАНДЫ ПО UART С ДОБАВЛЕНИЕМ ПРЕФИКСА КОМАНДЫ '$PCAS' И КОНТРОЛЬНОЙ СУММЫ:										//	Возвращаемое значение:	отсутствует.
void	iarduino_GPS_ATGM336::_printCommand(const char* str){																		//	Параметры:				строка состоящая из номера команды и её параметров.
			uint8_t _checkSum  = 1;																									//	Определяем переменную для подсчёта контрольной суммы. Значение 1 соответствует строке "PCAS".
			char    _strEnd[6] = "*--\r\n";																							//	Определяем строку содержащую символы контрольной суммы.
			for(uint8_t i=0; i<255; i++){ if(str[i]==0){break;}else{_checkSum ^= str[i];} }											//	Вычисляем контрольную сумму.
			_strEnd[1]=(_checkSum/16)+'0'; if(_strEnd[1]>'9'){_strEnd[1]+=7;}														//	Определяем 1 символ контрольной суммы.
			_strEnd[2]=(_checkSum%16)+'0'; if(_strEnd[2]>'9'){_strEnd[2]+=7;}														//	Определяем 2 символ контрольной суммы.
			_SerialPrint("$PCAS");																									//	Выводим префикс команды.
			_SerialPrint(str);																										//	Выводим номер команды и её параметры.
			_SerialPrint(_strEnd);																									//	Выводим строку содержащую контрольную сумму.
}																																	//
																																	//
//		ФУНКЦИИ РАБОТЫ С UART:																										//
void	iarduino_GPS_ATGM336::_SerialFlush(void){ while(_SerialAvailable()){_SerialRead();} }										//	Очистка буфера UART.
bool	iarduino_GPS_ATGM336::_SerialReady(void){																					//
			if(_flgTypeSerial==1){return (bool   )(*(HardwareSerial*)_objSerial);            }else									//	Проверка готовности аппаратного UART.
#ifndef RENESAS_CORTEX_M4																											//	Библиотека SoftwareSerial для плат Arduino UNO R4 не поддерживает operator!(bool). По состоянию на март 2024г.
			if(_flgTypeSerial==2){return (bool   )(*(SOFTwareSerial*)_objSerial);            }else									//	Проверка готовности программного UART.
#endif																																//
			                     {return 1;}																						//
}																																	//
uint8_t	iarduino_GPS_ATGM336::_SerialRead(void){																					//
			if(_flgTypeSerial==1){return (uint8_t)(*(HardwareSerial*)_objSerial).read();     }else									//	Чтение байта из буфера аппаратного UART.
			if(_flgTypeSerial==2){return (uint8_t)(*(SOFTwareSerial*)_objSerial).read();     }else									//	Чтение байта из буфера программного UART.
			                     {return 0;}																						//
}																																	//
uint8_t	iarduino_GPS_ATGM336::_SerialAvailable(void){																				//
			if(_flgTypeSerial==1){return (uint8_t)(*(HardwareSerial*)_objSerial).available();}else									//	Чтение заполненности буфера аппаратного UART.
			if(_flgTypeSerial==2){return (uint8_t)(*(SOFTwareSerial*)_objSerial).available();}else									//	Чтение заполненности буфера программного UART.
			                     {return 0;}																						//
}																																	//
void	iarduino_GPS_ATGM336::_SerialPrint(const char* i){																			//
			if(_flgTypeSerial==1){                (*(HardwareSerial*)_objSerial).print(i);   }else									//	Отправка строки по шине аппаратного UART.
			if(_flgTypeSerial==2){                (*(SOFTwareSerial*)_objSerial).print(i);   }										//	Отправка строки по шине программного UART.
}																																	//
