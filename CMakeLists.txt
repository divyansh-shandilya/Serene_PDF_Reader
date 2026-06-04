cmake_minimum_required(VERSION 3.21)

project(SereneReaderNative VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- Qt Path ----
# Note: Keeping your specific Qt path here as requested
set(CMAKE_PREFIX_PATH "C:/Qt/6.11.0/msvc2022_64")

# ---- Qt ----
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Qml
        
    Sql
    QuickControls2
    Widgets
    Concurrent
)

qt_policy(SET QTP0001 OLD)
qt_standard_project_setup()

# ---- MuPDF (manual linking via vcpkg) ----
set(MUPDF_INCLUDE_DIR "C:/Users/divya/vcpkg/installed/x64-windows/include")
set(MUPDF_LIB_DIR "C:/Users/divya/vcpkg/installed/x64-windows/lib")

# ---- Sources ----
set(PROJECT_SOURCES
    src/main.cpp
    src/engine/MuPDFEngine.cpp
    src/engine/MuPDFDocument.cpp
    src/engine/RenderWorker.cpp
    src/ui/PdfQuickItem.cpp
    src/models/LibraryModel.cpp
)

set(PROJECT_HEADERS
    src/engine/MuPDFEngine.h
    src/engine/MuPDFDocument.h
    src/engine/RenderWorker.h
    src/ui/PdfQuickItem.h
    src/models/LibraryModel.h
)

# ---- Executable ----
if(WIN32)
    set(RESOURCE_FILES "resources.rc")
else()
    set(RESOURCE_FILES "")
endif()

qt_add_executable(SereneReaderNative
    ${PROJECT_SOURCES}
    ${PROJECT_HEADERS}
    ${RESOURCE_FILES}
)

# ---- QML ----
qt_add_qml_module(SereneReaderNative
    URI SereneReader
    VERSION 1.0
    QML_FILES
        qml/main.qml
        qml/SplashScreen.qml
        qml/LibraryView.qml
        qml/ReaderView.qml
        qml/FloatingToolbar.qml
        qml/SettingsDialog.qml
        qml/VerticalZoomSlider.qml
        qml/Colors.qml
        qml/FilterChip.qml
        qml/IconButton.qml
        qml/LibraryCard.qml
        qml/SearchBar.qml
        qml/Sidebar.qml
        qml/Spacing.qml
        qml/Theme.qml
        qml/TopBar.qml
        qml/Typography.qml
        qml/logo.svg
    RESOURCES
        app_icon.ico
)

# ---- Include dirs ----
target_include_directories(SereneReaderNative PRIVATE ${MUPDF_INCLUDE_DIR})

# ---- Link dirs ----
target_link_directories(SereneReaderNative PRIVATE ${MUPDF_LIB_DIR})

# ---- Linking ----
target_link_libraries(SereneReaderNative PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::Sql
    Qt6::QuickControls2
    Qt6::Widgets
    Qt6::Concurrent

    libmupdf

    freetype
    harfbuzz
    libpng16
    z
    jpeg

    openjp2
    jbig2dec
    gumbo

    dwmapi
)

# ---- Windows ----
if(WIN32)
    target_link_libraries(SereneReaderNative PRIVATE dwmapi)
endif()

set_target_properties(SereneReaderNative PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
)

# ---- Benchmark executable ----
qt_add_executable(RenderBench
    src/bench/RenderBench.cpp
    src/engine/MuPDFDocument.cpp
)

target_include_directories(RenderBench PRIVATE ${MUPDF_INCLUDE_DIR})
target_link_directories(RenderBench PRIVATE ${MUPDF_LIB_DIR})

target_link_libraries(RenderBench PRIVATE
    Qt6::Core
    Qt6::Gui
    libmupdf
    freetype
    harfbuzz
    libpng16
    z
    jpeg
    openjp2
    jbig2dec
    gumbo
)

set_target_properties(RenderBench PROPERTIES
    WIN32_EXECUTABLE OFF
)
