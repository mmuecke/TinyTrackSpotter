"""Local Spotify OAuth callback bridge for TinyTrackSpotter.

Run this on the computer whose browser will approve Spotify access. The helper
binds only to IPv4 loopback and forwards the resulting refresh token to the ESP.
"""

from base64 import b64encode
from html import escape
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import threading
import time
from urllib.error import HTTPError, URLError
from urllib.parse import parse_qs, urlencode, urlsplit
from urllib.request import Request, urlopen


REDIRECT_URI = "http://127.0.0.1:8888/callback"
SCOPES = "user-read-currently-playing user-read-playback-state"


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

    def send_loading_page(self, flow_id):
        device_url = escape(getattr(self.server, "esp_url", ""), quote=True)
        body = ("<!doctype html><html lang='en'><head><meta charset='utf-8'>"
                "<meta name='viewport' content='width=device-width,initial-scale=1'>"
                "<meta name='color-scheme' content='dark'><title>Connecting device | TinyTrackSpotter</title>"
                "<style>*{box-sizing:border-box}body{margin:0;min-height:100vh;padding:24px;display:grid;"
                "place-items:center;background:radial-gradient(ellipse at 50% 0%,#203c2b 0,#111714 58%);"
                "color:#f3f7f4;font:16px/1.55 system-ui,-apple-system,Segoe UI,sans-serif}"
                ".card{width:min(100%,480px);padding:36px;border:1px solid #34483a;border-radius:22px;"
                "background:rgba(23,32,26,.96);box-shadow:0 24px 80px #0008}"
                ".brand{display:flex;align-items:center;gap:10px;color:#b9c8bd;font-size:13px;"
                "font-weight:700;letter-spacing:.12em;text-transform:uppercase}.mark{width:32px;height:32px;"
                "display:grid;place-items:center;border-radius:50%;background:#1ed760;color:#102016;font-size:17px}"
                ".eyebrow{margin:32px 0 8px;color:#1ed760;font-size:12px;font-weight:700;"
                "letter-spacing:.14em;text-transform:uppercase}h1{margin:0;font-size:clamp(27px,6vw,38px);"
                "line-height:1.12;letter-spacing:-.035em}p{margin:16px 0 0;color:#bdc9c0}.activity{display:flex;"
                "align-items:center;gap:14px;margin-top:26px;color:#dce8df}.spinner{width:24px;height:24px;"
                "border:3px solid #47624e;border-top-color:#1ed760;border-radius:50%;animation:spin .8s linear infinite}"
                "@keyframes spin{to{transform:rotate(360deg)}}.device-link{display:inline-block;margin-top:28px;"
                "color:#1ed760;text-decoration:none;font-weight:700}.device-link:hover{text-decoration:underline}"
                ".error{display:none;margin-top:20px;padding:13px;border:1px solid #714d43;border-radius:10px;"
                "background:#33231f;color:#ffd6c8}@media(max-width:480px){.card{padding:27px;border-radius:17px}}"
                "</style></head><body><main class='card'><div class='brand'><span class='mark' aria-hidden='true'>&#9835;</span>"
                "TinyTrackSpotter</div><div class='eyebrow'>Spotify setup</div><h1 id='title'>Connecting to your device</h1>"
                "<div class='activity' id='activity'><span class='spinner' aria-hidden='true'></span>"
                "<span id='message'>Waiting for the ESP32 to finish starting...</span></div>"
                "<div class='error' id='error' role='alert'></div><a class='device-link' href='" + device_url +
                "' target='_blank' rel='noopener noreferrer'>Return to your Spotify device</a></main>"
                "<script>const flow=" + str(flow_id) + ";async function poll(){try{const r=await fetch('/login/status?flow='+flow,{cache:'no-store'});"
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

    def send_page(self, title, message, status=200, device_url=None):
        device_url = device_url or getattr(self.server, "esp_url", "")
        device_link = ""
        if device_url:
            safe_url = escape(device_url, quote=True)
            device_link = (
                "<a class='device-link' href='" + safe_url + "' target='_blank' "
                "rel='noopener noreferrer'>Return to your Spotify device <span aria-hidden='true'>↗</span></a>"
            )

        body = ("<!doctype html><html lang='en'><head><meta charset='utf-8'>"
                "<meta name='viewport' content='width=device-width,initial-scale=1'>"
                "<meta name='color-scheme' content='dark'>"
                "<title>" + escape(title) + " · TinyTrackSpotter</title>"
                "<style>"
                "*{box-sizing:border-box}body{margin:0;min-height:100vh;padding:24px;display:grid;"
                "place-items:center;background:radial-gradient(ellipse at 50% 0%,#203c2b 0,#111714 58%);"
                "color:#f3f7f4;font:16px/1.55 Inter,ui-sans-serif,system-ui,-apple-system,Segoe UI,sans-serif}"
                ".card{width:min(100%,480px);padding:36px;border:1px solid #34483a;border-radius:22px;"
                "background:rgba(23,32,26,.96);box-shadow:0 24px 80px #0008}"
                ".brand{display:flex;align-items:center;gap:10px;color:#b9c8bd;font-size:13px;"
                "font-weight:700;letter-spacing:.12em;text-transform:uppercase}.mark{width:32px;height:32px;"
                "display:grid;place-items:center;border-radius:50%;background:#1ed760;color:#102016;font-size:17px}"
                ".eyebrow{margin:32px 0 8px;color:#1ed760;font-size:12px;font-weight:700;"
                "letter-spacing:.14em;text-transform:uppercase}h1{margin:0;font-size:clamp(27px,6vw,38px);"
                "line-height:1.12;letter-spacing:-.035em}p{margin:16px 0 0;color:#bdc9c0}"
                ".device-link{display:flex;justify-content:space-between;align-items:center;margin-top:28px;"
                "padding:13px 16px;border-radius:10px;background:#1ed760;color:#102016;"
                "font-weight:750;text-decoration:none}.device-link:hover{background:#63ec91}"
                ".foot{margin-top:26px!important;font-size:13px;color:#829087}"
                "@media(max-width:480px){.card{padding:27px;border-radius:17px}}"
                "</style></head><body><main class='card'><div class='brand'><span class='mark' aria-hidden='true'>♪</span>"
                "TinyTrackSpotter</div><div class='eyebrow'>Spotify setup</div><h1>" + escape(title) +
                "</h1><p>" + escape(message) + "</p>" + device_link +
                "<p class='foot'>You can close this tab when you are ready.</p></main></body></html>").encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        parsed = urlsplit(self.path)
        query = parse_qs(parsed.query)

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
            self.send_page("Spotify connected", "The refresh token was saved to your TinyTrackSpotter device.")
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
