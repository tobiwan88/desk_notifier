# Haiku: one part lookup

Check ONE LCSC part for a JLCPCB-assembled board. Run:
  uv run python hardware/tools/jlcparts.py {lcsc}
and, if needed, the datasheet URL it returns.

Report JSON only:
{"lcsc": "...", "mpn": "...", "library": "Basic|Extended", "stock": n, "price_usd": x,
 "matches": {"value": "{value}", "package": "{package}"}, "ok": bool, "notes": "..."}
