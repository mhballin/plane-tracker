Import("env")
import os
import pathlib


def load_env_file(path=".env"):
    """Parse .env file directly — PlatformIO doesn't reliably inject it into os.environ
    before pre-build scripts run, so we do it ourselves."""
    env_path = pathlib.Path(path)
    if not env_path.exists():
        print(f"[credentials] WARNING: {path} not found — using placeholder values")
        return
    with open(env_path) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('#') or '=' not in line:
                continue
            key, _, value = line.partition('=')
            key = key.strip()
            value = value.strip()
            # Strip surrounding quotes (single or double)
            if len(value) >= 2 and value[0] in ('"', "'") and value[-1] == value[0]:
                value = value[1:-1]
            if key:
                os.environ[key] = value


load_env_file()


def exclude_helium_files(node):
    """Filter out ARM Helium assembly files from LVGL"""
    return None if "helium" in node.get_path() else node

# Filter source files to exclude Helium
env.AddBuildMiddleware(exclude_helium_files, "*")


def generate_credentials(env):
    """
    Generate src/config/credentials.h from .env variables.

    Using a generated header instead of build_flags because build_flags
    break when credentials contain spaces, @ signs, or other special
    characters that confuse SCons' Python expression evaluator.

    PlatformIO 6+ auto-loads .env from the project root, so
    os.environ will contain the values by the time this script runs.
    """
    creds_path = pathlib.Path("src/config/credentials.h")
    creds_path.parent.mkdir(parents=True, exist_ok=True)

    wifi_ssid       = os.environ.get("WIFI_SSID",            "YOUR_WIFI_SSID")
    wifi_password   = os.environ.get("WIFI_PASSWORD",        "YOUR_WIFI_PASSWORD")
    ow_key          = os.environ.get("OPENWEATHER_API_KEY",  "YOUR_OPENWEATHER_API_KEY")
    weather_city    = os.environ.get("WEATHER_CITY",         "Portland,US")
    opensky_id      = os.environ.get("OPENSKY_CLIENT_ID",    "your-opensky-client-id")
    opensky_secret  = os.environ.get("OPENSKY_CLIENT_SECRET","your-opensky-client-secret")
    home_lat        = os.environ.get("HOME_LAT",             "43.6591")
    home_lon        = os.environ.get("HOME_LON",             "-70.2568")
    aerodatabox_key = os.environ.get("AERODATABOX_API_KEY",   "your-aerodatabox-api-key")

    # Escape backslashes and double-quotes inside the string values so the
    # generated #define is valid C regardless of credential content.
    def esc(s):
        return s.replace("\\", "\\\\").replace('"', '\\"')

    content = (
        "// AUTO-GENERATED — do not edit. Source: .env via extra_script.py\n"
        "// Re-generated on every build. File is git-ignored.\n"
        "#pragma once\n"
        "\n"
        f'#define WIFI_SSID_MACRO           "{esc(wifi_ssid)}"\n'
        f'#define WIFI_PASSWORD_MACRO        "{esc(wifi_password)}"\n'
        f'#define OPENWEATHER_API_KEY_MACRO  "{esc(ow_key)}"\n'
        f'#define WEATHER_CITY_MACRO         "{esc(weather_city)}"\n'
        f'#define OPENSKY_CLIENT_ID_MACRO    "{esc(opensky_id)}"\n'
        f'#define OPENSKY_CLIENT_SECRET_MACRO "{esc(opensky_secret)}"\n'
        f"#define HOME_LAT_MACRO             {home_lat}f\n"
        f"#define HOME_LON_MACRO             {home_lon}f\n"
        f'#define AERODATABOX_API_KEY_MACRO   "{esc(aerodatabox_key)}"\n'
    )

    creds_path.write_text(content)
    print(f"[credentials] Generated {creds_path} from environment")

generate_credentials(env)
