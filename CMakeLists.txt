cmake_minimum_required(VERSION 3.24)

if(CMAKE_GENERATOR STREQUAL "Ninja" AND NOT CMAKE_MAKE_PROGRAM)
    find_program(PMX_NINJA_EXECUTABLE
        NAMES ninja ninja-build
        PATHS
            "/home/oscarc/.local/bin"
            "/home/oscarc/.local/share/flatpak/runtime/org.kde.Sdk/x86_64/6.11/305b8ec8d506448558798256bd9afcfd856c7e62357a42e320a6678a08f81465/files/bin"
            "/home/oscarc/.local/share/flatpak/runtime/org.freedesktop.Sdk/x86_64/26.08/3311c13c67b602315c39cf5310de129dd3460d04b6c8cf5be1b2cbc42c51abd7/files/bin"
    )
    if(PMX_NINJA_EXECUTABLE)
        set(CMAKE_MAKE_PROGRAM "${PMX_NINJA_EXECUTABLE}" CACHE FILEPATH "Ninja build program" FORCE)
    endif()
endif()

project(PmxEditor VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

option(PMX_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer in Debug builds" ON)
if(PMX_ENABLE_SANITIZERS AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

set(QT_NO_PRIVATE_MODULE_WARNING ON)
if(COMMAND qt_policy)
    qt_policy(SET QTP0001 NEW)
    qt_policy(SET QTP0004 NEW)
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick QuickControls2 Network Test GuiPrivate ShaderTools)

find_package(glm QUIET)
if(NOT TARGET glm::glm)
    if(TARGET glm)
        add_library(glm::glm ALIAS glm)
    else()
        find_path(GLM_INCLUDE_DIR NAMES glm/glm.hpp)
        if(NOT GLM_INCLUDE_DIR)
            message(FATAL_ERROR "glm headers (glm/glm.hpp) not found")
        endif()
        add_library(glm_headers INTERFACE)
        target_include_directories(glm_headers INTERFACE "${GLM_INCLUDE_DIR}")
        add_library(glm::glm ALIAS glm_headers)
    endif()
endif()

add_library(PmxCore STATIC
    src/core/PmxTypes.h
    src/core/PmxDocument.h
    src/core/PmxReader.h
    src/core/PmxReader.cpp
    src/core/PmxWriter.h
    src/core/PmxWriter.cpp
    src/core/PmdImporter.h
    src/core/PmdImporter.cpp
    src/core/PmxMerger.h
    src/core/PmxMerger.cpp
)

target_include_directories(PmxCore PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core"
)

target_link_libraries(PmxCore PUBLIC
    Qt6::Core
    glm::glm
)

add_library(PmxModels STATIC
    src/models/PmxSelectionManager.h
    src/models/PmxSelectionManager.cpp
    src/models/PmxUndoCommands.h
    src/models/PmxUndoCommands.cpp
    src/models/PmxDocumentController.h
    src/models/PmxDocumentController.cpp
    src/models/PmxInfoModel.h
    src/models/PmxInfoModel.cpp
    src/models/PmxMaterialTableModel.h
    src/models/PmxMaterialTableModel.cpp
    src/models/PmxBoneTableModel.h
    src/models/PmxBoneTableModel.cpp
    src/models/PmxVertexTableModel.h
    src/models/PmxVertexTableModel.cpp
    src/models/PmxMorphTableModel.h
    src/models/PmxMorphTableModel.cpp
    src/models/PmxDisplayFrameModel.h
    src/models/PmxDisplayFrameModel.cpp
    src/models/PmxRigidBodyTableModel.h
    src/models/PmxRigidBodyTableModel.cpp
    src/models/PmxJointTableModel.h
    src/models/PmxJointTableModel.cpp
)

target_include_directories(PmxModels PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/models"
)

target_link_libraries(PmxModels PUBLIC
    PmxCore
    Qt6::Core
    Qt6::Gui
)

add_library(PmxViewport STATIC
    src/viewport/PmxCamera.h
    src/viewport/PmxCamera.cpp
    src/viewport/PmxViewportItem.h
    src/viewport/PmxViewportItem.cpp
)

target_include_directories(PmxViewport PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/viewport"
)

target_link_libraries(PmxViewport PUBLIC
    PmxCore
    PmxModels
    Qt6::Core
    Qt6::Gui
    Qt6::GuiPrivate
    Qt6::Quick
)

qt_add_shaders(PmxViewport "pmx_shaders"
    PREFIX
        "/shaders"
    BASE
        "src/viewport/shaders"
    FILES
        src/viewport/shaders/mesh.vert
        src/viewport/shaders/mesh.frag
        src/viewport/shaders/lines.vert
        src/viewport/shaders/lines.frag
)

add_executable(pmx-editor
    src/app/main.cpp
    src/app/PmxAppBridge.h
    src/app/PmxAppBridge.cpp
)

target_include_directories(pmx-editor PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/app"
)

if(COMMAND qt_policy)
    qt_policy(SET QTP0004 NEW)
endif()

qt_add_qml_module(pmx-editor
    URI PmxEditorApp
    VERSION 1.0
    RESOURCE_PREFIX "/qt/qml"
    QML_FILES
        src/app/qml/Main.qml
        src/app/qml/InspectorPanel.qml
        src/app/qml/BatchOperationDialog.qml
        src/app/qml/tabs/InfoTab.qml
        src/app/qml/tabs/VertexTab.qml
        src/app/qml/tabs/MaterialTab.qml
        src/app/qml/tabs/BoneTab.qml
        src/app/qml/tabs/MorphTab.qml
        src/app/qml/tabs/DisplayFrameTab.qml
        src/app/qml/tabs/RigidBodyTab.qml
        src/app/qml/tabs/JointTab.qml
)

target_link_libraries(pmx-editor PRIVATE
    PmxViewport
    PmxModels
    PmxCore
    Qt6::QuickControls2
)

add_library(PmxMcpServer STATIC
    src/mcp/PmxMcpServer.h
    src/mcp/PmxMcpServer.cpp
)

target_include_directories(PmxMcpServer PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/mcp"
)

target_link_libraries(PmxMcpServer PUBLIC
    PmxCore
    PmxModels
    Qt6::Core
    Qt6::Network
)

add_executable(pmx-mcp
    src/mcp/main_mcp.cpp
)

target_include_directories(pmx-mcp PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/mcp"
)

target_link_libraries(pmx-mcp PRIVATE
    PmxMcpServer
    PmxModels
    PmxCore
)

enable_testing()

add_executable(tst_pmx_roundtrip
    tests/tst_pmx_roundtrip.cpp
)

set_target_properties(tst_pmx_roundtrip PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
)

target_link_libraries(tst_pmx_roundtrip PRIVATE
    PmxCore
    Qt6::Test
)

add_test(NAME tst_pmx_roundtrip COMMAND tst_pmx_roundtrip)

add_executable(tst_pmx_undo_models
    tests/tst_pmx_undo_models.cpp
)

set_target_properties(tst_pmx_undo_models PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
)

target_link_libraries(tst_pmx_undo_models PRIVATE
    PmxModels
    Qt6::Test
)

add_test(NAME tst_pmx_undo_models COMMAND tst_pmx_undo_models)

add_executable(tst_pmx_viewport
    tests/tst_pmx_viewport.cpp
    src/app/PmxAppBridge.cpp
)

set_target_properties(tst_pmx_viewport PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
)

target_link_libraries(tst_pmx_viewport PRIVATE
    PmxViewport
    PmxModels
    PmxCore
    Qt6::Test
    Qt6::Quick
)

add_test(NAME tst_pmx_viewport COMMAND tst_pmx_viewport)

add_executable(tst_pmx_mcp
    tests/tst_pmx_mcp.cpp
)

set_target_properties(tst_pmx_mcp PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
)

target_link_libraries(tst_pmx_mcp PRIVATE
    PmxMcpServer
    PmxModels
    PmxCore
    Qt6::Test
)

add_test(NAME tst_pmx_mcp COMMAND tst_pmx_mcp)

add_executable(tst_pmd_import
    tests/tst_pmd_import.cpp
)

set_target_properties(tst_pmd_import PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
)

target_link_libraries(tst_pmd_import PRIVATE
    PmxCore
    Qt6::Test
)

add_test(NAME tst_pmd_import COMMAND tst_pmd_import)

add_executable(tst_pmx_merger
    tests/tst_pmx_merger.cpp
)

set_target_properties(tst_pmx_merger PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
)

target_link_libraries(tst_pmx_merger PRIVATE
    PmxCore
    PmxModels
    Qt6::Test
)

add_test(NAME tst_pmx_merger COMMAND tst_pmx_merger)
