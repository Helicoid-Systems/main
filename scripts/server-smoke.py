#!/usr/bin/env python3
"""Exercise a fresh loopback server without printing credentials or tokens."""

import argparse
import json
from pathlib import Path
import secrets
import socket
import subprocess
import tempfile
import time
from urllib.error import HTTPError, URLError
from urllib.request import Request, build_opener, ProxyHandler


def run(binary: Path, client_probes: list[list[str]] | None = None) -> list[dict]:
    checks = []
    # Bypass machine proxy settings for literal loopback development traffic.
    http = build_opener(ProxyHandler({}))
    with tempfile.TemporaryDirectory(prefix="helicoid-smoke-") as directory:
        database = Path(directory) / "smoke.db"
        initialized = subprocess.run(
            [str(binary), "--data", str(database), "init"],
            capture_output=True, text=True, timeout=15, check=True,
        )
        marker = "single-use bootstrap token (shown once, stored hashed):\n"
        bootstrap_token = initialized.stdout.split(marker, 1)[1].splitlines()[0]
        with socket.socket() as probe:
            probe.bind(("127.0.0.1", 0))
            port = probe.getsockname()[1]
        address = f"http://127.0.0.1:{port}"
        process = subprocess.Popen(
            [str(binary), "--data", str(database), "serve", "--bind", f"127.0.0.1:{port}"],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )

        def request(name, method, path, expected, payload=None, token=None):
            headers = {"Content-Type": "application/json"}
            if token is not None:
                headers["Authorization"] = f"Bearer {token}"
            body = None if payload is None else json.dumps(payload).encode()
            req = Request(address + path, data=body, headers=headers, method=method)
            try:
                response = http.open(req, timeout=10)
            except HTTPError as error:
                response = error
            with response:
                status = response.code
                raw = response.read(256 * 1024 + 1)
            if status != expected or len(raw) > 256 * 1024:
                raise RuntimeError(f"{name}: expected HTTP {expected}, received {status}")
            checks.append({"check": name, "status": status})
            return json.loads(raw) if raw else None

        try:
            deadline = time.monotonic() + 15
            while True:
                if process.poll() is not None:
                    raise RuntimeError("server exited before readiness")
                try:
                    health = request("health", "GET", "/health", 200)
                    break
                except (URLError, TimeoutError):
                    if time.monotonic() >= deadline:
                        raise RuntimeError("server readiness deadline exceeded") from None
                    time.sleep(0.1)
            features = health["features"]
            if features["remote_control"] or features["screen_observation"]:
                raise RuntimeError("development foundation advertised a remote engine")
            request("anonymous inventory denied", "GET", "/v1/computers", 401)
            password = secrets.token_urlsafe(24)
            bootstrap = request("bootstrap", "POST", "/v1/bootstrap", 201, {
                "bootstrap_token": bootstrap_token, "username": "smoke-admin", "password": password,
                "manager_public_identity": "smoke-manager", "manager_friendly_name": "Smoke manager",
            })
            credentials = {"username": "smoke-admin", "password": password,
                           "manager_public_identity": "smoke-manager"}
            login = request("approved manager login", "POST", "/v1/auth/login", 200, credentials)
            token = login["token"]
            empty = request("administration does not imply inventory", "GET", "/v1/computers", 200, token=token)
            if empty["computers"]:
                raise RuntimeError("administration implied computer access")
            computers = []
            for index in range(2):
                computers.append(request(f"pending computer {index + 1}", "POST", "/v1/computers", 201, {
                    "public_identity": f"smoke-computer-{index}", "friendly_name": f"Smoke computer {index}",
                    "platform": "windows",
                }, token))
            request("explicit access assignment", "POST", "/v1/access", 201, {
                "account_id": bootstrap["account"]["id"], "capability": "observe",
                "computer_ids": [computers[0]["id"]], "computer_scope_all": False,
            }, token)
            scoped = request("scoped inventory", "GET", "/v1/computers", 200, token=token)
            if [item["id"] for item in scoped["computers"]] != [computers[0]["id"]]:
                raise RuntimeError("inventory expanded outside explicit scope")
            for command in client_probes or []:
                probe_input = json.dumps({
                    "address": address, "username": "smoke-admin", "password": password,
                    "manager": "smoke-manager", "computer_id": computers[0]["id"],
                })
                probe_result = subprocess.run(
                    command, input=probe_input, capture_output=True, text=True, timeout=45,
                )
                if probe_result.returncode != 0:
                    # Never print captured output: test credentials were supplied over stdin.
                    raise RuntimeError(f"client probe failed with exit {probe_result.returncode}")
                checks.append({"check": "native client integration", "status": "passed"})
            request("outside scope denied", "GET", f"/v1/computers/{computers[1]['id']}", 403, token=token)
            manager = request("new manager pending", "POST", "/v1/managers", 201, {
                "public_identity": "smoke-second-manager", "friendly_name": "Second manager",
            }, token)
            pending_credentials = dict(credentials, manager_public_identity="smoke-second-manager")
            request("pending manager denied", "POST", "/v1/auth/login", 403, pending_credentials)
            request("explicit manager approval", "POST", f"/v1/managers/{manager['id']}/approve", 200,
                    {"confirm": True}, token)
            second = request("newly approved manager login", "POST", "/v1/auth/login", 200, pending_credentials)
            request("manager revocation", "POST", f"/v1/managers/{manager['id']}/revoke", 200, token=token)
            request("revoked manager session denied", "GET", "/v1/auth/session", 401, token=second["token"])
            request("file jobs explicitly unavailable", "POST", "/v1/files/jobs", 501, {}, token)
            request("logout", "POST", "/v1/auth/logout", 204, token=token)
            request("logged out session denied", "GET", "/v1/auth/session", 401, token=token)
            return checks
        finally:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=5)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path, help="path to built hs-server executable")
    parser.add_argument("--client-probe", action="append", nargs="+", metavar="COMMAND",
                        help="local client probe command; receives ephemeral test input on stdin")
    args = parser.parse_args()
    checks = run(args.binary.resolve(strict=True), args.client_probe)
    print(json.dumps({"passed": len(checks), "checks": checks}, indent=2))


if __name__ == "__main__":
    main()
