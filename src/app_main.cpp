#include "ui/ui_manager.hpp"
#include "app/config.hpp"
#include <iostream>
#include <string_view>

int main(int argc, char* argv[]) {
    // Manejo de argumentos CLI
    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            std::cout << "WallFinder - v1.0.0\n"
                      << "Uso: " << argv[0] << " [opciones]\n\n"
                      << "Opciones:\n"
                      << "  -h, --help       Muestra este mensaje de ayuda y termina.\n"
                      << "  -v, --version    Muestra la versión de la aplicación.\n"
                      << "  --check-init     Verifica la inicialización de GLFW e ImGui y sale.\n";
            return 0;
        } else if (arg == "--version" || arg == "-v") {
            std::cout << "WallFinder 1.0.0 (C++20, Dear ImGui, GLFW, OpenGL3)\n";
            return 0;
        }
    }

    std::cout << "Iniciando WallFinder...\n";
    wallpaper::UIManager app;

    if (!app.init(1280, 800, "WallFinder")) {
        std::cerr << "Aviso: No fue posible abrir la ventana gráfica. Compruebe que un servidor Wayland o X11 esté activo.\n";
        return 1;
    }

    for (int i = 1; i < argc; ++i) {
        if (std::string_view(argv[i]) == "--check-init") {
            std::cout << "Inicialización gráfica verificada con éxito.\n";
            app.shutdown();
            return 0;
        }
    }

    std::cout << "Ventana iniciada correctamente. Ejecutando bucle principal a 60 FPS...\n";
    app.run();
    app.shutdown();

    std::cout << "Aplicación cerrada correctamente.\n";
    return 0;
}
