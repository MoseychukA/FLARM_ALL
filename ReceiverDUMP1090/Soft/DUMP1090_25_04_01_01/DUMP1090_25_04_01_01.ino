/*
Raspberry Pi Pico RP2040
Программа приема и расшифровки информации с приемника DUMP1090



*/

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <pthread.h>
#include <unistd.h>
//#include <libusb.h>



#ifdef _WIN32
#define sleep Sleep
#if defined(_MSC_VER) && (_MSC_VER < 1800)
#define round(x) (x > 0.0 ? floor(x + 0.5): ceil(x - 0.5))
#endif
#endif

#define ADSB_RATE			2000000
#define ADSB_FREQ			1090000000
#define DEFAULT_ASYNC_BUF_NUMBER	12
#define DEFAULT_BUF_LENGTH		(16 * 16384)
#define AUTO_GAIN			-100

#define MESSAGEGO    253
#define OVERWRITE    254
#define BADSAMPLE    255

static pthread_t demod_thread;
static pthread_cond_t ready;
static pthread_mutex_t ready_m;
static volatile int do_exit = 0;
//static rtlsdr_dev_t* dev = NULL;

//int pthread_cond_signal(pthread_cond_t* ready_m);
//int pthread_mutex_lock(pthread_cond_t* ready_m);
//int pthread_mutex_unlock(pthread_cond_t* ready_m);
//int pthread_cond_wait(pthread_cond_t* ready_n, pthread_cond_t* ready);
//
////int pthread_mutex_lock(&ready_m);
//
////pthread_mutex_init(&mutex, NULL);
////
//int pthread_mutex_lock(pthread_mutex_t* mutex);
//int pthread_mutex_trylock(pthread_mutex_t* mutex);
//int pthread_mutex_unlock(pthread_mutex_t* mutex);

pthread_t tid[2];
int counter;
pthread_mutex_t lock;

void* trythis(void* arg)
{
	pthread_mutex_lock(&lock);

	unsigned long i = 0;
	counter += 1;
	printf("\n Job %d has started\n", counter);

	for (i = 0; i < (0xFFFFFFFF); i++)
		;

	printf("\n Job %d has finished\n", counter);

	pthread_mutex_unlock(&lock);

	return NULL;
}

uint16_t squares[256];

/* todo, bundle these up in a struct */
uint8_t* buffer;  /* also abused for uint16_t */
int verbose_output = 0;
int short_output = 0;
int quality = 10;
int allowed_errors = 5;
FILE* file;
int adsb_frame[14];
#define preamble_len		16
#define long_frame		112
#define short_frame		56

/* сигналы по умолчанию не потокобезопасны*/
#define safe_cond_signal(n, m) pthread_mutex_lock(m); pthread_cond_signal(n); pthread_mutex_unlock(m)
#define safe_cond_wait(n, m) pthread_mutex_lock(m); pthread_cond_wait(n, m); pthread_mutex_unlock(m)

//================================== dump1090 ======================================================
#include "dump1090.h"

struct stModes Modes; struct stDF tDF;
//
// ============================= Utility functions ==========================
//
void sigintHandler(int dummy) 
{
	MODES_NOTUSED(dummy);
	signal(SIGINT, SIG_DFL);  // reset signal handler - bit extra safety
	Modes.exit = 1;           // Signal to threads that we are done
}

//#ifndef _WIN32
//// Get the number of rows after the terminal changes size.
//int getTermRows() 
//{
//	struct winsize w;
//	ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
//	return (w.ws_row);
//}
//
//// Handle resizing terminal
//void sigWinchCallback() 
//{
//	signal(SIGWINCH, SIG_IGN);
//	Modes.interactive_rows = getTermRows();
//	interactiveShowData();
//	signal(SIGWINCH, sigWinchCallback);
//}
//#else 
int getTermRows() { return MODES_INTERACTIVE_ROWS; }
//#endif
//
// =============================== Initialization ===========================
//
void modesInitConfig(void) {
	// Default everything to zero/NULL
	memset(&Modes, 0, sizeof(Modes));

	// Now initialise things that should not be 0/NULL to their defaults
	Modes.gain = MODES_MAX_GAIN;
	Modes.freq = MODES_DEFAULT_FREQ;
	Modes.ppm_error = MODES_DEFAULT_PPM;
	Modes.check_crc = 1;
	Modes.net_heartbeat_rate = MODES_NET_HEARTBEAT_RATE;
	Modes.net_output_sbs_port = MODES_NET_OUTPUT_SBS_PORT;
	Modes.net_output_raw_port = MODES_NET_OUTPUT_RAW_PORT;
	Modes.net_input_raw_port = MODES_NET_INPUT_RAW_PORT;
	Modes.net_output_beast_port = MODES_NET_OUTPUT_BEAST_PORT;
	Modes.net_input_beast_port = MODES_NET_INPUT_BEAST_PORT;
	Modes.net_http_port = MODES_NET_HTTP_PORT;
	Modes.interactive_rows = getTermRows();
	Modes.interactive_delete_ttl = MODES_INTERACTIVE_DELETE_TTL;
	Modes.interactive_display_ttl = MODES_INTERACTIVE_DISPLAY_TTL;
	Modes.fUserLat = MODES_USER_LATITUDE_DFLT;
	Modes.fUserLon = MODES_USER_LONGITUDE_DFLT;
}
//
//=========================================================================
//
void modesInit(void) 
{
	int i, q;

	pthread_mutex_init(&Modes.pDF_mutex, NULL);
	pthread_mutex_init(&Modes.data_mutex, NULL);
	pthread_cond_init(&Modes.data_cond, NULL);

	// Allocate the various buffers used by Modes
	if (((Modes.icao_cache = (uint32_t*)malloc(sizeof(uint32_t) * MODES_ICAO_CACHE_LEN * 2)) == NULL) ||
		((Modes.pFileData = (uint16_t*)malloc(MODES_ASYNC_BUF_SIZE)) == NULL) ||
		((Modes.magnitude = (uint16_t*)malloc(MODES_ASYNC_BUF_SIZE + MODES_PREAMBLE_SIZE + MODES_LONG_MSG_SIZE)) == NULL) ||
		((Modes.maglut = (uint16_t*)malloc(sizeof(uint16_t) * 256 * 256)) == NULL) ||
		((Modes.beastOut = (char*)malloc(MODES_RAWOUT_BUF_SIZE)) == NULL) ||
		((Modes.rawOut = (char*)malloc(MODES_RAWOUT_BUF_SIZE)) == NULL))
	{
		fprintf(stderr, "Out of memory allocating data buffer.\n");
		exit(1);
	}

	// Clear the buffers that have just been allocated, just in-case
	memset(Modes.icao_cache, 0, sizeof(uint32_t) * MODES_ICAO_CACHE_LEN * 2);
	memset(Modes.pFileData, 127, MODES_ASYNC_BUF_SIZE);
	memset(Modes.magnitude, 0, MODES_ASYNC_BUF_SIZE + MODES_PREAMBLE_SIZE + MODES_LONG_MSG_SIZE);

	// Validate the users Lat/Lon home location inputs
	if ((Modes.fUserLat > 90.0)  // Latitude must be -90 to +90
		|| (Modes.fUserLat < -90.0)  // and 
		|| (Modes.fUserLon > 360.0)  // Longitude must be -180 to +360
		|| (Modes.fUserLon < -180.0)) {
		Modes.fUserLat = Modes.fUserLon = 0.0;
	}
	else if (Modes.fUserLon > 180.0) { // If Longitude is +180 to +360, make it -180 to 0
		Modes.fUserLon -= 360.0;
	}
	// Если и широта, и долгота равны 0,0, то местоположение пользователя либо неверно/не установлено, либо он находится в
 // Атлантический океан у западного побережья Африки. Вряд ли это правильно.
 // Устанавливаем действительный флаг пользователя LatLon только в том случае, если Lat или Lon не равны нулю. Обратите внимание на Гринвичский меридиан.
 // имеет значение 0,0 Lon, поэтому мы должны проверить, что либо fLat, либо fLon не равны нулю, а не оба одновременно.
 // Проверка флага во время выполнения будет намного быстрее, чем ((fLon != 0.0) || (fLat != 0.0))
	Modes.bUserFlags &= ~MODES_USER_LATLON_VALID;
	if ((Modes.fUserLat != 0.0) || (Modes.fUserLon != 0.0)) {
		Modes.bUserFlags |= MODES_USER_LATLON_VALID;
	}

	// Ограничьте максимальный запрошенный размер необработанных выходных данных до менее одного блока Ethernet.
	if (Modes.net_output_raw_size > (MODES_RAWOUT_BUF_FLUSH))
	{
		Modes.net_output_raw_size = MODES_RAWOUT_BUF_FLUSH;
	}
	if (Modes.net_output_raw_rate > (MODES_RAWOUT_BUF_RATE))
	{
		Modes.net_output_raw_rate = MODES_RAWOUT_BUF_RATE;
	}
	if (Modes.net_sndbuf_size > (MODES_NET_SNDBUF_MAX))
	{
		Modes.net_sndbuf_size = MODES_NET_SNDBUF_MAX;
	}

	// Initialise the Block Timers to something half sensible
	ftime(&Modes.stSystemTimeBlk);
	for (i = 0; i < MODES_ASYNC_BUF_NUMBER; i++)
	{
		Modes.stSystemTimeRTL[i] = Modes.stSystemTimeBlk;
	}

	// Каждое значение I и Q варьируется от 0 до 255, что соответствует диапазону от -1 до +1. Чтобы получить от
	 // беззнаковый диапазон (0–255), поэтому вы вычитаете 127 (или 128, или 127,5) из каждого I и Q, получая
	 // диапазон от -127 до +128 (или от -128 до +127, или от -127,5 до +127,5)..
	 //
	 // Чтобы декодировать сигнал AM, вам нужна величина сигнала, которая задается sqrt((I^2)+(Q^2))
	 // Самое большее, что может быть, это если I&Q оба равны 128 (или 127, или 127,5), так что в итоге вы можете получить величину
	 // из 181.019 (или 179.605, или 180.312)
	 //
	 // Однако на самом деле величина сигнала никогда не должна превышать диапазон от -1 до +1, потому что
	 // значения I = rCos(w) и Q = rSin(w). Следовательно, вычисленная целочисленная величина никогда не должна (может?)
	 // превышаем 128 (или 127, или 127,5, или что-то еще)
	 //
	 // Если мы увеличим результаты так, чтобы они находились в диапазоне от 0 до 65535 (16 бит), нам нужно будет умножить
	 // на 511,99 (или 516,02 или 514). исходный код антиреза умножается на 360, предположительно потому, что он
	 // предполагая, что максимальная рассчитанная амплитуда равна 181,019 и (181,019 * 360) = 65166.
	 //
	 // Итак, давайте посмотрим, сможем ли мы улучшить ситуацию, вычитая 127,5. Ну, в целочисленной арифметике мы не можем
	 // вычитаем половину, то есть удвоим все и вычтем единицу, а затем компенсируем удвоение
	 // в множителе в конце.
	 //
	 // Если мы сделаем это, мы никогда не сможем получить I или Q равными 0 — они могут быть только +/- 1.
	 // Это дает нам минимальную величину корня 2 (0,707), поэтому динамический диапазон становится (1,414-255). Этот
	 // также влияет на наше значение масштабирования, которое теперь равно 65535/(255 - 1,414) или 258,433254
	 //
	 // Тогда суммы станут mag = 258,433254 * (sqrt((I*2-255)^2 + (Q*2-255)^2) - 1,414)
	 // или mag = (258.433254 * sqrt((I*2-255)^2 + (Q*2-255)^2)) - 365.4798
	 //
	 // Нам также нужно обрезать магнитную величину только потому, что любые неверные значения I/Q каким-то образом имеют величину больше 255.
	 //

	for (i = 0; i <= 255; i++) {
		for (q = 0; q <= 255; q++) {
			int mag, mag_i, mag_q;

			mag_i = (i * 2) - 255;
			mag_q = (q * 2) - 255;

			mag = (int)round((sqrt((mag_i * mag_i) + (mag_q * mag_q)) * 258.433254) - 365.4798);

			Modes.maglut[(i * 256) + q] = (uint16_t)((mag < 65535) ? mag : 65535);
		}
	}



	// Prepare error correction tables
	modesInitErrorInfo();
}
//



















//
// =============================== Terminal handling ========================
//


void usage(void)
{
	fprintf(stderr,
		"rtl_adsb, a simple ADS-B decoder\n\n"
		"Use:\trtl_adsb [-R] [-g gain] [-p ppm] [output file]\n"
		"\t[-d device_index (default: 0)]\n"
		"\t[-V verbove output (default: off)]\n"
		"\t[-S show short frames (default: off)]\n"
		"\t[-Q quality (0: no sanity checks, 0.5: half bit, 1: one bit (default), 2: two bits)]\n"
		"\t[-e allowed_errors (default: 5)]\n"
		"\t[-g tuner_gain (default: automatic)]\n"
		"\t[-p ppm_error (default: 0)]\n"
		"\t[-T enable bias-T on GPIO PIN 0 (works for rtl-sdr.com v3 dongles)]\n"
		"\tfilename (a '-' dumps samples to stdout)\n"
		"\t (omitting the filename also uses stdout)\n\n"
		"Streaming with netcat:\n"
		"\trtl_adsb | netcat -lp 8080\n"
		"\twhile true; do rtl_adsb | nc -lp 8080; done\n"
		"Streaming with socat:\n"
		"\trtl_adsb | socat -u - TCP4:sdrsharp.com:47806\n"
		"\n");
	exit(1);
}

#ifdef _WIN32
BOOL WINAPI
sighandler(int signum)
{
	if (CTRL_C_EVENT == signum) {
		fprintf(stderr, "Signal caught, exiting!\n");
		do_exit = 1;
		rtlsdr_cancel_async(dev);
		return TRUE;
	}
	return FALSE;
}
#else
static void sighandler(int signum)
{
	
	signal(SIGPIPE, SIG_IGN);
	fprintf(stderr, "Signal caught, exiting!\n");
	do_exit = 1;
	//!!rtlsdr_cancel_async(dev);
}
#endif



void display(int* frame, int len)
{
	int i, df;
	if (!short_output && len <= short_frame) {
		return;
	}
	df = (frame[0] >> 3) & 0x1f;
	if (quality == 0 && !(df == 11 || df == 17 || df == 18 || df == 19)) {
		return;
	}
	fprintf(file, "*");
	for (i = 0; i < ((len + 7) / 8); i++) {
		fprintf(file, "%02x", frame[i]);
	}
	fprintf(file, ";\r\n");
	if (!verbose_output) {
		return;
	}
	fprintf(file, "DF=%i CA=%i\n", df, frame[0] & 0x07);
	fprintf(file, "ICAO Address=%06x\n", frame[1] << 16 | frame[2] << 8 | frame[3]);
	if (len <= short_frame) {
		return;
	}
	fprintf(file, "PI=0x%06x\n", frame[11] << 16 | frame[12] << 8 | frame[13]);
	fprintf(file, "Type Code=%i S.Type/Ant.=%x\n", (frame[4] >> 3) & 0x1f, frame[4] & 0x07);
	fprintf(file, "--------------\n");
}


int abs8(int x)
/* do not subtract 127 from the raw iq, this handles it */
{
	if (x >= 127) {
		return x - 127;
	}
	return 127 - x;
}




void squares_precompute(void)
/* equiv to abs(x-128) ^ 2 */
{
	int i, j;
	// todo, check if this LUT is actually any faster
	for (i = 0; i < 256; i++) {
		j = abs8(i);
		squares[i] = (uint16_t)(j * j);
	}
}

int magnitute(uint8_t* buf, int len)
/* takes i/q, changes buf in place (16 bit), returns new len (16 bit) */
{
	int i;
	uint16_t* m;
	for (i = 0; i < len; i += 2) {
		m = (uint16_t*)(&buf[i]);
		*m = squares[buf[i]] + squares[buf[i + 1]];
	}
	return len / 2;
}


static inline uint16_t single_manchester(uint16_t a, uint16_t b, uint16_t c, uint16_t d)
/* берет 4 последовательных реальных образца, возвращает 0 или 1, BADSAMPLE в случае ошибки*/
{
	int bit, bit_p;
	bit_p = a > b;
	bit = c > d;

	if (quality == 0) {
		return bit;
	}

	if (quality == 5) {
		if (bit && bit_p && b > c) {
			return BADSAMPLE;
		}
		if (!bit && !bit_p && b < c) {
			return BADSAMPLE;
		}
		return bit;
	}

	if (quality == 10) {
		if (bit && bit_p && c > b) {
			return 1;
		}
		if (bit && !bit_p && d < b) {
			return 1;
		}
		if (!bit && bit_p && d > b) {
			return 0;
		}
		if (!bit && !bit_p && c < b) {
			return 0;
		}
		return BADSAMPLE;
	}

	if (bit && bit_p && c > b && d < a) {
		return 1;
	}
	if (bit && !bit_p && c > a && d < b) {
		return 1;
	}
	if (!bit && bit_p && c < a && d > b) {
		return 0;
	}
	if (!bit && !bit_p && c < b && d > a) {
		return 0;
	}
	return BADSAMPLE;
}


static inline uint16_t min16(uint16_t a, uint16_t b)
{
	return a < b ? a : b;
}

static inline uint16_t max16(uint16_t a, uint16_t b)
{
	return a > b ? a : b;
}

static inline int preamble(uint16_t* buf, int i)
/* returns 0/1 for preamble at index i */
{
	int i2;
	uint16_t low = 0;
	uint16_t high = 65535;
	for (i2 = 0; i2 < preamble_len; i2++) {
		switch (i2) {
		case 0:
		case 2:
		case 7:
		case 9:
			//high = min16(high, buf[i+i2]);
			high = buf[i + i2];
			break;
		default:
			//low  = max16(low,  buf[i+i2]);
			low = buf[i + i2];
			break;
		}
		if (high <= low) {
			return 0;
		}
	}
	return 1;
}



void manchester(uint16_t* buf, int len)
/* перезаписывает буфер величин допустимыми битами (BADSAMPLE при ошибках) */
{
	/* a и b сохраняют старые значения для проверки локального манчестера */
	uint16_t a = 0, b = 0;
	uint16_t bit;
	int i, i2, start, errors;
	int maximum_i = len - 1;        // len-1, так как мы смотрим на i и i+1
	 // todo, разрешить перенос между буферами
	i = 0;
	while (i < maximum_i) {
		/* find preamble */
		for (; i < (len - preamble_len); i++) {
			if (!preamble(buf, i)) {
				continue;
			}
			a = buf[i];
			b = buf[i + 1];
			for (i2 = 0; i2 < preamble_len; i2++) {
				buf[i + i2] = MESSAGEGO;
			}
			i += preamble_len;
			break;
		}
		i2 = start = i;
		errors = 0;
		/* mark bits until encoding breaks */
		for (; i < maximum_i; i += 2, i2++) {
			bit = single_manchester(a, b, buf[i], buf[i + 1]);
			a = buf[i];
			b = buf[i + 1];
			if (bit == BADSAMPLE) {
				errors += 1;
				if (errors > allowed_errors) {
					buf[i2] = BADSAMPLE;
					break;
				}
				else {
					bit = a > b;
					/* these don't have to match the bit */
					a = 0;
					b = 65535;
				}
			}
			buf[i] = buf[i + 1] = OVERWRITE;
			buf[i2] = bit;
		}
	}
}

void messages(uint16_t* buf, int len)
{
	int i, data_i, index, shift, frame_len;
	// todo, allow wrap across buffers
	for (i = 0; i < len; i++) {
		if (buf[i] > 1) {
			continue;
		}
		frame_len = long_frame;
		data_i = 0;
		for (index = 0; index < 14; index++) {
			adsb_frame[index] = 0;
		}
		for (; i < len && buf[i] <= 1 && data_i < frame_len; i++, data_i++) {
			if (buf[i]) {
				index = data_i / 8;
				shift = 7 - (data_i % 8);
				adsb_frame[index] |= (uint8_t)(1 << shift);
			}
			if (data_i == 7) {
				if (adsb_frame[0] == 0) {
					break;
				}
				if (adsb_frame[0] & 0x80) {
					frame_len = long_frame;
				}
				else {
					frame_len = short_frame;
				}
			}
		}
		if (data_i < (frame_len - 1)) {
			continue;
		}
		display(adsb_frame, frame_len);
		fflush(file);
	}
}

static void rtlsdr_callback(unsigned char* buf, uint32_t len, void* ctx)
{
	if (do_exit) {
		return;
	}
	memcpy(buffer, buf, len);
	safe_cond_signal(&ready, &ready_m);
}

static void* demod_thread_fn(void* arg)
{
	int len;
	while (!do_exit) 
	{
		safe_cond_wait(&ready, &ready_m);
		len = magnitute(buffer, DEFAULT_BUF_LENGTH);
		manchester((uint16_t*)buffer, len);
		messages((uint16_t*)buffer, len);
	}
	//rtlsdr_cancel_async(dev);
	return 0;
}


//#ifndef _WIN32
//struct sigaction sigact;
//#endif
char* filename = NULL;
int r, opt;
int gain = AUTO_GAIN; /* tenths of a dB */
int dev_index = 0;
int dev_given = 0;
int ppm_error = 0;
int enable_biastee = 0;


void setup() 
{
	//!!pthread_cond_init(&ready, NULL);
	//!!pthread_mutex_init(&ready_m, NULL);
	squares_precompute();

}

void loop() 
{
	//while ((opt = getopt(argc, argv, "d:g:p:e:Q:VST")) != -1)
	//{
	//	switch (opt) {
	//	case 'd':
	//		dev_index = verbose_device_search(optarg);
	//		dev_given = 1;
	//		break;
	//	case 'g':
	//		gain = (int)(atof(optarg) * 10);
	//		break;
	//	case 'p':
	//		ppm_error = atoi(optarg);
	//		break;
	//	case 'V':
	//		verbose_output = 1;
	//		break;
	//	case 'S':
	//		short_output = 1;
	//		break;
	//	case 'e':
	//		allowed_errors = atoi(optarg);
	//		break;
	//	case 'Q':
	//		quality = (int)(atof(optarg) * 10);
	//		break;
	//	case 'T':
	//		enable_biastee = 1;
	//		break;
	//	default:
	//		usage();
	//		return 0;
	//	}
	//}

	//if (argc <= optind) {
	//	filename = "-";
	//}
	//else {
	//	filename = argv[optind];
	//}

//!!	buffer = malloc(DEFAULT_BUF_LENGTH * sizeof(uint8_t));

	if (!dev_given) 
	{
		//!!dev_index = verbose_device_search("0");
	}

	if (dev_index < 0) 
	{
		exit(1);
	}

	//!!r = rtlsdr_open(&dev, (uint32_t)dev_index);
	if (r < 0) 
	{
		fprintf(stderr, "Failed to open rtlsdr device #%d.\n", dev_index);
		exit(1);
	}
#ifndef _WIN32
	//sigact.sa_handler = sighandler;
	//sigemptyset(&sigact.sa_mask);
	//sigact.sa_flags = 0;
	//sigaction(SIGINT, &sigact, NULL);
	//sigaction(SIGTERM, &sigact, NULL);
	//sigaction(SIGQUIT, &sigact, NULL);
	//sigaction(SIGPIPE, &sigact, NULL);
#else
	SetConsoleCtrlHandler((PHANDLER_ROUTINE)sighandler, TRUE);
#endif

	if (strcmp(filename, "-") == 0) { /* Write samples to stdout */
		file = stdout;
		setvbuf(stdout, NULL, _IONBF, 0);
#ifdef _WIN32
		_setmode(_fileno(file), _O_BINARY);
#endif
	}
	else {
		file = fopen(filename, "wb");
		if (!file) {
			fprintf(stderr, "Failed to open %s\n", filename);
			exit(1);
		}
	}

	/* Set the tuner gain */
	if (gain == AUTO_GAIN) 
	{
		//!!verbose_auto_gain(dev);
	}
	else 
	{
		//!!gain = nearest_gain(dev, gain);
		//!!verbose_gain_set(dev, gain);
	}

	//!!verbose_ppm_set(dev, ppm_error);
	//!!r = rtlsdr_set_agc_mode(dev, 1);

	/* Set the tuner frequency */
	//!!verbose_set_frequency(dev, ADSB_FREQ);

	/* Set the sample rate */
	//!!verbose_set_sample_rate(dev, ADSB_RATE);

	//!!rtlsdr_set_bias_tee(dev, enable_biastee);
	if (enable_biastee)
		fprintf(stderr, "activated bias-T on GPIO PIN 0\n");

	/* Сбросить конечную точку, прежде чем начать чтение с нее (обязательно)*/
	//!!verbose_reset_buffer(dev);

	//!!pthread_create(&demod_thread, NULL, demod_thread_fn, (void*)(NULL));
	//!!rtlsdr_read_async(dev, rtlsdr_callback, (void*)(NULL),
	//!!	DEFAULT_ASYNC_BUF_NUMBER,
	//!!	DEFAULT_BUF_LENGTH);

	if (do_exit) {
		fprintf(stderr, "\nUser cancel, exiting...\n");
	}
	else {
		fprintf(stderr, "\nLibrary error %d, exiting...\n", r);
	}

	//!!rtlsdr_cancel_async(dev);
	//!!pthread_cancel(demod_thread);
	//!!pthread_join(demod_thread, NULL);
	//!!pthread_cond_destroy(&ready);
	//!!pthread_mutex_destroy(&ready_m);

	if (file != stdout) 
	{
		fclose(file);
	}

	//!!rtlsdr_close(dev);
	free(buffer);
	//return r >= 0 ? r : -r;

}
