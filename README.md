# Pocket AI (ESP32-S3)

A pocket-sized, battery-powered AI assistant built around an ESP32-S3. You type a question with five buttons on a 0.96" OLED screen, the ESP32 sends it straight to Google Gemini over WiFi, the answer appears on the screen, and a copy of every conversation is posted to your Telegram chat. No laptop or server is needed after flashing.

> **Status: work in progress.** The firmware and the case were designed with AI assistance (see [Acknowledgements](#acknowledgements)). At the time of writing they have not been verified on real hardware or a real 3D printer. Treat the pin numbers and case dimensions as a starting point, and please open an issue or pull request with anything you fix.

## How it works

```
 5 buttons -> ESP32-S3 --HTTPS--> Gemini API   (the AI lives here)
                 |  <-- answer ---
                 |
                 +--> OLED screen (read the answer)
                 +--> Telegram Bot API --> your phone (conversation log)
```

A Telegram bot cannot read messages sent by itself, so the usual "device -> bot -> AI" idea does not work without a middle server. Instead, the ESP32 talks to both APIs directly: it asks Gemini, shows the reply, and then posts the question and answer to Telegram. The Telegram bot is only a log, not the AI.

## Features

- Type with five buttons: previous, next, OK, delete, send
- Hold a button to auto-scroll characters, hold delete for one second to clear the text
- Answers are word-wrapped and paged on the 128x64 OLED
- Every question and answer is logged to Telegram
- Screen turns off after 30 seconds and wakes on any button press, to save battery
- Optional IR remote support (for example, a spare fan remote)
- Parametric 3D-printable case (OpenSCAD) with a window, five button holes, USB cutouts, and a snap-fit lid

## Bill of materials

| Part | Notes |
|---|---|
| ESP32-S3 dev board | N16R8 DevKitC-style board. WiFi is 2.4 GHz only. |
| 0.96" OLED, 128x64, SSD1306 | The **6-pin SPI** version (GND, VCC, SCL, SDA, RES, DC). A 4-pin I2C version needs small code changes. |
| 5 push buttons | Tactile switches, normally open |
| IR receiver (optional) | VS1838B, TSOP1838, or a KY-022 / HX1838 module, 38 kHz, 3.3V |
| LiPo battery 3.7V, about 550 mAh | With a protection circuit. Check the connector polarity. |
| TP4056 charger module, USB-C | The version with battery protection (DW01 and FS8205 chips) |
| MT3608 boost converter | Raises 3.7V to 5V for the board's 5V pin |
| 1N5819 Schottky diode | Prevents current flowing back into the boost module or USB |
| Slide switch | Power on/off |
| Wire, solder, foam tape, Kapton tape | |
| 3D-printed case | See the `case/` folder |

## Wiring

### OLED (SPI)

| OLED pin | ESP32-S3 pin |
|---|---|
| GND | GND |
| VCC | 3V3 |
| SCL (SCK) | GPIO 12 |
| SDA (MOSI) | GPIO 11 |
| RES | GPIO 13 |
| DC | GPIO 10 |

On SPI modules the pins labelled SCL and SDA are the SPI clock and data lines, not I2C. The module's CS pin is already tied to GND, so nothing is wired to it.

### Buttons

Each button has one leg on its GPIO pin and the other leg on GND. The firmware uses the internal pull-ups, so no resistors are needed.

| Button | GPIO |
|---|---|
| Previous (<) | 4 |
| Next (>) | 5 |
| OK | 7 |
| Delete | 15 |
| Send | 16 |

### IR receiver (optional)

| IR pin | ESP32-S3 pin |
|---|---|
| OUT | GPIO 6 |
| VCC | 3V3 |
| GND | GND |

Check the pin order printed on your module, because it differs between brands.

### Power chain

```
LiPo -> TP4056 (USB-C charging) -> slide switch -> MT3608 (set to 5.0 V)
     -> 1N5819 diode (stripe toward the ESP32) -> ESP32-S3 5V pin
```

All grounds (battery, TP4056, MT3608, ESP32) must be connected together.

Before connecting the ESP32:

1. Power the MT3608 input from the battery chain only.
2. Put a multimeter on the MT3608 output and turn its small trimmer screw until it reads **5.0 V**. It takes many turns, and the direction varies between modules.
3. Only then connect the output to the ESP32.

Never connect the battery directly to the 3V3 pin. A full LiPo is 4.2 V, which is above what that pin tolerates.

The ESP32 also draws power from its own USB port when you plug it in for flashing. The diode prevents the two supplies from fighting each other, but switching the battery off while flashing is the safest habit.

## Firmware setup

### 1. Install the tools

- Arduino IDE with the **esp32 by Espressif Systems** board package
- Libraries (Library Manager):
  - Adafruit SSD1306
  - Adafruit GFX Library
  - ArduinoJson (version 7.x)
  - IRremote (version 4.x), only if you enable IR

Board settings: select **ESP32S3 Dev Module**. If you use the board's native USB port, set **USB CDC On Boot** to Enabled. Match the PSRAM and flash size options to your module (for example N16R8: 16 MB flash, OPI PSRAM).

### 2. Get your keys

**Gemini API key and model ID**

1. Open [Google AI Studio](https://aistudio.google.com) and sign in.
2. Create an API key.
3. Choose a Flash model and copy its model ID exactly as shown. You can also list the models available to your key at `https://generativelanguage.googleapis.com/v1beta/models?key=YOUR_KEY` and copy the part after `models/`.

**Telegram bot token and chat ID**

1. In Telegram, talk to @BotFather, send `/newbot`, and follow the prompts. Save the token.
2. Open your new bot and send it any message (for example "hi").
3. Visit `https://api.telegram.org/botYOUR_TOKEN/getUpdates` and copy the number after `"chat":{"id":`. If `result` is empty, you have not messaged the bot yet.

### 3. Add your credentials

```
cd firmware/pocket_ai
cp secrets.example.h secrets.h
```

Edit `secrets.h` and fill in your WiFi name and password, bot token, chat ID, Gemini key, and model ID. `secrets.h` is ignored by git. **Never commit it.**

### 4. Flash

Open `firmware/pocket_ai/pocket_ai.ino`, click Verify, then Upload. If the upload fails, hold BOOT, tap RESET, release BOOT, and try again. Open the Serial Monitor at 115200 baud to see debug output.

## Usage

| Button | Typing screen | Answer screen |
|---|---|---|
| < and > | Pick the previous or next character (hold to auto-scroll) | Previous or next page |
| OK | Add the selected character | Back to typing |
| Delete | Delete one character (hold 1 s to clear all) | Back to typing |
| Send | Send the text to Gemini | |

The character list is `a-z`, a space (shown as `_`), `0-9`, and `. , ? ! -`. Any button press wakes the screen when it has turned off; that first press only wakes it.

### Optional IR remote

Set `USE_IR` to `1` in the sketch, press each remote button, and note the `command` values printed in the Serial Monitor. Fill them into `IR_PREV`, `IR_NEXT`, `IR_OK`, `IR_DEL`, and `IR_SEND`. Some remotes use rolling codes and cannot be read this way.

## Case

The case is a parametric OpenSCAD design in `case/pocket_ai_case.scad`. With the default dimensions it is about **75 x 35 x 29 mm**.

Open the file in [OpenSCAD](https://openscad.org), edit the parameters at the top, press F6 to render, then export as STL. Set `part` to `"body"` or `"lid"` to export each piece separately.

Layers, from the front to the back:

| # | Layer | Contents |
|---|---|---|
| 1 | Front plate | OLED window, five button holes in a plus pattern |
| 2 | OLED | The display module, glass facing the window |
| 3 | ESP32-S3 | Dev board with USB cutouts in the right end wall |
| 4 | Charger and boost | TP4056 on the left (USB-C cutout on the left wall), MT3608 on top of the battery |
| 5 | Battery | LiPo cell, plus a switch cutout in the side wall |
| 6 | Back cover | Press-fit lid with two lips |

The default dimensions are estimates. **Measure your own parts** and update the values at the top of the file: the ESP32 board (`esp_l`, `esp_w`, `esp_h`), the OLED module (`oled_l`, `oled_w`, `oled_h`), the battery (`bat_l`, `bat_w`, `bat_t`), the charger (`chg_l`, `chg_w`, `chg_h`), and the boost module (`boost_l`, `boost_w`, `boost_h`). If the boost module does not fit on top of the battery, the file stops with an error so you know the case needs to grow.

Printing tips:

- Print the body with the front face on the bed, and the lid with its plate on the bed.
- Print a small test piece first to check tolerances (`tol`, `gap`, `end_gap`), because every printer is different.
- The OLED window position is estimated from product photos. Adjust `win_from_top` if it is off.
- Antennas dislike metal. The case places the charger (not the battery) under the ESP32's antenna end.
- Mount the OLED and buttons with hot glue or double-sided tape from the inside.
- Put insulating (Kapton) tape between the battery and the boost module.

## Battery life

Rough estimate only: a 550 mAh cell gives a few hours of active use. The ESP32 draws tens to a few hundred mA while WiFi is active, the OLED adds 10 to 20 mA, and the boost converter loses 10 to 20 percent. The screen timeout and reduced brightness help. A USB power meter will tell you the real number for your build.

## Safety

- Use a LiPo with a protection circuit and a TP4056 with protection.
- Check connector polarity before plugging in a battery. Plus and minus are swapped between some JST connectors.
- Do not crush, bend, pierce, or overheat the cell. Stop using a swollen battery.
- Do not charge unattended for long periods.
- Do not connect any part of this project to mains voltage.

## Troubleshooting

| Symptom | Likely cause |
|---|---|
| Serial Monitor is empty | Set USB CDC On Boot to Enabled for the native USB port |
| OLED stays blank | Check the SCL, SDA, RES, and DC wiring. Confirm the module is SPI and the chip is SSD1306 (1.3" modules often use an SH1106). |
| `JsonDocument was not declared` | ArduinoJson is version 6. Install 7.x. |
| `secrets.h: No such file` | Copy `secrets.example.h` to `secrets.h` |
| Error 400 on screen | Wrong model ID |
| Error 403 on screen | Wrong or inactive API key |
| Error 429 on screen | Free-tier quota used up, wait a while |
| `finishReason` error | The model ran out of tokens before answering. Raise `maxOutputTokens`. |
| `"result":[]` from getUpdates | You have not sent a message to your bot yet |
| WiFi will not connect | The ESP32-S3 supports 2.4 GHz only |

## Known limitations

- Typing with five buttons is slow by design. A keyboard or a voice input module could be added later.
- The OLED font cannot show emoji or accented characters. They are stripped from the screen, but the Telegram log keeps the full answer.
- HTTPS uses `setInsecure()`, so server certificates are not verified. Acceptable for a personal gadget, but consider pinning root certificates for anything more.
- Your API key and bot token live in the firmware. Do not share flashed devices or firmware images.
- No conversation memory yet. Each question is sent on its own.

## Roadmap

- Conversation history
- Two-way chat from Telegram to the device
- Voice input with an I2S microphone
- Smaller board option (for example a Seeed XIAO ESP32S3) and a thinner case

## Acknowledgements

The firmware, wiring plan, and case design were developed with assistance from Claude, an AI assistant made by Anthropic.

## License

MIT. See [LICENSE](LICENSE).

