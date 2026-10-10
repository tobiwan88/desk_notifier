"""Device registry: manual list (EYES_DEVICES) + mDNS discovery of _eyes._tcp."""
from __future__ import annotations

import os
import threading
from dataclasses import dataclass

from eyes_client import Device


@dataclass
class Entry:
    id: str
    host: str
    token: str


class Registry:
    def __init__(self, default_token: str | None = None):
        self.token = default_token if default_token is not None else os.environ.get("EYES_TOKEN", "")
        self._found: dict[str, str] = {}
        self._lock = threading.Lock()
        self._zc = None
        for h in filter(None, (s.strip() for s in os.environ.get("EYES_DEVICES", "").split(","))):
            host = h.split(":")[0]
            self._found[h if host.replace(".", "").isdigit() else host.split(".")[0]] = h

    def add(self, id_: str, host: str) -> None:
        with self._lock:
            self._found[id_] = host

    def start_mdns(self) -> None:
        try:
            from zeroconf import ServiceBrowser, Zeroconf
        except ImportError:
            return
        reg = self

        class L:
            def add_service(self, zc, type_, name):
                info = zc.get_service_info(type_, name)
                if info and info.parsed_addresses():
                    reg.add(name.split(".")[0], info.parsed_addresses()[0])

            def update_service(self, *a): pass
            def remove_service(self, *a): pass

        self._zc = Zeroconf()
        ServiceBrowser(self._zc, "_eyes._tcp.local.", L())

    def entries(self) -> list[Entry]:
        with self._lock:
            return [Entry(i, h, self.token) for i, h in sorted(self._found.items())]

    def get(self, id_: str) -> Device | None:
        with self._lock:
            host = self._found.get(id_)
        return Device(host, self.token, timeout=8) if host else None
