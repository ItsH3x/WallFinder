# WallFinder

<p align="center">
  <img src="assets/wallfinder.png" alt="WallFinder Logo" width="140" height="140" />
</p>

<p align="center">
  <b>Explorador, gestor y aplicador de fondos de pantalla de alta resolución para Linux.</b><br>
  Optimizado nativamente para <b>KDE Plasma 6</b>, <b>GNOME</b>, <b>XFCE</b>, <b>Hyprland</b>, <b>Sway</b> y entornos X11 / Wayland.
</p>

---

## Características Principales

- **Búsqueda Avanzada**: Conexión directa y fluida con el catálogo de Wallhaven.
- **Filtro Cromático Inteligente**:
  - Detección perceptual mediante el algoritmo **CIEDE2000**.
  - Modo bicolor y filtros por temperatura de color (cálidos, fríos y monocromo estricto).
  - Tolerancias estrictas para evitar falsos positivos cromáticos.
- **Integración Nativa con el Escritorio**:
  - Aplica fondos instantáneamente en tu entorno con un solo clic.
  - **Soporte KDE Plasma 6**: Registra los fondos automáticamente como paquetes nativos del sistema en `~/.local/share/wallpapers/` con miniaturas reales, evitando duplicados en el selector de KDE.
  - Compatible con GNOME (`gsettings`), XFCE, Hyprland (`swww`), Sway (`swaybg`) y gestores X11 (`feh`).
- **Rendimiento**:
  - Desarrollado en **C++20** con aceleración por hardware (OpenGL 3.3).
  - Decodificación y carga de imágenes multihilo con `ThreadPool` asíncrono y caché en memoria.
  - Interfaz a 60 FPS con animaciones fluidas y culling visual.
- **Comprobación Automática de Actualizaciones**: Escanea las nuevas versiones publicadas en GitHub directamente al iniciar.

---

## Requisitos del Sistema

### Dependencias de compilación y librerías
Para compilar WallFinder en cualquier distribución Linux, asegúrate de tener instalados los siguientes paquetes:

#### En Arch Linux / CachyOS / Manjaro:
```bash
sudo pacman -S --needed base-devel cmake curl glfw-x11 nlohmann-json
```
*(Si usas Wayland puro, puedes utilizar `glfw-wayland` según tu configuración).*

#### En Ubuntu / Debian / Linux Mint:
```bash
sudo apt update
sudo apt install build-essential cmake libcurl4-openssl-dev libglfw3-dev nlohmann-json3-dev libgl1-mesa-dev
```

#### En Fedora / RHEL:
```bash
sudo dnf install gcc-c++ cmake libcurl-devel glfw-devel nlohmann-json-devel mesa-libGL-devel
```

---

## Instalación y Compilación

### 1. Clonar el repositorio
```bash
git clone https://github.com/ItsH3x/WallFinder.git
cd WallFinder
```

### 2. Compilación rápida (Script automático)
El proyecto incluye un script preparado para compilar y lanzar la aplicación con un solo comando:
```bash
chmod +x run.sh
./run.sh
```

### 3. Compilación manual con CMake
Si prefieres compilar paso a paso utilizando CMake:
```bash
# Crear directorio de compilación
mkdir -p build && cd build

# Configurar el proyecto en modo Release
cmake .. -DCMAKE_BUILD_TYPE=Release

# Compilar utilizando todos los núcleos disponibles
cmake --build . -j$(nproc)

# Ejecutar WallFinder
./wallpaper_finder
```

---

## Integración en el Menú de Aplicaciones (KDE / GNOME)

Para que WallFinder aparezca en tu menú de inicio (Kickoff de KDE, KRunner, GNOME Dash, etc.) con su ícono oficial:

```bash
# Crear directorios si no existen
mkdir -p ~/.local/share/applications ~/.local/share/icons/hicolor/512x512/apps

# Copiar el archivo de escritorio y el icono
cp wallfinder.desktop ~/.local/share/applications/
cp assets/wallfinder.png ~/.local/share/icons/hicolor/512x512/apps/wallfinder.png

# Actualizar la base de datos de escritorios
update-desktop-database ~/.local/share/applications
kbuildsycoca6 --noincremental # (Si estás en KDE Plasma)
```

---

## Pruebas del Sistema

WallFinder incluye una suite integral de pruebas unitarias que verifica la concurrencia, el cliente de red, la detección del entorno de escritorio, el análisis de color CIEDE2000 y el escáner de versiones:

```bash
./build/wallpaper_finder_test
```

---

## Licencia

Distribuido bajo la licencia MIT.
