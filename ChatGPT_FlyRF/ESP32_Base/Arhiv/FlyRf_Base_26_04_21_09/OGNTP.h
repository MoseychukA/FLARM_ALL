/*
  Модуль OGNTP.h
  Назначение:
  - Константы и интерфейс работы с протоколом OGNTP.

  Что содержит файл:
  - Параметры синхрослова, CRC, размера пакета и интервала передачи.
  - Объявления функций кодирования и декодирования пакетов OGNTP.
*/


#ifndef PROTOCOL_OGNTP_H
#define PROTOCOL_OGNTP_H

#define OGNTP_PREAMBLE_TYPE   RF_PREAMBLE_TYPE_AA
#define OGNTP_PREAMBLE_SIZE   1 /* Warmup: 6 bits, preamble: 8 bits, value:  0xAA */
/* IEEE  Manchester(0AF3656C) = AA 66 55 A5 96 99 96 5A */
#define OGNTP_SYNCWORD        {0xAA, 0x66, 0x55, 0xA5, 0x96, 0x99, 0x96, 0x5A}
#define OGNTP_SYNCWORD_SIZE   8
#define OGNTP_PAYLOAD_SIZE    20
#define OGNTP_CRC_TYPE        RF_CHECKSUM_TYPE_GALLAGER
#define OGNTP_CRC_SIZE        6

#define OGNTP_AIR_TIME        6 /* in ms */

#define OGNTP_TX_INTERVAL_MIN 600 /* in ms */
#define OGNTP_TX_INTERVAL_MAX 1400

#include "ogn.h"
#include "Container.h"
typedef struct {

  /* Dummy type definition. Actual Tx/Rx packet format is defined in ogn.h */

} ogntp_packet_t;

extern const rf_proto_desc_t ogntp_proto_desc;

bool ogntp_decode(void *, ufo_t *, ufo_t *);
size_t ogntp_encode(void *, ufo_t *);

#endif /* PROTOCOL_OGNTP_H */
