"""
PlatformIO custom OTA uploader.

Supports:
1) ElegantOTA 3.x API (default):
   - GET  /ota/start?hash=<md5>&mode=firmware
   - POST /ota/upload
2) Legacy AsyncElegantOTA endpoint:
   - POST /update

Configuration (platformio.ini in the selected [env:*]):
  upload_protocol = custom
  extra_scripts = post:upload.py
    custom_ota_url = http://192.168.5.60
    custom_ota_username = admin
    custom_ota_password = admin
    custom_ota_auth_type = digest   ; digest|basic|none

Any value can also be supplied via environment variables:
  OTA_URL, OTA_USERNAME, OTA_PASSWORD, OTA_AUTH_TYPE
"""

import hashlib
import importlib
import os
from typing import Any, TYPE_CHECKING
from urllib.parse import urlsplit, urlunsplit

import requests
from requests.auth import HTTPBasicAuth, HTTPDigestAuth

if TYPE_CHECKING:
    # Provided by PlatformIO/SCons at runtime.
    env: Any

# Keep a local symbol so static analyzers don't report "env is unbound".
env: Any = None

try:
    Import("env")  # type: ignore[name-defined]
except NameError:
    # Allows static analysis (Pylance/pyright) outside PlatformIO runtime.
    def Import(*_args, **_kwargs):  # type: ignore[no-redef]
        raise RuntimeError("This script must be executed by PlatformIO/SCons")


class _DummyEnv:
    def Execute(self, *_args, **_kwargs):
        return None

    def Replace(self, *_args, **_kwargs):
        return None

    def GetProjectOption(self, *_args, **_kwargs):
        return None


if env is None:
    env = _DummyEnv()

try:
    requests_toolbelt_module = importlib.import_module("requests_toolbelt")
    tqdm_module = importlib.import_module("tqdm")
except ImportError:
    env.Execute("$PYTHONEXE -m pip install requests_toolbelt")
    env.Execute("$PYTHONEXE -m pip install tqdm")
    requests_toolbelt_module = importlib.import_module("requests_toolbelt")
    tqdm_module = importlib.import_module("tqdm")

MultipartEncoder = requests_toolbelt_module.MultipartEncoder
MultipartEncoderMonitor = requests_toolbelt_module.MultipartEncoderMonitor
tqdm = tqdm_module.tqdm


def _option(name, env_name=None, default=None):
    try:
        value = env.GetProjectOption(name)
    except Exception:
        value = None

    if (value is None or value == "") and env_name:
        value = os.getenv(env_name)

    if value is None or value == "":
        return default
    return value


def _normalize_url(url):
    # Accept plain host URLs and keep backwards compatibility for /update.
    parsed = urlsplit(url)
    if not parsed.scheme:
        url = f"http://{url}"
        parsed = urlsplit(url)
    if not parsed.path:
        parsed = parsed._replace(path="/")
    return urlunsplit(parsed)


def _build_auth(auth_type, username, password):
    auth_type = (auth_type or "digest").lower().strip()
    if auth_type == "none":
        return None
    if auth_type == "basic":
        return HTTPBasicAuth(username or "", password or "")
    return HTTPDigestAuth(username or "", password or "")


def _request_ok(response, action):
    if response.status_code != 200:
        raise RuntimeError(
            f"{action} failed: HTTP {response.status_code} {response.text}"
        )


def _upload_elegant_ota(base_url, auth, firmware_path, md5_hash):
    start_resp = requests.get(
        f"{base_url}/ota/start",
        params={"hash": md5_hash, "mode": "firmware"},
        auth=auth,
        timeout=20,
    )
    _request_ok(start_resp, "OTA start")

    with open(firmware_path, "rb") as firmware:
        encoder = MultipartEncoder(
            fields={
                "firmware": ("firmware.bin", firmware, "application/octet-stream"),
            }
        )

        bar = tqdm(
            desc="Upload Progress",
            total=encoder.len,
            dynamic_ncols=True,
            unit="B",
            unit_scale=True,
            unit_divisor=4096,
        )
        monitor = MultipartEncoderMonitor(
            encoder,
            lambda progress: bar.update(progress.bytes_read - bar.n),
        )

        upload_resp = requests.post(
            f"{base_url}/ota/upload",
            auth=auth,
            data=monitor,
            headers={"Content-Type": monitor.content_type},
            timeout=240,
        )
        bar.close()
    _request_ok(upload_resp, "OTA upload")


def _upload_legacy_update(upload_url, auth, firmware_path, md5_hash):
    with open(firmware_path, "rb") as firmware:
        encoder = MultipartEncoder(
            fields={
                "MD5": md5_hash,
                "firmware": ("firmware", firmware, "application/octet-stream"),
            }
        )
        bar = tqdm(
            desc="Upload Progress",
            total=encoder.len,
            dynamic_ncols=True,
            unit="B",
            unit_scale=True,
            unit_divisor=4096,
        )
        monitor = MultipartEncoderMonitor(
            encoder,
            lambda progress: bar.update(progress.bytes_read - bar.n),
        )

        upload_resp = requests.post(
            upload_url,
            auth=auth,
            data=monitor,
            headers={"Content-Type": monitor.content_type},
            timeout=240,
        )
        bar.close()
    _request_ok(upload_resp, "Legacy OTA upload")


def on_upload(source, target, env):
    firmware_path = str(source[0])
    upload_url = _option("custom_ota_url", "OTA_URL")
    if not upload_url:
        # Backwards compatibility with older local configs.
        upload_url = _option("upload_url", "OTA_URL")
    if not upload_url:
        raise RuntimeError(
            "Missing OTA URL. Set 'custom_ota_url' in platformio.ini or OTA_URL env var."
        )

    upload_url = _normalize_url(upload_url)
    username = _option("custom_ota_username", "OTA_USERNAME", default="")
    password = _option("custom_ota_password", "OTA_PASSWORD", default="")
    auth_type = _option("custom_ota_auth_type", "OTA_AUTH_TYPE", default="digest")
    auth = _build_auth(auth_type, username, password)

    with open(firmware_path, "rb") as firmware:
        md5_hash = hashlib.md5(firmware.read()).hexdigest()

    print(f"Uploading '{firmware_path}' to {upload_url}")
    print(f"Auth mode: {auth_type}")

    # If URL explicitly points to /update, use legacy path.
    if urlsplit(upload_url).path.rstrip("/") == "/update":
        _upload_legacy_update(upload_url, auth, firmware_path, md5_hash)
    else:
        # Use ElegantOTA 3.x flow by default. upload_url can be host root.
        parsed = urlsplit(upload_url)
        base_url = urlunsplit((parsed.scheme, parsed.netloc, "", "", "")).rstrip("/")
        _upload_elegant_ota(base_url, auth, firmware_path, md5_hash)

    print("OTA upload finished successfully")


env.Replace(UPLOADCMD=on_upload)
