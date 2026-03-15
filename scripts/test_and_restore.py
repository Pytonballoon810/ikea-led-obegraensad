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
import socket
import subprocess
import sys
import time
from typing import Mapping


def _run(cmd: list[str], env: Mapping[str, str] | None = None) -> None:
    print(f"[FLOW] Running: {' '.join(cmd)}")
    subprocess.run(cmd, check=True, env=dict(os.environ, **(env or {})))


def _is_recovery_http_reachable(host: str, port: int = 80, timeout: float = 2.0) -> bool:
    try:
        with socket.create_connection((host, port), timeout=timeout):
            return True
    except OSError:
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
    host: str,
    timeout_seconds: int,
    connect_ap: bool,
    ap_ssid: str,
    ap_password: str,
) -> None:
    print(f"[FLOW] Waiting for recovery endpoint at http://{host}/update")

    deadline = time.time() + timeout_seconds
    connected_attempted = False

    while time.time() < deadline:
        if _is_recovery_http_reachable(host):
            print("[FLOW] Recovery OTA endpoint is reachable.")
            return

        # Try a single AP connection attempt (Linux only) if requested.
        if connect_ap and not connected_attempted:
            connected_attempted = True
            _connect_linux_ap(ap_ssid, ap_password)

        time.sleep(2)

    raise TimeoutError(
        "Timed out waiting for test firmware recovery OTA endpoint. "
        "Ensure test image finished and host is connected to IKEA-Test-OTA."
    )


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Upload test image, run tests, then auto-upload real firmware via recovery OTA."
    )
    parser.add_argument("--test-env", default="esp32dev-unit", help="PlatformIO test environment")
    parser.add_argument(
        "--restore-env",
        default="esp32dev-recovery-ota",
        help="PlatformIO upload environment for real firmware",
    )
    parser.add_argument(
        "--recovery-host",
        default="192.168.4.1",
        help="Recovery firmware host/IP",
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

    try:
        # 1) Upload and execute tests. Test firmware then stays in OTA recovery mode.
        _run(["pio", "test", "-e", args.test_env])

        # 2) Wait for recovery endpoint; optionally switch host to test AP.
        _wait_for_recovery(
            host=args.recovery_host,
            timeout_seconds=args.wait_seconds,
            connect_ap=args.connect_recovery_ap,
            ap_ssid=args.ap_ssid,
            ap_password=args.ap_password,
        )

        # 3) Upload real firmware over recovery endpoint using dedicated OTA env.
        _run(
            ["pio", "run", "-e", args.restore_env, "-t", "upload"],
            env={
                "OTA_URL": f"http://{args.recovery_host}/update",
                "OTA_USERNAME": "admin",
                "OTA_PASSWORD": "admin",
                "OTA_AUTH_TYPE": "digest",
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
