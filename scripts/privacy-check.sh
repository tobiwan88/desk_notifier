#!/usr/bin/env bash
# Scan tracked files and the staged diff for private data. Extra patterns: .privacy-patterns (gitignored).
set -u
cd "$(dirname "$0")/.."
pat=(
  '([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}'                 # MAC address
  '192\.168\.[0-9]+\.[0-9]+|10\.[0-9]+\.[0-9]+\.[0-9]+' # LAN IP
  '/Users/[A-Za-z0-9._-]+|/home/[A-Za-z0-9._-]+'       # home path
  '/dev/cu\.usb[a-z]+-?[0-9]+'                         # serial port
  '(ssid|password|passwd|secret|api[_-]?key|token)[" ]*[:=][" ]*"[^"]{4,}"'  # credential-looking literals
  'BEGIN [A-Z ]*PRIVATE KEY'
)
[ -f .privacy-patterns ] && while IFS= read -r l; do [ -n "$l" ] && pat+=("$l"); done < .privacy-patterns
rc=0
for p in "${pat[@]}"; do
  hits=$( { git grep -n -I -E -e "$p" -- . ':!uv.lock' ':!scripts/privacy-check.sh' ':!hardware/notifier-v1/*.kicad_*' ':!hardware/notifier-v1/*.step' ':!hardware/gen' ':!hardware/tools' ':!firmware/components/*/third_party' ; git diff --cached -U0 | grep -E -e "^\+[^+]" | grep -E -e "$p" ; } 2>/dev/null | grep -v 'testtoken' | head -5)
  [ -n "$hits" ] && { echo "privacy: pattern /$p/"; echo "$hits" | cut -c1-160 | sed 's/^/  /'; rc=1; }
done
[ $rc -eq 0 ] && echo "privacy-check: clean" || echo "privacy-check: FINDINGS (review above)"
exit $rc
