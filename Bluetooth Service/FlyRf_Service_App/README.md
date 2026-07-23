# FlyRF Service

Simple Android BLE control app for FlyRF Base and FlyRF external display.

Commands sent to BLE service `0xFFE0`, characteristic `0xFFE1`:

- `WIFI OFF`
- `WIFI AP`
- `WIFI ON`
- `STATUS`
- `RESTART`

Open the project in Android Studio, grant Bluetooth permissions on the phone, press `Scan`, select a `FlyRf_Base-...-BLE` or `FlyRf_Disp-...-BLE` device, then use the command buttons.
