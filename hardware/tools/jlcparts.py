"""JLCPCB parts library lookup (stock, Basic/Extended, price) for BOM checks.

Uses the endpoint behind jlcpcb.com/parts. Usage:
  jlcparts.py C2913204 C165948        # exact LCSC numbers
  jlcparts.py -s "1x7 female 2.54"    # keyword search
"""

import argparse
import json
import sys
import urllib.request

URL = "https://jlcpcb.com/api/overseas-pcb-order/v1/shoppingCart/smtGood/selectSmtComponentList"
LIB = {"base": "Basic", "expand": "Extended"}


def query(keyword: str, size: int = 10) -> list[dict]:
    body = json.dumps({"keyword": keyword, "currentPage": 1, "pageSize": size}).encode()
    req = urllib.request.Request(URL, body, {"Content-Type": "application/json", "User-Agent": "desk-hardware"})
    with urllib.request.urlopen(req, timeout=20) as r:
        data = json.load(r)["data"]["componentPageInfo"]
    return [
        {
            "lcsc": c["componentCode"],
            "mpn": c["componentModelEn"],
            "package": c.get("componentSpecificationEn"),
            "library": LIB.get(c["componentLibraryType"], c["componentLibraryType"]),
            "preferred": bool(c.get("preferredComponentFlag")),
            "stock": c["stockCount"],
            "price": (c.get("componentPrices") or [{}])[0].get("productPrice"),
            "description": c.get("describe"),
            "datasheet": c.get("dataManualUrl"),
        }
        for c in (data.get("list") or [])
    ]


def lookup(lcsc: str) -> dict | None:
    return next((c for c in query(lcsc, 5) if c["lcsc"] == lcsc), None)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("terms", nargs="+")
    ap.add_argument("-s", "--search", action="store_true")
    ap.add_argument("-n", type=int, default=10)
    a = ap.parse_args()
    out = {t: (query(t, a.n) if a.search else lookup(t)) for t in a.terms}
    json.dump(out, sys.stdout, indent=1, ensure_ascii=False)


if __name__ == "__main__":
    main()
