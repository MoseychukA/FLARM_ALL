# Basic MAC

Basic MAC is a portable implementation of the LoRa™ Alliance's LoRaWAN™
specification in the C programming language. It is a fork of IBM's LMiC
library, and supports multiple regions, which are selectable at compile and/or
run time. It can handle Class A, Class B, and Class C devices.

## This repository
This repository contains an Arduino port of the BasicMAC library.

* Original repository [lorabasics](https://github.com/lorabasics/basicmac)
* Arduino port repository [LacunaSpace](https://github.com/LacunaSpace/basicmac)

### Development status

This branch is still under heavy development and should be considered an
early testing version. Some commits are unfinished or hacky (in
particular the debug printing support is a bit of a mess in an attempt
to reduce RAM usage on AVR).

There will likely be some history editing and rebasing in the future.
Also, the API is likely to change in a breaking way.

However, the library should actually work as it is now.

### Hardware support

This port is intended to work on any Arduino board, regardless of
architecture. It was tested on AVR, SAMD and STM32 boards, and with
SX1272, SX1276 and SX1262 radios.

Unfortunately, BasicMAC is quite a bit bigger than LMIC, so it seems it
does not fit in an atmega328p anymore (maybe some more optimization can
be done, but now the ttn-otaa example compiles down to 35k of flash,
with only 32k available).
