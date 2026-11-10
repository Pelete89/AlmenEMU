#!/usr/bin/env python3
"""Download the latest Azahar libretro core into ./cores for the current OS."""
from __future__ import annotations

import io
import json
import platform
import sys
import urllib.request
import zipfile
from pathlib import Path

REPO = "azahar-emu/azahar"
API = f"https://api.github.com/repos/{REPO}/releases/latest"


def target() -> tuple[str, str]:
    system = platform.system()
    machine = platform.machine().lower()
    if system == "Windows":
        return "windows-x86_64", ".dll"
    if system == "Linux":
        if machine not in ("x86_64", "amd64"):
            raise SystemExit(f"Linux {machine}: compila el core desde fuente para esta arquitectura.")
        return "linux-x86_64", ".so"
    if system == "Darwin":
        arch = "arm64" if machine in ("arm64", "aarch64") else "x86_64"
        return f"macos-{arch}", ".dylib"
    raise SystemExit(f"Sistema no soportado por el script: {system}")


def main() -> int:
    key, ext = target()
    print(f"Buscando core para {key}...")
    request = urllib.request.Request(API, headers={"User-Agent": "AlmenEMU-core-fetcher"})
    with urllib.request.urlopen(request, timeout=60) as response:
        release = json.load(response)

    asset = next(
        (item for item in release.get("assets", [])
         if "libretro" in item["name"] and key in item["name"]),
        None,
    )
    if asset is None:
        print("No se encontró un asset libretro para esta plataforma.", file=sys.stderr)
        return 1

    print(f"Descargando {asset['name']} ({asset['size'] // 1024} KiB)...")
    with urllib.request.urlopen(asset["browser_download_url"], timeout=120) as response:
        archive = io.BytesIO(response.read())

    output = Path(__file__).resolve().parent.parent / "cores" / f"azahar_libretro{ext}"
    output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(archive) as zipped:
        member = next((name for name in zipped.namelist() if name.endswith(ext)), None)
        if member is None:
            print("El ZIP no contiene el core esperado.", file=sys.stderr)
            return 1
        output.write_bytes(zipped.read(member))

    print(f"Core instalado en: {output}")
    print("Vuelve a compilar o copia el core junto al ejecutable.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
