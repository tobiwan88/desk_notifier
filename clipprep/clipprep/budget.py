from __future__ import annotations

from dataclasses import dataclass


@dataclass(frozen=True)
class Budget:
    max_bytes: int = 1_500_000
    max_bps: int = 2_000_000  # sustained bytes per second the device can read/push
    min_fps: float = 5.0
