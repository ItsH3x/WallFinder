#!/usr/bin/env python3
import os
import sys
import glob
import json
import subprocess
import re
from pathlib import Path

def update_kde():
    home = Path.home()
    share_wallpapers = home / ".local" / "share" / "wallpapers"
    share_wallpapers.mkdir(parents=True, exist_ok=True)

    # Search for wallpapers in common directories
    candidate_dirs = [
        home / "Imágenes" / "Wallpapers",
        home / "Pictures" / "Wallpapers",
        home / "Wallpapers",
        home / "Imágenes",
        home / "Pictures",
    ]

    found_images = []
    registered_dirs = set()

    for d in candidate_dirs:
        if d.exists() and d.is_dir():
            for ext in ("*.jpg", "*.jpeg", "*.png", "*.webp"):
                for p in d.glob(ext):
                    if p.is_file():
                        found_images.append(p)
                        registered_dirs.add(str(d))

    print(f"Encontradas {len(found_images)} imágenes de fondo en {list(registered_dirs)}")

    # 1. Crear paquetes de fondos en ~/.local/share/wallpapers/
    pkg_count = 0
    for img in found_images:
        stem = img.stem
        # Clean id
        clean_id = stem
        for prefix in ("wallhaven-", "wallhaven_"):
            if clean_id.startswith(prefix):
                clean_id = clean_id[len(prefix):]
                break

        pkg_id = f"wallhaven_{clean_id}"
        pkg_dir = share_wallpapers / pkg_id
        contents_img_dir = pkg_dir / "contents" / "images"
        contents_img_dir.mkdir(parents=True, exist_ok=True)

        target_img = contents_img_dir / img.name
        screenshot_target = pkg_dir / "contents" / "screenshot.png"
        flat_target = share_wallpapers / img.name

        for dest in (target_img, screenshot_target, flat_target):
            try:
                if dest.is_symlink() or dest.exists():
                    dest.unlink()
                dest.symlink_to(img.resolve())
            except Exception:
                pass

        # metadata.json (KDE Plasma 6 KPlugin)
        meta_json_path = pkg_dir / "metadata.json"
        meta = {
            "KPlugin": {
                "Id": pkg_id,
                "Name": f"Wallhaven {clean_id}",
                "Authors": [{"Name": "Wallhaven"}],
                "License": "CC-BY-SA-4.0"
            }
        }
        with open(meta_json_path, "w", encoding="utf-8") as f:
            json.dump(meta, f, indent=4)

        # metadata.desktop (KDE Plasma 5 / KF5 compatibility)
        meta_desktop_path = pkg_dir / "metadata.desktop"
        with open(meta_desktop_path, "w", encoding="utf-8") as f:
            f.write(f"[Desktop Entry]\nName=Wallhaven {clean_id}\nX-KDE-PluginInfo-Name={pkg_id}\nX-KDE-PluginInfo-Author=Wallhaven\nX-KDE-PluginInfo-License=CC-BY-SA-4.0\n")

        pkg_count += 1

    print(f"Registrados {pkg_count} paquetes en {share_wallpapers}")

    # 2. Actualizar ~/.config/plasma-org.kde.plasma.desktop-appletsrc
    appletsrc_path = home / ".config" / "plasma-org.kde.plasma.desktop-appletsrc"
    if appletsrc_path.exists():
        with open(appletsrc_path, "r", encoding="utf-8", errors="ignore") as f:
            content = f.read()

        # Encontrar todas las secciones [Containments][...][Wallpaper][org.kde.image][General]
        pattern = r"(\[Containments\]\[\d+\]\[Wallpaper\]\[org\.kde\.image\]\[General\][^\[]*)"
        
        def update_containment(match):
            block = match.group(1)
            lines = block.splitlines()
            
            # SlidePaths
            slide_paths = []
            has_slide = False
            user_wallpapers = []
            has_users = False
            
            for line in lines:
                if line.startswith("SlidePaths="):
                    has_slide = True
                    slide_paths = [p.strip() for p in line.split("=", 1)[1].split(",") if p.strip()]
                elif line.startswith("usersWallpapers="):
                    has_users = True
                    user_wallpapers = [p.strip() for p in line.split("=", 1)[1].split(",") if p.strip()]

            # Add directories to SlidePaths
            for d in registered_dirs:
                d_slash = d.rstrip("/") + "/"
                if d_slash not in slide_paths and d not in slide_paths:
                    slide_paths.append(d_slash)

            # Add images to user_wallpapers
            for img in found_images:
                s_img = str(img.resolve())
                if s_img not in user_wallpapers:
                    user_wallpapers.append(s_img)

            # Rebuild lines
            new_lines = []
            for line in lines:
                if line.startswith("SlidePaths="):
                    new_lines.append(f"SlidePaths={','.join(slide_paths)}")
                elif line.startswith("usersWallpapers="):
                    new_lines.append(f"usersWallpapers={','.join(user_wallpapers)}")
                else:
                    new_lines.append(line)

            if not has_slide and slide_paths:
                new_lines.append(f"SlidePaths={','.join(slide_paths)}")
            if not has_users and user_wallpapers:
                new_lines.append(f"usersWallpapers={','.join(user_wallpapers)}")

            return "\n".join(new_lines)

        new_content = re.sub(pattern, update_containment, content)
        
        # Guardar copia de seguridad y escribir nuevo archivo
        backup_path = appletsrc_path.with_suffix(".appletsrc.bak_wallfinder")
        try:
            with open(backup_path, "w", encoding="utf-8") as f:
                f.write(content)
            with open(appletsrc_path, "w", encoding="utf-8") as f:
                f.write(new_content)
            print("Configuración de Plasma desktop-appletsrc actualizada con éxito.")
        except Exception as e:
            print(f"Error escribiendo appletsrc: {e}")

    # 3. Refrescar sycoca de KDE 6 / 5
    for cmd in (["kbuildsycoca6", "--noincremental"], ["kbuildsycoca5", "--noincremental"]):
        try:
            res = subprocess.run(cmd, capture_output=True, text=True, timeout=10)
            if res.returncode == 0:
                print(f"Cache de configuración de KDE ({cmd[0]}) actualizado exitosamente.")
                break
        except Exception:
            pass

    print("¡Lista de fondos de KDE actualizada completamente!")

if __name__ == "__main__":
    update_kde()

