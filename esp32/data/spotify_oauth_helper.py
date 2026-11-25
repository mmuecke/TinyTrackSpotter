"""Local Spotify OAuth callback bridge for TinyTrackSpotter.

Run this on the computer whose browser will approve Spotify access. The helper
binds only to IPv4 loopback and forwards the resulting refresh token to the ESP.
"""

from base64 import b64decode, b64encode
from html import escape
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import threading
import time
from urllib.error import HTTPError, URLError
from urllib.parse import parse_qs, urlencode, urlsplit
from urllib.request import Request, urlopen


REDIRECT_URI = "http://127.0.0.1:8888/callback"
SCOPES = "user-read-currently-playing user-read-playback-state user-modify-playback-state"
HEALTH_GIF = b64decode("R0lGODlhAQABAAD/ACwAAAAAAQABAAACADs=")
DEVICE_ASSETS = {
    "/style.css": "text/css; charset=utf-8",
    "/favicon.ico": "image/x-icon",
    "/icons/favicon-16x16.png": "image/png",
    "/icons/favicon-32x32.png": "image/png",
    "/icons/favicon-96x96.png": "image/png",
    "/icons/favicon-128x128.png": "image/png",
}


def esp_request(base_url, path, method="GET", timeout=12):
    request = Request(base_url.rstrip("/") + path, method=method)
    with urlopen(request, timeout=timeout) as response:
        return response.read().decode("utf-8")


def esp_setting(base_url, key, timeout=12):
    query = urlencode({key: ""})
    return esp_request(base_url, "/settings?" + query, timeout=timeout)


def prepare_authorization(server, esp_url, flow_id):
    deadline = time.monotonic() + 120
    last_error = ""
    while time.monotonic() < deadline:
        try:
            client_id = esp_setting(esp_url, "clientId", timeout=3).strip()
            if client_id:
                authorize_query = urlencode({
                    "client_id": client_id,
                    "response_type": "code",
                    "redirect_uri": REDIRECT_URI,
                    "scope": SCOPES,
                })
                state = {
                    "flow_id": flow_id,
                    "status": "ready",
                    "authorize_url": "https://accounts.spotify.com/authorize?" + authorize_query,
                }
                with server.login_lock:
                    if server.login_generation == flow_id:
                        server.login_state = state
                return
            last_error = "The device is reachable, but its Client ID is empty."
        except (HTTPError, URLError, TimeoutError, OSError) as error:
            last_error = str(error)
        time.sleep(2)

    state = {
        "flow_id": flow_id,
        "status": "error",
        "message": "The device is still unavailable. Check its Wi-Fi connection and try again. " + last_error,
    }
    with server.login_lock:
        if server.login_generation == flow_id:
            server.login_state = state


class Handler(BaseHTTPRequestHandler):
    server_version = "TinyTrackSpotterOAuth/1.0"

    def log_message(self, _format, *_args):
        # Do not write OAuth query parameters or credentials to the console.
        if self.path.startswith("/login/status"):
            return
        print("OAuth helper request received")

    def send_device_asset(self, path):
        esp_url = getattr(self.server, "esp_url", "")
        if not esp_url:
            self.send_error(503, "Device address is not available yet")
            return
        try:
            asset_request = Request(esp_url.rstrip("/") + path)
            with urlopen(asset_request, timeout=4) as response:
                content = response.read()
            self.send_response(200)
            self.send_header("Content-Type", DEVICE_ASSETS[path])
            self.send_header("Content-Length", str(len(content)))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(content)
        except (HTTPError, URLError, TimeoutError, OSError):
            self.send_error(503, "Device asset is temporarily unavailable")

    def send_loading_page(self, flow_id):
        device_url = escape(getattr(self.server, "esp_url", ""), quote=True)
        body = ("<!doctype html><html lang='en'><head><meta charset='utf-8'>"
                "<meta name='viewport' content='width=device-width,initial-scale=1'>"
                "<meta name='color-scheme' content='dark'><title>Connecting device | TinyTrackSpotter</title>"
                "<link rel='icon' href='/favicon.ico' sizes='any'>"
                "<link rel='icon' type='image/png' sizes='16x16' href='/icons/favicon-16x16.png'>"
                "<link rel='icon' type='image/png' sizes='32x32' href='/icons/favicon-32x32.png'>"
                "<link rel='icon' type='image/png' sizes='96x96' href='/icons/favicon-96x96.png'>"
                "<link rel='icon' type='image/png' sizes='128x128' href='/icons/favicon-128x128.png'>"
                "<link id='shared-style' rel='stylesheet' href='/style.css'>"
                "<style>body{margin:0;background:#111714;color:#f3f7f4;font:16px system-ui}"
                ".setup-page{min-height:100vh;display:grid;place-items:center;padding:24px}"
                ".setup-card{width:min(100%,480px);padding:32px;border:1px solid #34483a;border-radius:20px;background:#17201a}"
                ".activity{display:flex;align-items:center;gap:14px;margin-top:24px}.spinner{width:24px;height:24px;"
                "border:3px solid #47624e;border-top-color:#1ed760;border-radius:50%;animation:spin .8s linear infinite}"
                "@keyframes spin{to{transform:rotate(360deg)}}.error{display:none}.device-link{color:#1ed760}"
                "</style></head><body class='setup-page'><main class='setup-card'>"
                "<div class='brand'><img class='brand-icon' src='/icons/favicon-32x32.png' alt=''>TinyTrackSpotter</div>"
                "<div class='eyebrow'>Spotify setup</div><h1 id='title'>Connecting to your device</h1>"
                "<div class='activity' id='activity'><span class='spinner' aria-hidden='true'></span>"
                "<span id='message'>Waiting for the ESP32 to finish starting...</span></div>"
                "<div class='error' id='error' role='alert'></div><a class='device-link' href='" + device_url +
                "' target='_blank' rel='noopener noreferrer'>Return to your Spotify device</a></main>"
                "<script>const flow=" + str(flow_id) + ";const css=document.querySelector('#shared-style');"
                "css.onerror=()=>setTimeout(()=>css.href='/style.css?retry='+Date.now(),2000);"
                "async function poll(){try{const r=await fetch('/login/status?flow='+flow,{cache:'no-store'});"
                "const s=await r.json();if(s.status==='ready'){document.querySelector('#message').textContent='Device ready. Opening Spotify...';"
                "location.replace(s.authorize_url);return;}if(s.status==='error'){document.querySelector('#title').textContent='Device not ready yet';"
                "document.querySelector('#activity').style.display='none';const e=document.querySelector('#error');e.textContent=s.message;"
                "e.style.display='block';return;}}catch(_){document.querySelector('#message').textContent='Reconnecting to the helper...';}"
                "setTimeout(poll,1200);}poll();</script></body></html>").encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def send_page(self, title, message, status=200, device_url=None, redirect_url=None):
        device_url = device_url or getattr(self.server, "esp_url", "")
        device_link = ""
        redirect_meta = ""
        redirect_script = ""
        if device_url:
            safe_url = escape(device_url, quote=True)
            device_link = (
                "<a class='device-link' href='" + safe_url + "' target='_blank' "
                "rel='noopener noreferrer'>Return to your Spotify device</a>"
            )
        if redirect_url:
            safe_redirect = escape(redirect_url, quote=True)
            redirect_meta = "<meta http-equiv='refresh' content='5;url=" + safe_redirect + "'>"
            redirect_script = "<script>setTimeout(()=>location.replace(" + json.dumps(redirect_url) + "),5000);</script>"

        body = ("<!doctype html><html lang='en'><head><meta charset='utf-8'>"
                "<meta name='viewport' content='width=device-width,initial-scale=1'>"
                "<meta name='color-scheme' content='dark'>"
                + redirect_meta + "<title>" + escape(title) + " | TinyTrackSpotter</title>"
                "<link rel='icon' href='/favicon.ico' sizes='any'>"
                "<link rel='icon' type='image/png' sizes='16x16' href='/icons/favicon-16x16.png'>"
                "<link rel='icon' type='image/png' sizes='32x32' href='/icons/favicon-32x32.png'>"
                "<link rel='icon' type='image/png' sizes='96x96' href='/icons/favicon-96x96.png'>"
                "<link rel='icon' type='image/png' sizes='128x128' href='/icons/favicon-128x128.png'>"
                "<link id='shared-style' rel='stylesheet' href='/style.css'>"
                "<style>body{margin:0;background:#111714;color:#f3f7f4;font:16px system-ui}"
                ".setup-page{min-height:100vh;display:grid;place-items:center;padding:24px}"
                ".setup-card{width:min(100%,480px);padding:32px;border:1px solid #34483a;border-radius:20px;background:#17201a}"
                ".device-link{display:inline-block;margin-top:24px;color:#1ed760}</style></head>"
                "<body class='setup-page'><main class='setup-card'><div class='brand'>"
                "<img class='brand-icon' src='/icons/favicon-32x32.png' alt=''>TinyTrackSpotter</div>"
                "<div class='eyebrow'>Spotify setup</div><h1>" + escape(title) +
                "</h1><p>" + escape(message) + "</p>" + device_link +
                "<p class='foot'>You can close this tab when you are ready.</p></main>"
                "<script>const css=document.querySelector('#shared-style');css.onerror=()=>setTimeout(()=>css.href='/style.css?retry='+Date.now(),2000);</script>"
                + redirect_script + "</body></html>").encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        parsed = urlsplit(self.path)
        query = parse_qs(parsed.query)

        if parsed.path == "/health.png":
            self.send_response(200)
            self.send_header("Content-Type", "image/gif")
            self.send_header("Content-Length", str(len(HEALTH_GIF)))
            self.send_header("Cache-Control", "no-store, no-cache, must-revalidate")
            self.end_headers()
            self.wfile.write(HEALTH_GIF)
            return

        if parsed.path in DEVICE_ASSETS:
            self.send_device_asset(parsed.path)
            return

        if parsed.path == "/login":
            esp_url = query.get("esp", [""])[0]
            esp_parts = urlsplit(esp_url)
            if esp_parts.scheme != "http" or not esp_parts.hostname:
                self.send_page("Setup error", "Open this helper from the device setup page.", 400)
                return

            self.server.esp_url = esp_url.rstrip("/")
            with self.server.login_lock:
                self.server.login_generation += 1
                flow_id = self.server.login_generation
                self.server.login_state = {"flow_id": flow_id, "status": "waiting"}
            thread = threading.Thread(
                target=prepare_authorization,
                args=(self.server, self.server.esp_url, flow_id),
                daemon=True,
            )
            thread.start()
            self.send_loading_page(flow_id)
            return

        if parsed.path == "/login/status":
            try:
                requested_flow = int(query.get("flow", ["0"])[0])
            except ValueError:
                requested_flow = 0
            with self.server.login_lock:
                if requested_flow == self.server.login_generation:
                    state = dict(self.server.login_state)
                else:
                    state = {"flow_id": requested_flow, "status": "error", "message": "This login attempt expired. Start again from the device page."}
            body = json.dumps(state).encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(body)
            return

        if parsed.path != "/callback":
            self.send_page("TinyTrackSpotter OAuth helper", "Start authorization from the device setup page.", 404)
            return

        oauth_error = query.get("error", [""])[0]
        code = query.get("code", [""])[0]
        esp_url = getattr(self.server, "esp_url", "")
        if oauth_error:
            self.send_page("Spotify authorization declined", oauth_error, 400)
            return
        if not code or not esp_url:
            self.send_page("Authorization could not finish", "Restart the connection from the device setup page.", 400)
            return

        try:
            client_id = esp_setting(esp_url, "clientId").strip()
            client_secret = esp_setting(esp_url, "clientSecret").strip()
            token_body = urlencode({
                "grant_type": "authorization_code",
                "code": code,
                "redirect_uri": REDIRECT_URI,
            }).encode("ascii")
            basic = b64encode((client_id + ":" + client_secret).encode("utf-8")).decode("ascii")
            token_request = Request(
                "https://accounts.spotify.com/api/token",
                data=token_body,
                headers={
                    "Authorization": "Basic " + basic,
                    "Content-Type": "application/x-www-form-urlencoded",
                },
                method="POST",
            )
            with urlopen(token_request, timeout=15) as response:
                token_data = json.loads(response.read().decode("utf-8"))
            refresh_token = token_data.get("refresh_token", "")
            if not refresh_token:
                raise ValueError("Spotify did not return a refresh token. Revoke access and authorize again.")

            save_query = urlencode({"refreshToken": refresh_token})
            esp_request(esp_url, "/settings?" + save_query, method="POST")
            self.send_page(
                "Spotify connected",
                "The refresh token was saved. Returning to your device in a few seconds...",
                redirect_url=esp_url,
            )
        except HTTPError as error:
            detail = error.read().decode("utf-8", errors="replace")[:500]
            self.send_page("Authorization failed", detail or str(error), 502)
        except (URLError, TimeoutError, OSError, ValueError, json.JSONDecodeError) as error:
            self.send_page("Authorization failed", str(error), 502)


def main():
    server = ThreadingHTTPServer(("127.0.0.1", 8888), Handler)
    server.daemon_threads = True
    server.login_lock = threading.Lock()
    server.login_generation = 0
    server.login_state = {"flow_id": 0, "status": "idle"}
    print("TinyTrackSpotter OAuth helper is listening on http://127.0.0.1:8888")
    print("Register this Spotify redirect URI: " + REDIRECT_URI)
    print("Keep this window open until Spotify authorization completes. Press Ctrl+C to stop.")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("Stopping OAuth helper.")
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
