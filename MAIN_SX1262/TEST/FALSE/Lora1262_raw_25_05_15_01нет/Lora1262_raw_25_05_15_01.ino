/*******************************************************************************
 * Copyright (c) 2015 Matthijs Kooijman
 * Copyright (c) 2018 Terry Moore, MCCI Corporation
 *
 * Permission is hereby granted, free of charge, to anyone
 * obtaining a copy of this document and accompanying files,
 * to do whatever they want with them without any restriction,
 * including, but not limited to, copying, modification and redistribution.
 * NO WARRANTY OF ANY KIND IS PROVIDED.
 *
 * This example transmits data on hardcoded channel and receives data
 * when not transmitting. Running this sketch on two nodes should allow
 * them to communicate.
 *******************************************************************************/

#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>

#define ledPin 4

#define LMIC_UNUSED_PIN 255
#define LORA_SS_PIN 46
#define LORA_RESET_PIN 7
#define LORA_DIO0_PIN 3
#define LORA_BYP_PIN 14
#define LORA_EN_PIN 17
#define LORA_BUSY_PIN 18
#define LORA_DIO1_PIN 1
#define LORA_DIO2_PIN 2

//#define CFG_eu868 1
//#define CFG_sx1262_radio 1


#define TX_INTERVAL 2000

// Pin mapping
const lmic_pinmap lmic_pins = {
    .nss = 46,
    .rxtx = LMIC_UNUSED_PIN,
    .rst = 7,
    .dio = {1, LMIC_UNUSED_PIN, LMIC_UNUSED_PIN},
};


// Эти обратные вызовы используются только при активации по беспроводной сети, поэтому они
// здесь оставлены пустыми (мы не можем полностью их исключить, если только
// DISABLE_JOIN не установлен в arduino-lmoc/project_config/lmic_project_config.h,
// в противном случае компоновщик выдаст сообщение об ошибке).
void os_getArtEui (u1_t* buf) { }
void os_getDevEui (u1_t* buf) { }
void os_getDevKey (u1_t* buf) { }

void onEvent (ev_t ev) 
{
}

osjob_t txjob;
osjob_t timeoutjob;
static void tx_func (osjob_t* job);

// Transmit the given string and call the given function afterwards
void tx(const char *str, osjobcb_t func) 
{
  digitalWrite(ledPin, LOW); // on
  os_radio(RADIO_RST); // Stop RX first
  delay(1); // Wait a bit, without this os_radio below asserts, apparently because the state hasn't changed yet
  LMIC.dataLen = 0;
  while (*str)
    LMIC.frame[LMIC.dataLen++] = *str++;
  LMIC.osjob.func = func;
  os_radio(RADIO_TX);
  Serial.println("TX");
  digitalWrite(ledPin, HIGH); // on
}

// Enable rx mode and call func when a packet is received
void rx(osjobcb_t func) 
{
  LMIC.osjob.func = func;
  LMIC.rxtime = os_getTime(); // RX _now_
  // Enable "continuous" RX (e.g. without a timeout, still stops after
  // receiving a packet)
  os_radio(RADIO_RXON);
  Serial.println("RX");
}

static void rxtimeout_func(osjob_t *job) 
{
  digitalWrite(ledPin, HIGH); // off
}

static void rx_func (osjob_t* job) 
{
  // Blink once to confirm reception and then keep the led on
  digitalWrite(ledPin, HIGH); // off
  delay(10);
  digitalWrite(ledPin, LOW); // on

  // Timeout RX (i.e. update led status) after 3 periods without RX
  os_setTimedCallback(&timeoutjob, os_getTime() + ms2osticks(3*TX_INTERVAL), rxtimeout_func);

  // Reschedule TX so that it should not collide with the other side's
  // next TX
  os_setTimedCallback(&txjob, os_getTime() + ms2osticks(TX_INTERVAL/2), tx_func);

  Serial.print("Got ");
  Serial.print(LMIC.dataLen);
  Serial.println(" bytes");
  Serial.write(LMIC.frame, LMIC.dataLen);
  Serial.println();

  // Restart RX
  rx(rx_func);
}

static void txdone_func (osjob_t* job) 
{
  rx(rx_func);
}

// log text to USART and toggle LED
static void tx_func (osjob_t* job) 
{
  // say hello
  tx("Hello, world!", txdone_func);
  // reschedule job every TX_INTERVAL (plus a bit of random to prevent
  // systematic collisions), unless packets are received, then rx_func
  // will reschedule at half this time.
  os_setTimedCallback(job, os_getTime() + ms2osticks(TX_INTERVAL + random(500)), tx_func);
}

// application entry point
void setup() 
{
  Serial.begin(115200);
  Serial.println("Starting");
  
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, HIGH);

  pinMode(LORA_BYP_PIN, OUTPUT);
  digitalWrite(LORA_BYP_PIN, HIGH);

  pinMode(LORA_EN_PIN, OUTPUT);
  digitalWrite(LORA_EN_PIN, HIGH);


  // инициализировать среду выполнения
  os_init();

#if defined(CFG_eu868)
  // Use a frequency in the g3 which allows 10% duty cycling.
  LMIC.freq = 868800000;
  // Use a medium spread factor. This can be increased up to SF12 for
  // better range, but then, the interval should be (significantly)
  // raised to comply with duty cycle limits as well.
  LMIC.datarate = DR_SF7;
  // Maximum TX power
  LMIC.txpow = 27;

#else
//# error Unsupported LMIC regional configuration.
#endif


  // disable RX IQ inversion
  LMIC.noRXIQinversion = true;

  // This sets CR 4/5, BW125 (except for EU/AS923 DR_SF7B, which uses BW250)
  LMIC.rps = updr2rps(LMIC.datarate);

  Serial.print("Frequency: "); Serial.print(LMIC.freq / 1000000);
  Serial.print("."); Serial.print((LMIC.freq / 100000) % 10);
  Serial.print("MHz");
  Serial.print("  LMIC.datarate: "); Serial.print(LMIC.datarate);
  Serial.print("  LMIC.txpow: "); Serial.println(LMIC.txpow);

  // This sets CR 4/5, BW125 (except for DR_SF7B, which uses BW250)
  LMIC.rps = updr2rps(LMIC.datarate);

  // disable RX IQ inversion
  LMIC.noRXIQinversion = true;

  Serial.println("Started");
  Serial.flush();

  // setup initial job
  os_setCallback(&txjob, tx_func);
}

void loop() {
  // execute scheduled jobs and events
  os_runloop_once();
}
