cmake_minimum_required(VERSION 3.14)
project(OpenStory VERSION 0.1.0)

include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "Minimum supported macOS version" FORCE)
endif()

if(APPLE)
    option(OPENSTORY_AUDIO "Enable BASS audio" OFF)
    option(OPENSTORY_FETCH_MACOS_DEPS "Build pinned macOS dependencies from source" ON)
else()
    option(OPENSTORY_AUDIO "Enable BASS audio" ON)
endif()

if(BUILD_TESTING)
    add_executable(OpenStoryNetTests
        tests/NetTests.cpp
        src/Net/Cryptography.cpp
        src/Net/InPacket.cpp)
    target_include_directories(OpenStoryNetTests PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/vendor/NoLifeNx)
    add_test(NAME OpenStoryNetTests COMMAND OpenStoryNetTests)

    add_executable(OpenStoryEquipJobTests tests/EquipJobTests.cpp)
    target_include_directories(OpenStoryEquipJobTests PRIVATE ${CMAKE_SOURCE_DIR}/src)
    add_test(NAME OpenStoryEquipJobTests COMMAND OpenStoryEquipJobTests)
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
    set(OPENSTORY_ICON ${CMAKE_CURRENT_BINARY_DIR}/OpenStory.icns)
    add_custom_command(
        OUTPUT ${OPENSTORY_ICON}
        COMMAND /usr/bin/sips -s format icns ${CMAKE_SOURCE_DIR}/resource/Icon.ico --out ${OPENSTORY_ICON}
        DEPENDS ${CMAKE_SOURCE_DIR}/resource/Icon.ico
        VERBATIM)
    set_source_files_properties(${OPENSTORY_ICON} PROPERTIES
        GENERATED TRUE
        MACOSX_PACKAGE_LOCATION Resources)
    target_sources(OpenStory PRIVATE ${OPENSTORY_ICON})

    set_target_properties(OpenStory PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER com.openstory.client
        MACOSX_BUNDLE_BUNDLE_NAME OpenStory
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
        MACOSX_BUNDLE_ICON_FILE OpenStory.icns)

    if(OPENSTORY_FETCH_MACOS_DEPS)
        include(FetchContent)

        set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
        set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static dependencies" FORCE)
        set(FT_DISABLE_ZLIB ON CACHE BOOL "Use FreeType's built-in zlib" FORCE)
        set(FT_DISABLE_BZIP2 ON CACHE BOOL "Disable FreeType bzip2 support" FORCE)
        set(FT_DISABLE_PNG ON CACHE BOOL "Disable FreeType PNG support" FORCE)
        set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "Disable FreeType HarfBuzz support" FORCE)
        set(FT_DISABLE_BROTLI ON CACHE BOOL "Disable FreeType Brotli support" FORCE)
        set(SKIP_INSTALL_ALL ON CACHE BOOL "Skip dependency install targets" FORCE)
        set(glew-cmake_BUILD_SHARED OFF CACHE BOOL "Disable shared GLEW" FORCE)
        set(glew-cmake_BUILD_STATIC ON CACHE BOOL "Build static GLEW" FORCE)
        set(ONLY_LIBS ON CACHE BOOL "Build only GLEW libraries" FORCE)
        set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "Disable GLFW examples" FORCE)
        set(GLFW_BUILD_TESTS OFF CACHE BOOL "Disable GLFW tests" FORCE)
        set(GLFW_BUILD_DOCS OFF CACHE BOOL "Disable GLFW docs" FORCE)
        set(GLFW_INSTALL OFF CACHE BOOL "Disable GLFW install targets" FORCE)

        FetchContent_Declare(freetype
            GIT_REPOSITORY https://github.com/freetype/freetype.git
            GIT_TAG 42608f77f20749dd6ddc9e0536788eaad70ea4b5)
        FetchContent_Declare(glew
            GIT_REPOSITORY https://github.com/Perlmint/glew-cmake.git
            GIT_TAG f456deace7b408655109aaeff71421ef2d3858c6)
        FetchContent_Declare(glfw
            GIT_REPOSITORY https://github.com/glfw/glfw.git
            GIT_TAG 7b6aead9fb88b3623e3b3725ebb42670cbe4c579)
        FetchContent_Declare(asio
            GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
            GIT_TAG 12e0ce9e0500bf0f247dbd1ae894272656456079)
        FetchContent_MakeAvailable(freetype glew glfw asio)

        # AGL was removed from modern macOS SDKs and GLEW does not use it when
        # built as a library.
        set_property(TARGET libglew_static PROPERTY INTERFACE_LINK_LIBRARIES OpenGL::GL)

        set(OPENSTORY_FREETYPE_TARGET freetype)
        set(OPENSTORY_GLEW_TARGET libglew_static)
        set(ASIO_INCLUDE_DIR ${asio_SOURCE_DIR}/asio/include)
    else()
        find_package(Freetype REQUIRED)
        find_package(GLEW REQUIRED)
        find_package(glfw3 CONFIG REQUIRED)
        find_path(ASIO_INCLUDE_DIR asio.hpp REQUIRED)

        set(OPENSTORY_FREETYPE_TARGET Freetype::Freetype)
        set(OPENSTORY_GLEW_TARGET GLEW::GLEW)
    endif()

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
        ${OPENSTORY_FREETYPE_TARGET}
        ${OPENSTORY_GLEW_TARGET}
        glfw
        ${COREGRAPHICS_FRAMEWORK})

    add_custom_command(TARGET OpenStory POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DAPP_BUNDLE=$<TARGET_BUNDLE_DIR:OpenStory>
            -P ${CMAKE_SOURCE_DIR}/cmake/FixupMacOSBundle.cmake
        COMMENT "Embedding dependencies and ad-hoc signing OpenStory.app"
        VERBATIM)
endif()
