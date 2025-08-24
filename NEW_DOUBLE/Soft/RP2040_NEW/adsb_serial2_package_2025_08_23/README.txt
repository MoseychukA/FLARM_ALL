ADS-B FlightObject Serial2 link (Sender + ESP32S3 Receiver)

Structure
- common/protocol.h: shared binary protocol (frame format, FlightObject, CRC32, sender and receiver helpers)
- adsb_sender_example/adsb_sender_example.ino: example of packing and sending a FlightObject via Serial2
- esp32s3_receiver/esp32s3_receiver.ino: ESP32S3 program receiving frames on GPIO42 (RX), GPIO41 (TX)

Wiring
- Connect Sender Serial2 TX -> ESP32S3 GPIO42 (RX)
- Connect Sender Serial2 RX -> ESP32S3 GPIO41 (TX) [optional if you add future ACK]
- Common GND between boards

Baud rate
- Default 230400. You may change SERIAL_BAUD in both sketches.

Arduino IDE notes
- Receiver: Select board "ESP32S3" (e.g., ESP32S3 Dev Module) and flash esp32s3_receiver.ino
- Sender: Integrate common/protocol.h into your ADS-B project and call protocol_send_flight(Serial2, fo) when a new packet is ready.

Integration into your ADS-B code
- Map your receivedPacket fields into FlightObject (see example). Ensure flight field is NUL-terminated.
- Initialize Serial2 at the desired baudrate in setup().
- For integrity, the frame includes magic bytes, version, length and CRC32.

Frame format
[0xAA 0x55][0x01 ver][0x01 type][len LE=49][payload FlightObject][crc32 LE]

