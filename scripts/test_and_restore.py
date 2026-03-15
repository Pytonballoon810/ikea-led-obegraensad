#!/usr/bin/env python3
"""Run embedded tests, then automatically restore real firmware over OTA.

Flow:
1) Upload and execute unit-test firmware with `pio test`.
2) Wait until the test image enters OTA recovery mode.
3) Upload the real firmware through recovery OTA endpoint.

Defaults are aligned with the test recovery mode implemented in test firmware:
- Recovery AP: IKEA-Test-OTA / adminadmin
- Recovery OTA endpoint: http://192.168.4.1/update
- Recovery auth: admin / admin
"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
import time
from typing import Mapping
from urllib.error import URLError
from urllib.parse import urlsplit
from urllib.request import urlopen


def _run(cmd: list[str], env: Mapping[str, str] | None = None) -> None:
    print(f"[FLOW] Running: {' '.join(cmd)}")
    subprocess.run(cmd, check=True, env=dict(os.environ, **(env or {})))


def _is_recovery_http_reachable(host: str, timeout: float = 2.0) -> bool:
    try:
        with urlopen(f"http://{host}/", timeout=timeout) as response:
            if response.status != 200:
                return False
            body = response.read().decode("utf-8", errors="ignore")
            # Distinguish real recovery firmware from generic gateways/routers.
            return "Test image OTA mode" in body
    except (TimeoutError, URLError, OSError):
        return False


def _connect_linux_ap(ssid: str, password: str) -> bool:
    nmcli = shutil.which("nmcli")
    if not nmcli:
        print("[FLOW] nmcli not found; skipping AP auto-connect.")
        return False

    print(f"[FLOW] Attempting to connect host to recovery AP '{ssid}' via nmcli")
    try:
        subprocess.run(
            [nmcli, "dev", "wifi", "connect", ssid, "password", password],
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
        )
        return True
    except subprocess.CalledProcessError as exc:
        print("[FLOW] Failed to connect to recovery AP automatically.")
        print(exc.stdout or "")
        return False


def _wait_for_recovery(
    hosts: list[str],
    timeout_seconds: int,
    connect_ap: bool,
    ap_ssid: str,
    ap_password: str,
) -> str:
    if not hosts:
        raise ValueError("No recovery hosts configured")

    host_list = ", ".join(f"http://{host}/update" for host in hosts)
    print(f"[FLOW] Waiting for recovery endpoint at one of: {host_list}")

    deadline = time.time() + timeout_seconds
    connected_attempted = False

    while time.time() < deadline:
        for host in hosts:
            if _is_recovery_http_reachable(host):
                print(f"[FLOW] Recovery OTA endpoint is reachable at http://{host}/update")
                return host

        # Try a single AP connection attempt (Linux only) if requested.
        if connect_ap and not connected_attempted:
            connected_attempted = True
            _connect_linux_ap(ap_ssid, ap_password)

        time.sleep(2)

    raise TimeoutError(
        "Timed out waiting for test firmware recovery OTA endpoint. "
        "Ensure test image finished and host is connected to the correct network."
    )


def _wait_for_upload_target(ota_url: str, timeout_seconds: int) -> None:
    parsed = urlsplit(ota_url)
    if not parsed.scheme or not parsed.netloc:
        raise ValueError(f"Invalid OTA URL: {ota_url}")

    probe_url = f"{parsed.scheme}://{parsed.netloc}/"
    print(f"[FLOW] Waiting for upload target at {probe_url}")
    deadline = time.time() + timeout_seconds

    while time.time() < deadline:
        try:
            with urlopen(probe_url, timeout=2.0) as _:
                print("[FLOW] Upload target is reachable.")
                return
        except (TimeoutError, URLError, OSError):
            time.sleep(2)

    raise TimeoutError(
        f"Timed out waiting for OTA target host: {parsed.netloc}. "
        "Verify the target URL and network connectivity."
    )


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Upload test image, run tests, then auto-upload real firmware via recovery OTA."
    )
    parser.add_argument("--test-env", default="esp32dev-unit", help="PlatformIO test environment")
    parser.add_argument(
        "--restore-env",
        default="esp32dev-ota",
        help="PlatformIO upload environment for real firmware",
    )
    parser.add_argument(
        "--ota-url",
        default=os.getenv("OTA_URL", ""),
        help="Final OTA URL for real firmware upload (e.g. http://<device-ip> or http://<device-ip>/update)",
    )
    parser.add_argument(
        "--ota-username",
        default=os.getenv("OTA_USERNAME", "admin"),
        help="Final OTA username",
    )
    parser.add_argument(
        "--ota-password",
        default=os.getenv("OTA_PASSWORD", "admin"),
        help="Final OTA password",
    )
    parser.add_argument(
        "--ota-auth-type",
        default=os.getenv("OTA_AUTH_TYPE", "digest"),
        help="Final OTA auth mode: digest|basic|none",
    )
    parser.add_argument(
        "--recovery-host",
        default=os.getenv("RECOVERY_HOST", ""),
        help=(
            "Host/IP to probe for test-firmware recovery readiness. "
            "Default: ota-url host."
        ),
    )
    parser.add_argument(
        "--wait-seconds",
        type=int,
        default=120,
        help="How long to wait for recovery endpoint",
    )
    parser.add_argument(
        "--connect-recovery-ap",
        action="store_true",
        help="Try to connect host to recovery AP via nmcli (Linux)",
    )
    parser.add_argument("--ap-ssid", default="IKEA-Test-OTA", help="Recovery AP SSID")
    parser.add_argument("--ap-password", default="adminadmin", help="Recovery AP password")
    args = parser.parse_args()

    if not args.ota_url:
        print("[FLOW] Missing OTA URL. Pass --ota-url or set OTA_URL.")
        return 2

    ota_parts = urlsplit(args.ota_url)
    if not ota_parts.scheme or not ota_parts.netloc:
        print(f"[FLOW] Invalid OTA URL: {args.ota_url}")
        return 2
    ota_host = ota_parts.hostname or ""

    # If not explicitly provided, probe OTA target host by default.
    # Add AP gateway only when AP mode is explicitly requested.
    if args.recovery_host:
        recovery_hosts = [args.recovery_host]
    else:
        recovery_hosts = [ota_host]
        if args.connect_recovery_ap and "192.168.4.1" not in recovery_hosts:
            recovery_hosts.append("192.168.4.1")

    try:
        # 1) Upload and execute tests. Test firmware then stays in OTA recovery mode.
        _run(["pio", "test", "-e", args.test_env])

        # 2) Wait for recovery endpoint; optionally switch host to test AP.
        recovery_host = _wait_for_recovery(
            hosts=recovery_hosts,
            timeout_seconds=args.wait_seconds,
            connect_ap=args.connect_recovery_ap,
            ap_ssid=args.ap_ssid,
            ap_password=args.ap_password,
        )

        if recovery_host != ota_host:
            print(
                "[FLOW] Recovery came up on a different host than OTA target. "
                "Proceeding with configured OTA URL for final upload."
            )

        # Ensure the final OTA target (real firmware endpoint) is reachable.
        _wait_for_upload_target(args.ota_url, args.wait_seconds)

        # 3) Upload real firmware over recovery endpoint using dedicated OTA env.
        _run(
            ["pio", "run", "-e", args.restore_env, "-t", "upload"],
            env={
                "OTA_URL": args.ota_url,
                "OTA_USERNAME": args.ota_username,
                "OTA_PASSWORD": args.ota_password,
                "OTA_AUTH_TYPE": args.ota_auth_type,
            },
        )

        print("[FLOW] Real firmware uploaded successfully after test image run.")
        return 0
    except subprocess.CalledProcessError as exc:
        print(f"[FLOW] Command failed with exit code {exc.returncode}")
        return exc.returncode
    except TimeoutError as exc:
        print(f"[FLOW] {exc}")
        return 2


if __name__ == "__main__":
    sys.exit(main())
