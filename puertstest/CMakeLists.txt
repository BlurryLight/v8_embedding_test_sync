cmake_minimum_required(VERSION 3.16)

project(Helloworld)

set (CMAKE_CXX_STANDARD 14)
set (CMAKE_CXX_STANDARD_REQUIRED ON)
set (CMAKE_CXX_EXTENSIONS OFF)

set (CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(JS_ENGINE_ROOT ${CMAKE_SOURCE_DIR}/../v8)

if(MSVC)
    foreach(flag_var
        CMAKE_C_FLAGS_DEBUG
        CMAKE_C_FLAGS_RELEASE
        CMAKE_C_FLAGS_RELWITHDEBINFO
        CMAKE_C_FLAGS_MINSIZEREL
        CMAKE_CXX_FLAGS_DEBUG
        CMAKE_CXX_FLAGS_RELEASE
        CMAKE_CXX_FLAGS_RELWITHDEBINFO
        CMAKE_CXX_FLAGS_MINSIZEREL
    )
        string(REPLACE "/MTd" "" ${flag_var} "${${flag_var}}")
        string(REPLACE "/MT" "" ${flag_var} "${${flag_var}}")
    endforeach()

    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

    if(NOT DEFINED CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES)
        get_filename_component(MSVC_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
        get_filename_component(MSVC_HOST_DIR "${MSVC_BIN_DIR}" DIRECTORY)
        get_filename_component(MSVC_TOOLS_BIN_DIR "${MSVC_HOST_DIR}" DIRECTORY)
        get_filename_component(MSVC_TOOLS_ROOT "${MSVC_TOOLS_BIN_DIR}" DIRECTORY)
        get_filename_component(MSVC_VERSION_ROOT "${MSVC_TOOLS_ROOT}" DIRECTORY)
        get_filename_component(MSVC_VC_ROOT "${MSVC_VERSION_ROOT}" DIRECTORY)
        set(MSVC_INCLUDE_DIR "${MSVC_TOOLS_ROOT}/include")
        set(MSVC_AUX_INCLUDE_DIR "${MSVC_VC_ROOT}/Auxiliary/VS/include")
        set(MSVC_LIB_DIR "${MSVC_TOOLS_ROOT}/lib/x64")

        get_filename_component(WINSDK_RC_DIR "${CMAKE_RC_COMPILER}" DIRECTORY)
        get_filename_component(WINSDK_VER_DIR "${WINSDK_RC_DIR}" DIRECTORY)
        get_filename_component(WINSDK_VERSION "${WINSDK_VER_DIR}" NAME)
        get_filename_component(WINSDK_BIN_DIR "${WINSDK_VER_DIR}" DIRECTORY)
        get_filename_component(WINSDK_ROOT "${WINSDK_BIN_DIR}" DIRECTORY)
        set(WINSDK_INCLUDE_ROOT "${WINSDK_ROOT}/include/${WINSDK_VERSION}")
        set(WINSDK_LIB_ROOT "${WINSDK_ROOT}/lib/${WINSDK_VERSION}")

        set(CMAKE_C_STANDARD_INCLUDE_DIRECTORIES
            "${MSVC_INCLUDE_DIR}"
            "${MSVC_AUX_INCLUDE_DIR}"
            "${WINSDK_INCLUDE_ROOT}/ucrt"
            "${WINSDK_INCLUDE_ROOT}/um"
            "${WINSDK_INCLUDE_ROOT}/shared"
            "${WINSDK_INCLUDE_ROOT}/winrt"
            "${WINSDK_INCLUDE_ROOT}/cppwinrt"
        )
        set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES ${CMAKE_C_STANDARD_INCLUDE_DIRECTORIES})
        link_directories(
            "${MSVC_LIB_DIR}"
            "${WINSDK_LIB_ROOT}/ucrt/x64"
            "${WINSDK_LIB_ROOT}/um/x64"
        )
    endif()
endif()

include_directories(
    ${JS_ENGINE_ROOT}/Inc
    puerts/include
)

set ( SRC hello-world.cc 
          puerts/src/CppObjectMapper.cpp
          puerts/src/DataTransfer.cpp
          puerts/src/JSClassRegister.cpp
          )

add_executable(helloworld ${SRC})

set (LIB_NAME wee8)

if ( ANDROID )
    find_library(log-lib log )

    if( ANDROID_ABI STREQUAL "armeabi-v7a")
        target_link_libraries(helloworld
            ${JS_ENGINE_ROOT}/Lib/Android/armv7/lib${LIB_NAME}.a
            ${log-lib}
        )
        target_compile_definitions (helloworld PRIVATE PLATFORM_ANDROID_ARM)
    else ( )
        target_link_libraries(helloworld
            ${JS_ENGINE_ROOT}/Lib/Android/arm64/lib${LIB_NAME}.a
            ${log-lib}
        )
        target_compile_definitions (helloworld PRIVATE PLATFORM_ANDROID_ARM64)
    endif ( )
elseif ( APPLE )
    if (IOS)
        target_link_libraries(helloworld
            ${JS_ENGINE_ROOT}/Lib/iOS/lib${LIB_NAME}.a
        )
        target_compile_definitions (helloworld PRIVATE PLATFORM_IOS)
    else ()
        target_link_libraries(helloworld
            ${JS_ENGINE_ROOT}/Lib/macOS/lib${LIB_NAME}.a
        )
        target_compile_definitions (helloworld PRIVATE PLATFORM_MAC)
    endif ( )
elseif (UNIX)
    target_link_libraries(helloworld
        ${JS_ENGINE_ROOT}/Lib/Linux/lib${LIB_NAME}.a
	    pthread
    )
    target_compile_definitions (helloworld PRIVATE PLATFORM_LINUX)
    set(CMAKE_CXX_COMPILER "clang++")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++ -lc++abi")
else ()
    if ( JS_ENGINE STREQUAL "v8")
        set(LIBS_NEEDED winmm.lib dbghelp.lib shlwapi.lib)
    endif()
    if("${CMAKE_SIZEOF_VOID_P}" STREQUAL "4")
        target_link_libraries(hello-world
            ${JS_ENGINE_ROOT}/Lib/Win32/${LIB_NAME}.lib
            ${LIBS_NEEDED}
        )
    else ()
        target_link_libraries(helloworld
            ${JS_ENGINE_ROOT}/Lib/Win64/${LIB_NAME}.lib
            ${LIBS_NEEDED}
        )
    endif ()
    target_compile_definitions (helloworld PRIVATE PLATFORM_WINDOWS)
endif ( )
