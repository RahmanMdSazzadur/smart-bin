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

The public broker has no passwords, so the topics start with a topic name (`topicPrefix` in `CONFIG` in `web/index.html`). The same name goes in `arduino_secrets.h`.

**Built-in secrets:** the Groq API key and the topic name are written into `CONFIG` so every device works without setup. The page is public, so anyone can read them: they could use up the free Groq limits or send commands to the bin. If Groq disables the key, create a new one and replace `groqKey` in `CONFIG`.

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

## 2. Check the topic name

Copy `topicPrefix` from `CONFIG` in `web/index.html` (for example `smartbin-b0p1r0mw5c`). You need it for `arduino_secrets.h`.

## 3. Upload the Wi-Fi sketch

1. In the Arduino IDE, install the **Arduino UNO R4 Boards** package and the **ArduinoMqttClient** library. `WiFiS3` comes with the board package.
2. Open `arduino/smart_bin_wifi/smart_bin_wifi.ino`, then fill in `arduino_secrets.h` (it opens as a tab): Wi-Fi name and password, and the topic name from step 2. Leave the broker user and password empty. The board only joins **2.4 GHz** Wi-Fi.
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
3. Nothing to enter: the Groq key and topic name are built in, and the app connects to the bin by itself. An optional hidden setup page (add `#setup` to the address, or tap the big result text 5 times quickly) holds a Gemini key for Live mode and broker login details.

4. Tap **Start camera**, point it at the empty platform and tap **Set empty platform**.
5. Tap **Connect bin**. The pill goes to **Bin online** when both the app and the Arduino are connected. The app reconnects by itself after drops and when it is reopened.

Place an item. When it has been still for about a second, the app classifies it, shows the category, sends it to the bin and waits for `DONE`. The app still classifies and shows results with no bin connected. If the result is **UNSURE** or **ERROR**, tap a manual sort button. **Turn one section** and **Spill** send the raw commands for testing.

### Fixed settings

Models, Live mode, sensitivity and the broker host are fixed in the `CONFIG` block near the top of the `<script>` in `web/index.html`. Nobody can change them from the page. To change one, edit `CONFIG` and push to GitHub.

- **Sensitivity:** the line under the result shows `change X / threshold`. Set `sensitivity` a little above the `change` value you see with an empty platform.
- **AI models:** every photo goes to `geminiModel` (Gemini, free tier) first. When Gemini is out of free quota or fails, the app switches to `photoModel` on Groq by itself and tries Gemini again once Google's wait time is over. With `geminiKey` empty, only Groq is used.
- **Groq limit:** `photoModel` is the Groq vision model. On the free plan, Groq allows roughly 3–4 items a minute; when the limit is hit, the app waits and tries again.
- **Live mode:** needs a Gemini API key on the setup page. Set `liveMode: true` to use a streaming Gemini Live session instead of single photos. If it gives no answer within 8 seconds, or the socket is closed, the app falls back to photo mode. Check that `liveModel` is one your key can use.

## Customer dashboard

`web/dashboard.html` (`https://<user>.github.io/smart-bin/web/dashboard.html`) is a read-only screen for customers, on any phone, tablet or monitor. It shows whether the bin is online, the last item, an estimated fill level and today's count for each compartment, and a message feed: items sorted, items that need emptying first, items that need manual sorting, and compartments that are almost full or full.

The camera app (`index.html`) publishes this as a retained JSON message on `<topic>/state`, so a dashboard opened later still shows the latest numbers. The bin has no fill sensor, so fill levels count items: `binCapacity` in `CONFIG` (25 by default) is how many items make a compartment full. After emptying a compartment, tap its button under **Bin emptied** in the camera app.

## Automatic updates

Phones can keep an old copy of the app for a while. Each page has an `APP_VERSION`, and `web/version.json` holds the latest one. The app checks `version.json` when it opens, when it comes back to the screen, and every 5 minutes; when GitHub has a newer version it reloads itself (never in the middle of a sort, and it restarts the camera if it was running).

The version is stamped automatically by `.githooks/pre-commit` whenever files in `web/` are committed. Turn the hook on once in each copy of the repository:

```
git config core.hooksPath .githooks
```

## Test plan

1. **Sketch only:** in the Serial Monitor at 9600 baud, check for `Broker connected, bin online`, then type `turn` and `spill`.
2. **Broker only:** open the [HiveMQ WebSocket client](https://www.hivemq.com/demos/websocket-client/) and click **Connect**. Subscribe to `<secret>/#`, publish `turn` to `<secret>/cmd`, and watch `BUSY`/`DONE` arrive.
3. **Web app only:** start the camera, set the empty platform, place an item and check that the category appears.
4. **Together:** tap **Connect bin** and wait for **Bin online**. Tap each category button and check that the right bin lines up, the carousel ends back at home, and `DONE` shows in the log. Then place an item and let the app sort it.
5. **Live mode:** switch it on and check for `Live session started` in the log. Place an item and check that the tool call sorts it. Then turn off Wi-Fi briefly (or wait for a socket close) and check that photo mode takes over.
