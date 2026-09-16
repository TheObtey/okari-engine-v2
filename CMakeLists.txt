cmake_minimum_required(VERSION 3.20)

project(OkariEngine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(External/glfw)

find_package(OpenGL REQUIRED)

# =========================
# SOURCES
# =========================

set(J3D_SOURCES
    Engine/IO/BigEndianReader.cpp

    Engine/Formats/J3D/J3DFileReader.cpp
    Engine/Formats/J3D/J3DStringTable.cpp
    Engine/Formats/J3D/J3DPoseEvaluator.cpp
    Engine/Formats/J3D/J3DDrawMatrixEvaluator.cpp
    Engine/Formats/J3D/J3DShapeMatrixPaletteResolver.cpp
    Engine/Formats/J3D/J3DVertexDecoder.cpp
    Engine/Formats/J3D/J3DShapeDisplayListParser.cpp
    Engine/Formats/J3D/J3DShapeVertexIndexScanner.cpp
    Engine/Formats/J3D/J3DShapeVertexReferenceDecoder.cpp
    Engine/Formats/J3D/J3DGeometryAssembler.cpp
    Engine/Formats/J3D/J3DTriangleTopologyBuilder.cpp

    Engine/Formats/J3D/Sections/INF1Parser.cpp
    Engine/Formats/J3D/Sections/JNT1Parser.cpp
    Engine/Formats/J3D/Sections/DRW1Parser.cpp
    Engine/Formats/J3D/Sections/EVP1Parser.cpp
    Engine/Formats/J3D/Sections/VTX1Parser.cpp
    Engine/Formats/J3D/Sections/SHP1Parser.cpp
)

set(ENGINE_SOURCES
    External/glad/src/glad.c
    External/ufbx/ufbx.c

    ${J3D_SOURCES}

    Engine/Core/Application.cpp
    Engine/Core/Time.cpp

    Engine/Platform/Window.cpp

    Engine/Input/Input.cpp
    Engine/Input/InputContext.cpp
    Engine/Input/InputManager.cpp

    Engine/Rendering/Shader.cpp
    Engine/Rendering/Renderer.cpp
    Engine/Rendering/Camera.cpp
    Engine/Rendering/Texture2D.cpp
    Engine/Rendering/Framebuffer.cpp
    Engine/Rendering/Mesh.cpp
    Engine/Rendering/GX/GXMaterialBinder.cpp

    Engine/Physics/PhysicsSystem.cpp

    Engine/Collision/CollisionWorld.cpp

    Engine/Resources/MeshManager.cpp
    Engine/Resources/FBXLoader.cpp
    Engine/Resources/OKMATLoader.cpp
    Engine/Resources/OKCOLLoader.cpp
)

set(EDITOR_SOURCES
    Engine/Editor/EditorLayer.cpp
    Engine/Editor/EditorUI.cpp
    Engine/Editor/EditorCameraController.cpp

    Engine/Editor/Panels/HierarchyPanel.cpp
    Engine/Editor/Panels/InspectorPanel.cpp
    Engine/Editor/Panels/ViewportPanel.cpp
    Engine/Editor/Panels/AssetBrowserPanel.cpp
)

set(IMGUI_SOURCES
    External/imgui/imgui.cpp
    External/imgui/imgui_draw.cpp
    External/imgui/imgui_tables.cpp
    External/imgui/imgui_widgets.cpp
    External/imgui/backends/imgui_impl_glfw.cpp
    External/imgui/backends/imgui_impl_opengl3.cpp
)

set(COMMON_INCLUDES
    ${CMAKE_SOURCE_DIR}/Engine
    ${CMAKE_SOURCE_DIR}/Game

    ${CMAKE_SOURCE_DIR}/External/glfw/include
    ${CMAKE_SOURCE_DIR}/External/glad/include
    ${CMAKE_SOURCE_DIR}/External/glm
    ${CMAKE_SOURCE_DIR}/External/nlohmann
    ${CMAKE_SOURCE_DIR}/External/stb
    ${CMAKE_SOURCE_DIR}/External/ufbx
)

# =========================
# J3D INSPECTOR
# =========================

add_executable(J3DInspector
    Tools/J3DInspector/main.cpp
    ${J3D_SOURCES}
)

target_include_directories(J3DInspector PRIVATE
    ${CMAKE_SOURCE_DIR}/Engine
    ${CMAKE_SOURCE_DIR}/External/glm
)

# =========================
# SANDBOX
# =========================

add_executable(Sandbox
    Sandbox/main.cpp
    ${ENGINE_SOURCES}

    Game/GameLayer.cpp
    Game/World/World.cpp
    Game/Actors/PlayerActor.cpp
)

target_include_directories(Sandbox PRIVATE
    ${COMMON_INCLUDES}
)

target_link_libraries(Sandbox PRIVATE
    glfw
    OpenGL::GL
)

target_compile_definitions(Sandbox PRIVATE
    OKARI_ASSET_DIR="${CMAKE_SOURCE_DIR}/Assets"
)

# =========================
# EDITOR
# =========================

add_executable(OkariEditor
    Editor/main.cpp
    ${ENGINE_SOURCES}
    ${EDITOR_SOURCES}
    ${IMGUI_SOURCES}

    Game/World/World.cpp
    Game/Actors/ActorRegistry.cpp
    Game/Actors/ActorFactory.cpp
    Game/Actors/PlayerActor.cpp
    Game/Actors/DoorActor.cpp
)

target_include_directories(OkariEditor PRIVATE
    ${COMMON_INCLUDES}

    ${CMAKE_SOURCE_DIR}/External/imgui
    ${CMAKE_SOURCE_DIR}/External/imgui/backends
)

target_link_libraries(OkariEditor PRIVATE
    glfw
    OpenGL::GL
)

target_compile_definitions(OkariEditor PRIVATE
    OKARI_ASSET_DIR="${CMAKE_SOURCE_DIR}/Assets"
)

# =========================
# COPY ASSETS
# =========================

add_custom_command(TARGET Sandbox POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/Assets
    $<TARGET_FILE_DIR:Sandbox>/Assets
)

add_custom_command(TARGET OkariEditor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/Assets
    $<TARGET_FILE_DIR:OkariEditor>/Assets
)