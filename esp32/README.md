# TinyTrackSpotter (ESP32)

TinyTrackSpotter is intended to show the currently playing Spotify track (or podcast episode) on a LILYGO T-Display S3 AMOLED device. The display startup sequence has been observed working on the device. This README records that baseline and the next recovery steps; Spotify playback is not yet verified end to end.

## Current state

- **Firmware:** PlatformIO, Arduino framework, ESP32-S3, configured as `lilygo-t-display-s3` in `platformio.ini`.
- **Display:** Confirmed by the user as RM67162 QSPI at 536x240. The visible startup sequence has been observed working. A project-local driver is in `lib/rm67162`; `lib/Display` combines it with TFT_eSPI sprites and TJpg_Decoder; `lib/Screan` (spelling retained from source) renders track details and progress.
- **Network and web UI:** Wi-Fi connects to a network configured in `src/main.cpp`. An async HTTP server serves files from LittleFS and exposes `/settings` for Spotify credentials. The Vue 3/Quasar web app lives in the sibling `../web/TinyTrackSpotterUI` directory; `data/` contains what appears to be a built web bundle copied for LittleFS.
- **Spotify:** `lib/Spotify` polls Spotify's currently-playing endpoint, parses song/episode details, downloads cover art and contains a refresh-token flow. Credentials are stored as LittleFS files.
- **Dependencies:** `platformio.ini` lists unpinned/latest-resolved dependencies for ESP Async WebServer and ArduinoJson, and version ranges for TFT_eSPI, TFT_eFEX and JPEGDecoder, plus TJpg_Decoder and HTTPClient. The web manifest declares Vue 3, Quasar 2 and Vite-based Quasar tooling with broad version ranges. These are not locked as a reproducible firmware/web baseline.
- **Build/deployment:** LittleFS is configured as the firmware filesystem. The repository does not currently document a verified build, upload, filesystem upload, Spotify OAuth setup, or hardware test procedure.

## Findings to address

1. **Record the display baseline.** The confirmed panel is RM67162 QSPI at 536x240, with landscape rotation set by `lcd_setRotation(1)`. QSPI pins and transfer settings are in `lib/rm67162/pins_config.h`; the PlatformIO environment is `lilygo-t-display-s3`. The startup screen has been seen working, so retain this as the baseline when making display changes.
2. **The first screen clear can fail silently or crash.** `displayClear()` allocates a full 536x240 TFT_eSprite (about 257 KiB for RGB565), does not check whether allocation succeeded, then passes the pointer to the custom driver. A failed allocation can explain a black screen or invalid memory access. Smaller sprites in text/rectangle functions also need allocation checks. The clear uses a full framebuffer where bounded row/tile writes or a native display API would be more robust.
3. **Display initialization is difficult to diagnose.** `initDisplay()` wraps C++ calls in `try/catch`, but most embedded driver/allocation failures are not C++ exceptions. Keep serial diagnostics for each startup stage and verify the existing startup screen after display changes.
4. **Spotify initialization is not connected.** `initSpotify()` is commented out in `setup()` (and no implementation appears in the inspected declarations), yet startup logs "Initialized Spotify" based on the previous web-server result. Polling does run in `loop()`, but only after all setup stages complete.
5. **Spotify HTTP behavior is unreliable.** The code handles status 200/204/401 inconsistently: 204 means no playback but leaves prior song data in place; non-200 errors can be treated as success; on authorization errors it refreshes but may not retry immediately. `downloadImage()` has a path with no return value, uses response-stream availability as its termination condition, and does not propagate failure. Token request values are not URL-encoded, HTTP response data is logged, and TLS certificate validation is not configured explicitly.
6. **Wi-Fi setup can block startup indefinitely.** Addressed: the initial connection attempt is bounded to 15 seconds. If it times out, startup proceeds to the web server, reports Wi-Fi offline, and retries in the background every five seconds. Connection transitions and the acquired IP are logged on Serial; mDNS starts after connection.
7. **Credential handling needs hardening.** Wi-Fi credentials are currently embedded directly in firmware source. Spotify secrets/tokens are written as plain files in LittleFS and exposed by the generic `/settings` GET endpoint; the HTTP server has no authentication or transport security. Remove credentials from source/history where applicable, rotate any real credentials that have been committed, and avoid exposing client secrets or refresh tokens to an unauthenticated LAN client. Do not put live secrets in this README.
8. **Web artifact synchronization is unclear.** The checked-in `data/` folder contains hashed JS/CSS assets, while the firmware server explicitly requests `/index.js`, `/settings.js`, and `/style.css`; verify the generated `index.html` actually references paths the server can serve and document the command that copies/builds web output into `data/`.

## Improvement plan

### 1. Establish a reproducible baseline

- Record the confirmed RM67162 QSPI panel at 536x240, landscape rotation, and the configured pin map/clock in `lib/rm67162/pins_config.h`.
- Check the repository and its parent for existing build scripts and web artifact copy steps.
- Pin known-compatible PlatformIO platform and library versions; keep `platformio.ini` as the source of firmware dependencies. Pin the web toolchain via the existing package lockfile (or create one using the project's chosen package manager).
- Keep credentials in local, ignored configuration or provision them through a protected setup flow; make sure no secret is stored in the committed `data/` bundle.

### 2. Prove the display independently

- Reduce `setup()` temporarily to serial startup plus display init and a solid-color/corner-marker sequence.
- Verify RM67162 init, brightness, reset, QSPI transaction path and rotation against the exact hardware revision.
- Replace full-screen sprite clears with direct solid-color fills or a small bounded buffer; check allocation and bounds before every draw. Consider moving the renderer to the vendor-supported graphics stack if the custom TFT_eSPI bridge remains fragile.
- Only proceed when repeated cold boots render the test pattern reliably.

### 3. Make startup and Wi-Fi observable

- Initialize the display first and show explicit boot/error states while also logging them on serial.
- Keep the bounded initial Wi-Fi attempt and background reconnect behavior; verify offline startup still reaches the local setup UI.
- Mount LittleFS once through a clearly owned lifecycle and report mount/settings errors instead of continuing with empty credentials.

### 4. Make Spotify authentication and polling correct

- Implement and document OAuth authorization-code setup (or another supported flow), exact redirect URI, scopes, and credential provisioning.
- Use HTTPS with certificate validation where feasible, encode form values, avoid logging tokens, and protect secrets from unauthenticated settings reads.
- Treat 200 (playing data), 204 (nothing playing), 401 (refresh once then retry), rate limits, and network errors as distinct states.
- Add bounded polling/backoff, handle songs and episodes explicitly, validate JSON fields, clamp progress, and make cover-art download completion/error handling reliable.

### 5. Align the renderer with playback state

- Define a small playback-state model shared by the polling and rendering code: loading, playing, paused, no active device, no playback, authentication failure, and network failure.
- Render track title, artist/show, play state, progress and optional cover image from that model; clear stale artwork and metadata on 204 or errors.
- Handle long titles and small/missing image arrays safely.

### 6. Integrate and verify the setup web app

- Confirm the UI's credential and OAuth flow matches firmware routes and supported settings.
- Generate the web production build and copy its complete output into the LittleFS `data/` directory using a repeatable script; ensure SPA assets and routing work when served by the ESP32.
- Review dependency versions and remove unused packages only after checking actual imports.

### 7. End-to-end acceptance

- Build firmware and web UI from a clean checkout; upload firmware and filesystem separately.
- Verify serial logs and the display boot test, Wi-Fi recovery, settings page, Spotify authorization/token refresh, playing/paused/no-playback states, track changes, and cover art.
- Record tested board revision, tool versions, wiring assumptions (if any), and upload commands here.

### 8. Credential handling (deferred)

- Review credential storage, provisioning, exposure, and rotation only after the display, startup/Wi-Fi, Spotify flow, and setup UI have been verified. Credential handling is intentionally outside the current implementation scope.

## Suggested first commands

From this directory, with PlatformIO installed:

```sh
pio run -e lilygo-t-display-s3
pio run -e lilygo-t-display-s3 -t upload
pio run -e lilygo-t-display-s3 -t uploadfs
pio device monitor -b 19200
```

These are suggested commands based on the current configuration. They have not been run as part of this change. Build the web project from `../web/TinyTrackSpotterUI` with its documented package manager and establish the missing copy-to-`data/` step before filesystem upload.

## Hardware test for the current change

1. Build and upload firmware with `pio run -e lilygo-t-display-s3 -t upload`.
2. Open `pio device monitor -b 19200` and reset the board. Confirm serial output reports display, settings, Wi-Fi, and web-server stages in order, followed by `Startup complete`.
3. Confirm the known startup sequence still appears on the RM67162 536x240 display.
4. With the configured access point available, confirm Serial prints `Wi-Fi connected` and an IP address; open `http://<IP>/` from a device on the same LAN.
5. To exercise recovery, temporarily make the configured access point unavailable during boot. After the 15-second timeout, confirm the display reports `WiFi offline; retrying`, startup completes, and the web UI starts when Wi-Fi becomes available. Serial should log retry attempts, the connection transition, IP address, and mDNS start.

## Repository map

| Path | Purpose |
| --- | --- |
| `src/main.cpp` | Startup sequence and main loop |
| `lib/rm67162/` | Custom RM67162 QSPI display initialization and pixel transfer |
| `lib/Display/`, `lib/Screan/` | Drawing primitives and playback screen |
| `lib/Spotify/` | Spotify polling, token refresh and artwork download |
| `lib/Settings/` | LittleFS-backed settings and credentials |
| `lib/WiFiSetup/` | Wi-Fi connection handling |
| `lib/Web/` | Async settings server and static-file routes |
| `data/` | LittleFS web UI payload |
| `../web/TinyTrackSpotterUI/` | Vue/Quasar setup application |
