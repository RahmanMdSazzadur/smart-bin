# Smart Bin

A trash bin that sorts each item into **Energy**, **Mix**, **Bio** or **Metal**. A phone camera looks at the item, a vision model on **Groq** (free plan) names the category, and the phone tells the Arduino over **Wi-Fi** which bin to spill it into.

```
web/index.html                            Phone web app
arduino/smart_bin/smart_bin.ino           The team's tested sketch (USB serial only), kept unchanged
arduino/smart_bin_wifi/smart_bin_wifi.ino Wi-Fi version: the team's code copied unchanged, plus Wi-Fi
arduino/smart_bin_wifi/arduino_secrets.h  Wi-Fi and broker login (not pushed to GitHub)
```

## How the parts talk

The phone and the Arduino never connect to each other directly. Both connect to the free public **MQTT broker** `broker.hivemq.com`. It needs no account:

```
phone app  --wss:8884-->  broker.hivemq.com  <--TLS:8883--  Arduino Uno R4 WiFi
```

The app has to be on an `https://` page because Chrome only allows the camera there. Chrome also blocks an `https://` page from talking to a plain `http://` address on the local network, such as the Arduino's. Going through the broker avoids that block. The link needs internet access.

The public broker has no passwords, so the topics start with a **secret topic name** (like `smartbin-k3x9q2m7ab`) that only your app and bin know. The app makes one at random the first time it opens.

| Topic | Direction | Messages |
|---|---|---|
| `<secret>/cmd` | phone → bin | `ENERGY`, `MIX`, `BIO`, `METAL`, `turn`, `spill` |
| `<secret>/status` | bin → phone | `BUSY`, `DONE` |
| `<secret>/online` | bin → phone | `ONLINE` / `OFFLINE` (retained, so the app sees it as soon as it connects) |

A free public broker comes with no uptime guarantee. If it is down, `broker.emqx.io` works the same way: enter `wss://broker.emqx.io:8084/mqtt` as the broker host in the app, and `broker.emqx.io` in `arduino_secrets.h`.

## 1. Wire the hardware

| Pin     | Part                                                      |
|---------|-----------------------------------------------------------|
| D6      | Base servo (continuous rotation), turns the carousel      |
| D7, D5  | Joint servos 1 and 2 (continuous rotation), tilt to spill |
| D10, D9 | HC-SR04 homing sensor (TRIG, ECHO)                        |

**Power:** run the servos from a separate 6 V battery. Connect the battery's ground to the Arduino GND. Never power servos from the Arduino 5V pin.

## 2. Get the secret topic name

1. Open the app (step 5) once in Chrome on the phone.
2. Open the hidden setup page (see step 5). The **Secret topic name** field already holds a random name such as `smartbin-k3x9q2m7ab`.
3. Write it down exactly. You need it for `arduino_secrets.h`.

## 3. Upload the Wi-Fi sketch

1. In the Arduino IDE, install the **Arduino UNO R4 Boards** package and the **ArduinoMqttClient** library. `WiFiS3` comes with the board package.
2. Open `arduino/smart_bin_wifi/smart_bin_wifi.ino`, then fill in `arduino_secrets.h` (it opens as a tab): Wi-Fi name and password, and the secret topic name from step 2. Leave the broker user and password empty. The board only joins **2.4 GHz** Wi-Fi.
3. Set the bin order in the sketch: `TURNS[4] = {0, 1, 2, 3}` is how many turns from home reach Energy, Mix, Bio and Metal. Change it to match your carousel.
4. Upload. In the Serial Monitor at 9600 baud you should see the base home, then `Wi-Fi connected` and `Broker connected, bin online`.
   If Wi-Fi fails, update the board's connectivity firmware (Tools → Firmware Updater).

`turn` and `spill` still work from the Serial Monitor as before.

**What a sort does:** the bin sends `BUSY`, turns `TURNS[category]` sections, runs `spill_trash()`, keeps turning the same way back to home, then sends `DONE`.

**Drift:** like the original sketch, it only homes once, at power-up. If the bins drift out of line after many sorts, press reset on the Arduino.

## 4. Host the web app on GitHub Pages

1. Push this folder to a GitHub repository. `.gitignore` keeps `arduino_secrets.h` out of it.
2. In the repo, open **Settings → Pages**, set **Source** to "Deploy from a branch", choose `main` and `/ (root)`, and save.
3. After a minute the app is at `https://<user>.github.io/<repo>/web/`.

## 5. Open it on the phone

1. Open the URL in **Safari on iPhone** (iOS 16.4 or newer) or **Chrome on Android**, and allow the camera when asked.
2. Recommended on iPhone: **Share → Add to Home Screen**, then always open it from that icon. Safari can erase saved settings for websites you haven't opened for 7 days, but not for Home Screen apps. On Android: Chrome menu → **Add to Home screen**.
3. Open the **hidden setup page**: add `#setup` to the end of the address, or **tap the big result text (—) 5 times quickly**. The tap works in a Home Screen app, which has no address bar. Enter:
   - a **Groq API key**: free at [console.groq.com](https://console.groq.com) → **API Keys** → **Create API Key** (starts with `gsk_`)
   - the **Secret topic name**, matching `arduino_secrets.h`

   Tap **Done**. These are stored only on that phone, never in the public code.
4. Tap **Start camera**, point it at the empty platform and tap **Set empty platform**.
5. Tap **Connect bin**. The pill goes to **Bin online** when both the app and the Arduino are connected. The app reconnects by itself after drops and when it is reopened.

Place an item. When it has been still for about a second, the app classifies it, shows the category, sends it to the bin and waits for `DONE`. The app still classifies and shows results with no bin connected. If the result is **UNSURE** or **ERROR**, tap a manual sort button. **Turn one section** and **Spill** send the raw commands for testing.

### Fixed settings

Models, Live mode, sensitivity and the broker host are fixed in the `CONFIG` block near the top of the `<script>` in `web/index.html`. Nobody can change them from the page. To change one, edit `CONFIG` and push to GitHub.

- **Sensitivity:** the line under the result shows `change X / threshold`. Set `sensitivity` a little above the `change` value you see with an empty platform.
- **Photo model:** `photoModel` is the Groq vision model. On the free plan, Groq allows roughly 3–4 items a minute; when the limit is hit, the app waits and tries again.
- **Live mode:** needs a Gemini API key on the setup page. Set `liveMode: true` to use a streaming Gemini Live session instead of single photos. If it gives no answer within 8 seconds, or the socket is closed, the app falls back to photo mode. Check that `liveModel` is one your key can use.

## Test plan

1. **Sketch only:** in the Serial Monitor at 9600 baud, check for `Broker connected, bin online`, then type `turn` and `spill`.
2. **Broker only:** open the [HiveMQ WebSocket client](https://www.hivemq.com/demos/websocket-client/) and click **Connect**. Subscribe to `<secret>/#`, publish `turn` to `<secret>/cmd`, and watch `BUSY`/`DONE` arrive.
3. **Web app only:** start the camera, set the empty platform, place an item and check that the category appears.
4. **Together:** tap **Connect bin** and wait for **Bin online**. Tap each category button and check that the right bin lines up, the carousel ends back at home, and `DONE` shows in the log. Then place an item and let the app sort it.
5. **Live mode:** switch it on and check for `Live session started` in the log. Place an item and check that the tool call sorts it. Then turn off Wi-Fi briefly (or wait for a socket close) and check that photo mode takes over.
