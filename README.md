# Serene Reader Native - Build Guide

This project is a high-performance Windows PDF Reader built with **C++20**, **Qt 6**, and **MuPDF**.

## Prerequisites

1.  **Visual Studio 2022**: Install with "Desktop development with C++".
2.  **Qt 6.x**: Install via the Qt Online Installer. Ensure `Qt QML`, `Qt Quick`, and `Qt SQL` are selected.
3.  **CMake 3.20+**.
4.  **MuPDF Source/Binaries**:
    - Download MuPDF source from mupdf.com.
    - Compile it for Windows (using the provided `.sln` or `make`).
    - Note the location of the `include` folder and the `build/release` library folder.

## Build Steps

1.  **Configure environment**:
    Ensure `QT_DIR` is in your environment variables or path.
2.  **Open in IDE**:
    Open the root `CMakeLists.txt` in Visual Studio or Qt Creator.
3.  **Link MuPDF**:
    Update the `CMakeLists.txt` with your MuPDF paths:
    ```cmake
    include_directories("C:/path/to/mupdf/include")
    link_directories("C:/path/to/mupdf/platform/win32/x64/Release")
    ```
4.  **Generate & Build**:
    ```bash
    mkdir build
    cd build
    cmake ..
    cmake --build . --config Release
    ```

## Project Structure

- `src/engine/`: MuPDF wrapper and background renderer.
- `src/ui/`: QML Bridge (PdfQuickItem).
- `src/models/`: SQLite database persistence for bookmarks and favorites.
- `qml/`: Fluid UI components for the "Serene" experience.

## Performance Notes

- **RAM Target**: ~300MB.
- **Zero-Distraction**: Toggle UI elements using the toolbar "Focus" mode.
- **Hardware Acceleration**: QML uses the RHI (Rendering Hardware Interface) to leverage Direct3D on Windows.
