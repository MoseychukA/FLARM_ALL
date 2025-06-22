#ifndef __ADSB_ENCODER_INCLUDE__
#define __ADSB_ENCODER_INCLUDE__


enum DF { DF17  , DF18 , DF18ANON , DF18TRACK};


typedef struct  frame_data
{
	unsigned char msg[14];
}frame_data_t;


#define CPR_ODD		1
#define CPR_EVEN	0

#define Category_Set_D	1
#define Category_Set_C	2
#define Category_Set_B	3
#define Category_Set_A	4

#define AIR_POS			0
#define SURFACE_POS		1


/*
adsb Инициализация кодирования
*/
void adsb_encoder_init(); 


/*
Сгенерировать сообщение о воздушном положении
*/
frame_data_t  make_air_position_frame(
	unsigned short metype,  //[9,18] , [20,22]
	unsigned int addr,
	double lat, 
	double  lon,
	double alt, //ft
	unsigned int oddflag,
	DF df); 

/*
Генерация сообщения о местоположении на земле
*/
frame_data_t  make_surface_position_frame(
	unsigned short metype,   //[5,8]
	unsigned int addr,
	double lat, double  lon,
	unsigned int knot, //Скорость относительно земли (узл.)
	bool heading_valid,  //Заголовок действителен?
	double heading, //Направление скорости(0~360)
	unsigned int oddflag, DF df); 


/*
Создать сообщение классификации личности
*/
frame_data_t make_aircraft_identification_frame(
	unsigned int addr,
	unsigned char callsign[8], //Номер рейса, если номер меньше 8 байт, добавьте 0x00
	unsigned short category_set,
	unsigned short Category,
	DF df);

/*
Генерация сообщения о скорости относительно земли
*/
frame_data_t make_velocity_frame(
	unsigned int addr,
	double nsvel,  //Скорость север-юг (узлы, север положительный)
	double ewvel,  //Скорость восток-запад (узлы, восток положительный)
	double vrate,  //Скорость подъема (фут/мин, восходящее положительное направление)
	DF df); 


#endif
