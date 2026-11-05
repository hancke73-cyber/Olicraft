cmake_minimum_required(VERSION 3.16)
project(OliCraft VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  # Statische Laufzeit: das Spiel laeuft ohne installierte Visual-C++-Redistributables
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ---------------------------------------------------------------------------
# Abhaengigkeiten (werden beim ersten Konfigurieren per git geladen)
# ---------------------------------------------------------------------------
include(FetchContent)

set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)
if(UNIX AND NOT APPLE)
  # X11 reicht (auch unter Wayland via XWayland), spart zusaetzliche Systempakete.
  set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
endif()

FetchContent_Declare(glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG        3.4
  GIT_SHALLOW    TRUE)
FetchContent_Declare(glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG        1.0.1
  GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(glfw glm)

# GLAD (OpenGL 3.3 Core, mit glad2 erzeugt, liegt im Repo)
add_library(glad STATIC third_party/glad/src/gl.c)
target_include_directories(glad PUBLIC third_party/glad/include)

# ---------------------------------------------------------------------------
# Spiel
# ---------------------------------------------------------------------------
add_executable(OliCraft
  src/main.cpp
  src/Game.cpp
  src/Block.cpp
  src/Noise.cpp
  src/Atlas.cpp
  src/Shader.cpp
  src/Chunk.cpp
  src/Mesher.cpp
  src/TerrainGen.cpp
  src/World.cpp
  src/Player.cpp
  src/Physics.cpp
  src/Entities.cpp
  src/Models.cpp
  src/Camera.cpp
  src/Image.cpp
  src/Audio.cpp
  src/Sounds.cpp
  src/Sky.cpp
  src/Village.cpp
  src/Trades.cpp
  src/Particles.cpp src/Worlds.cpp src/Machines.cpp
  src/TextureStore.cpp
  src/TextureEditor.cpp
  src/EditorUi.cpp
  src/Skins.cpp
  src/Items.cpp
  src/Crafting.cpp
  src/Inventory.cpp
  src/Renderer.cpp
  src/Hud.cpp
  src/GuiTexture.cpp
  src/Settings.cpp
  src/Menu.cpp
  src/Save.cpp)

target_include_directories(OliCraft PRIVATE src third_party/stb third_party/miniaudio)
find_package(Threads REQUIRED)
target_link_libraries(OliCraft PRIVATE glfw glad glm::glm Threads::Threads ${CMAKE_DL_LIBS})

if(MSVC)
  target_compile_options(OliCraft PRIVATE /W3 /utf-8 /permissive-)
  target_compile_definitions(OliCraft PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
  set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT OliCraft)
else()
  target_compile_options(OliCraft PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

if(MINGW)
  # Eigenstaendige .exe ohne MinGW-DLLs
  target_link_options(OliCraft PRIVATE -static -static-libgcc -static-libstdc++)
endif()

# ---------------------------------------------------------------------------
# Tests (ohne OpenGL-Kontext): cmake -DOLICRAFT_BUILD_TESTS=ON, dann ctest
# ---------------------------------------------------------------------------
option(OLICRAFT_BUILD_TESTS "Tests bauen" OFF)
if(OLICRAFT_BUILD_TESTS)
  enable_testing()
  add_executable(olicraft_tests
    tests/tests.cpp
    src/Block.cpp src/Noise.cpp src/Chunk.cpp src/Mesher.cpp src/TerrainGen.cpp
    src/World.cpp src/Player.cpp src/Physics.cpp src/Entities.cpp src/Models.cpp src/Camera.cpp src/Image.cpp src/Audio.cpp src/Sounds.cpp src/Sky.cpp src/Village.cpp src/Trades.cpp src/Particles.cpp src/Worlds.cpp src/Machines.cpp src/TextureStore.cpp src/TextureEditor.cpp src/EditorUi.cpp src/Skins.cpp src/Items.cpp src/Crafting.cpp src/Inventory.cpp src/Save.cpp src/Atlas.cpp src/Hud.cpp src/GuiTexture.cpp src/Settings.cpp src/Menu.cpp)
  target_include_directories(olicraft_tests PRIVATE src third_party/stb third_party/miniaudio)
  target_link_libraries(olicraft_tests PRIVATE glad glm::glm Threads::Threads ${CMAKE_DL_LIBS})
  add_test(NAME olicraft_tests COMMAND olicraft_tests)
endif()
