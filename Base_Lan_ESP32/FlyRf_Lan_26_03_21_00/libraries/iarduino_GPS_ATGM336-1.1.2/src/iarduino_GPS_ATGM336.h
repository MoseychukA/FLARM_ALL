//	Библиотека для работы с GPS модулем ATGM336: https://iarduino.ru/shop/Expansion-payments/gps-glonass-modul-trema-modul.html
//  Версия: 1.1.2
//  Последнюю версию библиотеки Вы можете скачать по ссылке: https://iarduino.ru/file/539.html
//  Подробное описание функций бибилиотеки доступно по ссылке: https://wiki.iarduino.ru/page/GPS-module/
//  Библиотека является собственностью интернет магазина iarduino.ru и может свободно использоваться и распространяться!
//  При публикации устройств или скетчей с использованием данной библиотеки, как целиком, так и её частей,
//  в том числе и в некоммерческих целях, просим Вас опубликовать ссылку: http://iarduino.ru
//  Автор библиотеки: Панькин Павел.
//  Если у Вас возникли технические вопросы, напишите нам: shop@iarduino.ru

#ifndef iarduino_GPS_ATGM336_h																										//
#define iarduino_GPS_ATGM336_h																										//
																																	//
#if defined(__SOFTWARE_SERIAL_H__) || defined(SoftwareSerial_h)																		//	Если в скетче подключена библиотека  SoftwareSerial,
	#include <SoftwareSerial.h>																										//	то разрешаем работать  с библиотекой SoftwareSerial.
	#define incSWSerial																												//	Флаг разрешающий определить функцию begin(SoftwareSerial&)
	#define SOFTwareSerial SoftwareSerial																							//	Имя класса SoftwareSerial
#else																																//	Если библиотека не подключена, то ...
	#define SOFTwareSerial HardwareSerial																							//	Нельзя использовать имя класса SoftwareSerial, используем HardwareSerial.
#endif																																//
																																	//
#if defined(ARDUINO) && (ARDUINO >= 100)																							//
#include		<Arduino.h>																											//
#else																																//
#include		<WProgram.h>																										//
#endif																																//
																																	//
#define GPS_HOT_START		0																										//	Парамер функции reset()       указывающий стартовать с сохранением заданных настроек и данных о спутниках.
#define GPS_WARM_START		1																										//	Парамер функции reset()       указывающий стартовать с сохранением заданных настроек.
#define GPS_COLD_START		2																										//	Парамер функции reset()       указывающий стартовать с сохранением заданных настроек и сбросом данных о спутниках.
#define GPS_FACTORY_SET		3																										//	Парамер функции reset()       указывающий стартовать с восстановлением заводских настроек и сбросом данных о спутниках.
#define GPS_GP				0																										//	Парамер функции system()      указывающий использовать навигационную систему GPS.
#define GPS_BD				1																										//	Парамер функции system()      указывающий использовать навигационную систему Beidu.
#define GPS_GL				2																										//	Парамер функции system()      указывающий использовать навигационную систему Glonass.
#define GPS_PORTABLE		0																										//	Парамер функции model()       указывающий на использование модуля в портативном устройстве.
#define GPS_STATIC			1																										//	Парамер функции model()       указывающий на использование модуля в статично.
#define GPS_WALKING			2																										//	Парамер функции model()       указывающий на использование модуля пешеходом.
#define GPS_VEHICLE			3																										//	Парамер функции model()       указывающий на использование модуля в автомобиле.
#define GPS_VOYAGE			4																										//	Парамер функции model()       указывающий на использование модуля на морском судне.
#define GPS_AVIATION_1G		5																										//	Парамер функции model()       указывающий на использование модуля на воздушном судне < 1g.
#define GPS_AVIATION_2G		6																										//	Парамер функции model()       указывающий на использование модуля на воздушном судне < 2g.
#define GPS_AVIATION_4G		7																										//	Парамер функции model()       указывающий на использование модуля на воздушном судне < 4g.
#define NMEA_GGA			1																										//	Парамер функции composition() указывающий включить строку с идентификатором GGA в пакет NMEA.
#define NMEA_GLL			2																										//	Парамер функции composition() указывающий включить строку с идентификатором GLL в пакет NMEA.
#define NMEA_GSA			3																										//	Парамер функции composition() указывающий включить строку с идентификатором GSA в пакет NMEA.
#define NMEA_GSV			4																										//	Парамер функции composition() указывающий включить строку с идентификатором GSV в пакет NMEA.
#define NMEA_RMC			5																										//	Парамер функции composition() указывающий включить строку с идентификатором RMC в пакет NMEA.
#define NMEA_VTG			6																										//	Парамер функции composition() указывающий включить строку с идентификатором VTG в пакет NMEA.
#define NMEA_ZDA			7																										//	Парамер функции composition() указывающий включить строку с идентификатором ZDA в пакет NMEA.
#define NMEA_ANT			8																										//	Парамер функции composition() указывающий включить строку с идентификатором TXT с информацией о качестве антенны в пакет NMEA.
#define NMEA_DHV			9																										//	Парамер функции composition() указывающий включить строку с идентификатором DHV в пакет NMEA.
#define NMEA_TXT			10																										//	Парамер функции composition() указывающий включить строку с идентификатором TXT с информацией о чём то недокументорованном в пакет NMEA.
#define NMEA_GST			11																										//	Парамер функции composition() указывающий включить строку с идентификатором GST в пакет NMEA.
																																	//
class iarduino_GPS_ATGM336{																											//
	public:																															//
	/**	Пользовательские функции **/																								//
		bool		begin			(HardwareSerial &i				){ _flgTypeSerial=1; _objSerial=&i; return _begin(); }			//	Определяем функцию инициализации работы с модулем		(Параметр:  объект для работы с аппаратным  UART).
		#ifdef incSWSerial																											//
		bool		begin			(SoftwareSerial &i				){ _flgTypeSerial=2; _objSerial=&i; return _begin(); }			//	Определяем функцию инициализации работы с модулем		(Параметр:  объект для работы с программным UART).
		#endif																														//
		void		save			(void							);																//	Объявляем  функцию сохранения настроек в Flash модуля	(Параметр:  отсутствует).
		void		updaterate		(uint8_t						);																//	Объявляем  функцию установки частоты обновления данных	(Параметр:  частота обновления данных от 1 до 10 раз в секунду).
		void		composition		(uint8_t=0,uint8_t=0,uint8_t=0,uint8_t=0,uint8_t=0,uint8_t=0,uint8_t=0,uint8_t=0,uint8_t=0,uint8_t=0,uint8_t=0);	//	Объявляем функцию установки состава пакета NMEA	(Параметры: флаги наличия строк с указанными идентификаторами в пакетах NMEA в любой комбинации).
		void		system			(uint8_t=4,uint8_t=4,uint8_t=4	);																//	Объявляем  функцию выбора навигационных систем			(Параметры:	GPS_GP/GPS_GL/GPS_BD в любой комбинации).
		void		version			(float							);																//	Объявляем  функцию выбора версии NMEA					(Параметр:  версия NMEA: 2.2/4.0/4.1).
		void		reset			(uint8_t						);																//	Объявляем  функцию перезагрузки модуля					(Параметр:  тип старта после перезагрузки GPS_HOT_START/GPS_WARM_START/GPS_COLD_START/GPS_FACTORY_SET).
		void		model			(uint8_t						);																//	Объявляем  функцию выбора модели использования модуля	(Параметр:  тип модели GPS_PORTABLE/GPS_STATIC/GPS_WALKING/GPS_VEHICLE/GPS_VOYAGE/GPS_AVIATION_1G/GPS_AVIATION_2G/GPS_AVIATION_4G).
		uint32_t	baudrate		(uint32_t bps=0					){																//	Объявляем  функцию установки скорости					(Параметр:  скорость 4800/9600/19200/38400/57600/115200, если параметр отсутствует то будет установлена скорость на которую настроен модуль).
										if(_flgTypeSerial){																			//	Если модуль был инициализирован, то ...
											if(bps){																				//
												if( bps==  4800 ){ _printCommand("01,0"); }else										//	Команда $PCAS01,0*КОНТРОЛЬНАЯ_СУММА<CR><LF> - установка скорости   4800.
												if( bps==  9600 ){ _printCommand("01,1"); }else										//	Команда $PCAS01,1*КОНТРОЛЬНАЯ_СУММА<CR><LF> - установка скорости   9600.
												if( bps== 19200 ){ _printCommand("01,2"); }else										//	Команда $PCAS01,2*КОНТРОЛЬНАЯ_СУММА<CR><LF> - установка скорости  19200.
												if( bps== 38400 ){ _printCommand("01,3"); }else										//	Команда $PCAS01,3*КОНТРОЛЬНАЯ_СУММА<CR><LF> - установка скорости  38400.
												if( bps== 57600 ){ _printCommand("01,4"); }else										//	Команда $PCAS01,4*КОНТРОЛЬНАЯ_СУММА<CR><LF> - установка скорости  57600.
												if( bps==115200 ){ _printCommand("01,5"); }else{ return 0; }						//	Команда $PCAS01,5*КОНТРОЛЬНАЯ_СУММА<CR><LF> - установка скорости 115200.
												delay(100); _SerialBegin( bps ); while(!_SerialReady()){;}							//	Устанавливаем скорость шины UART.
												return _waitNMEA();																	//	Возвращаем результат установки скорости.
											}else{						if( _waitNMEA() ){ return      1; }							//	Скорость модуля совпадает с текущей скоростью шины UART.
												_SerialBegin(  9600);	if( _waitNMEA() ){ return   9600; }							//	Проверяем получение идентификаторов NMEA по UART на скорости 9600.
												_SerialBegin(  4800);	if( _waitNMEA() ){ return   4800; }							//	Проверяем получение идентификаторов NMEA по UART на скорости 4800.
												_SerialBegin( 19200);	if( _waitNMEA() ){ return  19200; }							//	Проверяем получение идентификаторов NMEA по UART на скорости 19200.
												_SerialBegin( 38400);	if( _waitNMEA() ){ return  38400; }							//	Проверяем получение идентификаторов NMEA по UART на скорости 38400.
												_SerialBegin( 57600);	if( _waitNMEA() ){ return  57600; }							//	Проверяем получение идентификаторов NMEA по UART на скорости 57600.
												_SerialBegin(115200);	if( _waitNMEA() ){ return 115200; }							//	Проверяем получение идентификаторов NMEA по UART на скорости 115200.
												_SerialBegin(  9600);																//	Если скорость не обнаружена, то устанавливаем скорость по умолчанию.
											}																						//
										}	return 0;																				//
									}																								//
																																	//
	/**	Пользовательские переменные **/																								//
	private:																														//
	/**	Внутренние переменные **/																									//
		void*		_objSerial;																										//	Объявляем  указатель на объект работы с UART			(Serial, Serial1, ..., SoftwareSerial).
		uint8_t		_flgTypeSerial	= 0;																							//	Определяем флаг указывающий на тип соединения			(0-нет, 1-HardwareSerial, 2-SoftwareSerial).
	/**	Внутренние функции **/																										//
		bool		_waitNMEA		(void							);																//	Объявляем  функцию ожидания любого опратора NMEA		(Параметр:  отсутствует).
		void		_printCommand	(const char*					);																//	Объявляем  функцию отправки команды с контрольн суммой	(Параметр:  строка состоящая из номера команды и её параметров).
		bool		_SerialReady	(void							);																//	Объявляем  функцию проверки готовности UART				(Параметр:  отсутствует).
		uint8_t		_SerialRead		(void							);																//	Объявляем  функцию чтения байта из буфера UART			(Параметр:  отсутствует).
		uint8_t		_SerialAvailable(void							);																//	Объявляем  функцию чтения заполненности буфера UART		(Параметр:  отсутствует).
		void		_SerialPrint	(const char*					);																//	Объявляем  функцию отправки строки по шине UART			(Параметр:  указатель на первый символ строки).
		void		_SerialFlush	(void							);																//	Объявляем  функцию очистки буфера UART					(Параметр:  отсутствует).
		void		_SerialBegin	(uint32_t i						){																//	Определяем функцию установки скорости UART				(Параметр:  скорость).
										if(_flgTypeSerial==1){ (*(HardwareSerial*)_objSerial).begin(i); }else						//	Устанавливаем скорость аппаратной  шины UART.
										if(_flgTypeSerial==2){ (*(SOFTwareSerial*)_objSerial).begin(i); }							//	Устанавливаем скорость программной шины UART.
									}																								//
		bool		_begin			(void							){																//	Определяем функцию инициализации работы с модулем		(Параметр:  отсутствует).
										if(_flgTypeSerial){																			//	Если модуль был инициализирован, то ...
											if( !_SerialReady() ){ delay(500); }													//	Ждём инициализации работы с шиной UART.
											if( !_SerialReady() ){ _SerialBegin(9600); while(!_SerialReady()){;} }					//	Инициируем работу с шиной UART если она не инициирована.
										//	Возвращаем результат:																	//
											return baudrate()? true:false;															//	Устанавливаем скорость шины UART равную скорости работы модуля. Функция baudrate вернет 0 если скорость модуля не определена.
										}	return false;																			//
									}																								//	
};																																	//
																																	//
#endif																																//