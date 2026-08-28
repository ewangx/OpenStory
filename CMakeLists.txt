cmake_minimum_required(VERSION 3.14)
project(OpenStory VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    option(OPENSTORY_AUDIO "Enable BASS audio" OFF)
else()
    option(OPENSTORY_AUDIO "Enable BASS audio" ON)
endif()

file(GLOB_RECURSE SOURCE_FILES
            "src/*.h"
            "src/*.cpp"
            "src/*/*.cpp"
            "src/*/*.h"
    )

if(OPENSTORY_AUDIO)
    list(REMOVE_ITEM SOURCE_FILES "${CMAKE_SOURCE_DIR}/src/Audio/AudioNull.cpp")
else()
    list(REMOVE_ITEM SOURCE_FILES "${CMAKE_SOURCE_DIR}/src/Audio/Audio.cpp")
endif()
# Embed the Windows app/window icon (Explorer exe icon + GLFW window icon).
if(WIN32)
    list(APPEND SOURCE_FILES "${CMAKE_SOURCE_DIR}/resource/OpenStory.rc")
endif(WIN32)

add_executable(OpenStory ${SOURCE_FILES})

set_target_properties(OpenStory PROPERTIES
                      RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/wz
                      RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_SOURCE_DIR}/wz
                      RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}/wz
)

# Emit a PDB for optimized Release builds so crash logs (CrashLog.cpp) resolve to
# real function names + line numbers instead of raw addresses.
if(MSVC)
    target_compile_options(OpenStory PRIVATE $<$<CONFIG:Release>:/Zi>)
    target_link_options(OpenStory PRIVATE $<$<CONFIG:Release>:/DEBUG>)
endif()

# ADD_CUSTOM_COMMAND(TARGET OpenStory POST_BUILD
#                 COMMAND copy \"${CMAKE_SOURCE_DIR}\vendor\bass24\x64\bass.dll\"  \"${RUNTIME_OUTPUT_DIRECTORY_DEBUG}/\"
#                 COMMENT "copying dlls …"
#                 )


find_package(OpenGL REQUIRED)
TARGET_INCLUDE_DIRECTORIES(OpenStory PUBLIC ${OPENGL_INCLUDE_DIRS})
target_link_libraries(OpenStory PRIVATE ${OPENGL_LIBRARIES})


if(WIN32)

    TARGET_INCLUDE_DIRECTORIES(OpenStory PUBLIC ${CMAKE_SOURCE_DIR}/vendor/NoLifeNx)
    TARGET_INCLUDE_DIRECTORIES(OpenStory PUBLIC ${CMAKE_SOURCE_DIR}/vendor/stb)
    TARGET_INCLUDE_DIRECTORIES(OpenStory PUBLIC ${CMAKE_SOURCE_DIR}/vendor/freetype/include)
    TARGET_INCLUDE_DIRECTORIES(OpenStory PUBLIC ${CMAKE_SOURCE_DIR}/vendor/glew-2.1.0/include)
    TARGET_INCLUDE_DIRECTORIES(OpenStory PUBLIC ${CMAKE_SOURCE_DIR}/vendor/bass24/c)
    TARGET_INCLUDE_DIRECTORIES(OpenStory PUBLIC ${CMAKE_SOURCE_DIR}/vendor/glfw-3.3.2.bin.WIN32/include)

    # Build NoLifeNx (and lz4) from source instead of the stale prebuilt
    # NoLifeNx.lib, so fixes to the bitmap/node code take effect on a normal
    # rebuild. lz4.c is compiled with MSVC here (the bundled liblz4_static.lib is
    # a MinGW build and won't link against MSVC).
    TARGET_INCLUDE_DIRECTORIES(OpenStory PUBLIC ${CMAKE_SOURCE_DIR}/vendor/NoLifeNx/nlnx/includes/lz4_v1_8_2_win64/include)
    target_sources(OpenStory PRIVATE
        ${CMAKE_SOURCE_DIR}/vendor/NoLifeNx/nlnx/audio.cpp
        ${CMAKE_SOURCE_DIR}/vendor/NoLifeNx/nlnx/bitmap.cpp
        ${CMAKE_SOURCE_DIR}/vendor/NoLifeNx/nlnx/file.cpp
        ${CMAKE_SOURCE_DIR}/vendor/NoLifeNx/nlnx/node.cpp
        ${CMAKE_SOURCE_DIR}/vendor/NoLifeNx/nlnx/nx.cpp
        ${CMAKE_SOURCE_DIR}/vendor/NoLifeNx/nlnx/includes/lz4_v1_8_2_win64/lz4.c)

    target_link_libraries(OpenStory PRIVATE ${CMAKE_SOURCE_DIR}/vendor/freetype/win64/freetype.lib)
    target_link_libraries(OpenStory PRIVATE ${CMAKE_SOURCE_DIR}/vendor/bass24/c/x64/bass.lib)
    target_link_libraries(OpenStory PRIVATE ${CMAKE_SOURCE_DIR}/vendor/glew-2.1.0/lib/Release/x64/glew32s.lib)
    target_link_libraries(OpenStory PRIVATE ${CMAKE_SOURCE_DIR}/vendor/glfw-3.3.2.bin.WIN64/lib-vc2019/glfw3.lib)
    target_link_libraries(OpenStory PRIVATE Iphlpapi.lib)
endif(WIN32)

if(APPLE)
    set_target_properties(OpenStory PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER com.openstory.client
        MACOSX_BUNDLE_BUNDLE_NAME OpenStory
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION})

    find_package(Freetype REQUIRED)
    find_package(GLEW REQUIRED)
    find_package(glfw3 CONFIG REQUIRED)
    find_path(ASIO_INCLUDE_DIR asio.hpp REQUIRED)
    find_library(COREGRAPHICS_FRAMEWORK CoreGraphics REQUIRED)

    add_library(NoLifeNx STATIC
        ${CMAKE_SOURCE_DIR}/vendor/NoLifeNx/nlnx/audio.cpp
        ${CMAKE_SOURCE_DIR}/vendor/NoLifeNx/nlnx/bitmap.cpp
        ${CMAKE_SOURCE_DIR}/vendor/NoLifeNx/nlnx/file.cpp
        ${CMAKE_SOURCE_DIR}/vendor/NoLifeNx/nlnx/node.cpp
        ${CMAKE_SOURCE_DIR}/vendor/NoLifeNx/nlnx/nx.cpp
        ${CMAKE_SOURCE_DIR}/vendor/NoLifeNx/nlnx/includes/lz4_v1_8_2_win64/lz4.c)
    target_include_directories(NoLifeNx PUBLIC
        ${CMAKE_SOURCE_DIR}/vendor/NoLifeNx
        ${CMAKE_SOURCE_DIR}/vendor/NoLifeNx/nlnx/includes/lz4_v1_8_2_win64/include)

    target_compile_definitions(OpenStory PRIVATE PLATFORM_MACOS=1 USE_ASIO=1)
    target_include_directories(OpenStory PRIVATE
        ${CMAKE_SOURCE_DIR}/vendor/stb
        ${ASIO_INCLUDE_DIR})
    target_link_libraries(OpenStory PRIVATE
        NoLifeNx
        Freetype::Freetype
        GLEW::GLEW
        glfw
        ${COREGRAPHICS_FRAMEWORK})

    add_custom_command(TARGET OpenStory POST_BUILD
        COMMAND /usr/bin/codesign --force --deep --sign - $<TARGET_BUNDLE_DIR:OpenStory>
        COMMENT "Ad-hoc signing OpenStory.app")
endif()
