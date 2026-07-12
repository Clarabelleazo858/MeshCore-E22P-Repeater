# XIAO ESP32S3 to E22P-868M30S wiring

Target module: **[Ebyte E22P-868M30S](https://www.cdebyte.com/products/E22P-868M30S)**.

## Power

- Supply E22P VCC from a stable external rail close to 5 V.
- The module can draw approximately 600-670 mA during 30 dBm transmission.
- Do not use the XIAO 3V3 pin as the E22P power source.
- Connect battery, supply, XIAO and E22P grounds together.
- Place 100 nF, 1-10 uF and suitable bulk capacitance close to E22P VCC/GND.
- Never transmit without a suitable 868 MHz antenna.

## Signals

| E22P signal | XIAO pin | ESP32S3 GPIO | Firmware setting |
| --- | --- | ---: | --- |
| SCK | D8 | 7 | `P_LORA_SCLK=7` |
| MISO | D9 | 8 | `P_LORA_MISO=8` |
| MOSI | D10 | 9 | `P_LORA_MOSI=9` |
| NSS | D4 | 5 | `P_LORA_NSS=5` |
| DIO1 | D1 | 2 | `P_LORA_DIO_1=2` |
| BUSY | D3 | 4 | `P_LORA_BUSY=4` |
| NRST | D2 | 3 | `P_LORA_RESET=3` |
| EN | D5 / SCL | 6 | `P_LORA_EN=6` |
| T/R CTRL | SX1262 DIO2 | - | `SX126X_DIO2_AS_RF_SWITCH=true` |

E22P EN is active high and is asserted by the firmware before radio
initialization. For the 868 MHz module, connect T/R CTRL to DIO2 as shown in
the Ebyte documentation; separate MCU TXEN/RXEN pins are not used.
