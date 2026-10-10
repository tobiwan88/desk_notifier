# Source me: activates ESP-IDF v6.1 (Python 3.13 env) regardless of the default python3.
_py13="$(command -v python3.13 || echo /opt/homebrew/bin/python3.13)"
_shim="$(mktemp -d)"; ln -sf "$_py13" "$_shim/python3"
export PATH="$_shim:$PATH"
. "${IDF_PATH_ROOT:-$HOME/esp/esp-idf}/export.sh" >/dev/null 2>&1
