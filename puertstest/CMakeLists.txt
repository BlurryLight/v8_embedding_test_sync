cmake_minimum_required(VERSION 3.16)

project(Helloworld)

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)
set (CMAKE_CXX_EXTENSIONS OFF)

set (CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(JS_ENGINE_ROOT ${CMAKE_SOURCE_DIR}/../v8)
set(LIB_NAME wee8)

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
endif()

include_directories(
    ${JS_ENGINE_ROOT}/Inc
    puerts/include
)

set(JSENV_SRC
    hello-world-bindings.cc
    puerts/src/DeclarationGenerator.cpp
    puerts/src/CppObjectMapper.cpp
    puerts/src/DataTransfer.cpp
    puerts/src/JSClassRegister.cpp
    puerts/src/JsEnv.cpp
)

add_library(jsenv SHARED ${JSENV_SRC})
target_compile_definitions(jsenv PRIVATE JSENV_EXPORTS)
target_include_directories(jsenv PUBLIC
    ${JS_ENGINE_ROOT}/Inc
    puerts/include
)

add_executable(helloworld hello-world.cc)
target_link_libraries(helloworld PRIVATE jsenv)
target_include_directories(helloworld PRIVATE
    ${JS_ENGINE_ROOT}/Inc
    puerts/include
)

if ( ANDROID )
    find_library(log-lib log )

    if( ANDROID_ABI STREQUAL "armeabi-v7a")
        target_link_libraries(jsenv
            ${JS_ENGINE_ROOT}/Lib/Android/armv7/lib${LIB_NAME}.a
            ${log-lib}
        )
        target_compile_definitions (helloworld PRIVATE PLATFORM_ANDROID_ARM)
        target_compile_definitions (jsenv PRIVATE PLATFORM_ANDROID_ARM)
    else ( )
        target_link_libraries(jsenv
            ${JS_ENGINE_ROOT}/Lib/Android/arm64/lib${LIB_NAME}.a
            ${log-lib}
        )
        target_compile_definitions (helloworld PRIVATE PLATFORM_ANDROID_ARM64)
        target_compile_definitions (jsenv PRIVATE PLATFORM_ANDROID_ARM64)
    endif ( )
elseif ( APPLE )
    if (IOS)
        target_link_libraries(jsenv
            ${JS_ENGINE_ROOT}/Lib/iOS/lib${LIB_NAME}.a
        )
        target_compile_definitions (helloworld PRIVATE PLATFORM_IOS)
        target_compile_definitions (jsenv PRIVATE PLATFORM_IOS)
    else ()
        target_link_libraries(jsenv
            ${JS_ENGINE_ROOT}/Lib/macOS/lib${LIB_NAME}.a
        )
        target_compile_definitions (helloworld PRIVATE PLATFORM_MAC)
        target_compile_definitions (jsenv PRIVATE PLATFORM_MAC)
    endif ( )
elseif (UNIX)
    set(V8_STATIC_LIBRARY ${JS_ENGINE_ROOT}/Lib/Linux/lib${LIB_NAME}.a)
    set(V8_DYNAMIC_LIBRARY_DIR "")
    set(V8_DEBUG_DYNAMIC_LIBRARY_DIR "")
    set(V8_RELEASE_DYNAMIC_LIBRARY_DIR "")
    set(V8_USE_CONFIG_MATCHED_DYNAMIC_LIBS OFF)
    set(V8_LINK_LIBS pthread)
    set(V8_LINK_DIRS "")
    set(V8_BUILD_RPATH "")

    if(EXISTS "${CMAKE_SOURCE_DIR}/../v8Debug/Lib/Linux/libv8.so")
        set(V8_DEBUG_DYNAMIC_LIBRARY_DIR ${CMAKE_SOURCE_DIR}/../v8Debug/Lib/Linux)
    endif()

    if(EXISTS "${CMAKE_SOURCE_DIR}/../v8Release/Lib/Linux/libv8.so")
        set(V8_RELEASE_DYNAMIC_LIBRARY_DIR ${CMAKE_SOURCE_DIR}/../v8Release/Lib/Linux)
    endif()

    if(EXISTS "${JS_ENGINE_ROOT}/Lib/LinuxDynamic/Linux/libv8.so")
        set(V8_DYNAMIC_LIBRARY_DIR ${JS_ENGINE_ROOT}/Lib/LinuxDynamic/Linux)
    elseif(EXISTS "${JS_ENGINE_ROOT}/Lib/LinuxDynamic/libv8.so")
        set(V8_DYNAMIC_LIBRARY_DIR ${JS_ENGINE_ROOT}/Lib/LinuxDynamic)
    elseif(CMAKE_CONFIGURATION_TYPES AND V8_DEBUG_DYNAMIC_LIBRARY_DIR AND V8_RELEASE_DYNAMIC_LIBRARY_DIR)
        set(V8_USE_CONFIG_MATCHED_DYNAMIC_LIBS ON)
    elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
        if(V8_DEBUG_DYNAMIC_LIBRARY_DIR)
            set(V8_DYNAMIC_LIBRARY_DIR ${V8_DEBUG_DYNAMIC_LIBRARY_DIR})
        else()
            message(FATAL_ERROR
                "CMAKE_BUILD_TYPE=Debug requires libv8.so under ${CMAKE_SOURCE_DIR}/../v8Debug/Lib/Linux."
            )
        endif()
    else()
        if(V8_RELEASE_DYNAMIC_LIBRARY_DIR)
            set(V8_DYNAMIC_LIBRARY_DIR ${V8_RELEASE_DYNAMIC_LIBRARY_DIR})
        elseif(V8_DEBUG_DYNAMIC_LIBRARY_DIR)
            message(FATAL_ERROR
                "Non-Debug build requires libv8.so under ${CMAKE_SOURCE_DIR}/../v8Release/Lib/Linux."
            )
        endif()
    endif()

    if(EXISTS "${V8_STATIC_LIBRARY}")
        list(APPEND V8_LINK_LIBS ${V8_STATIC_LIBRARY})
    elseif(V8_USE_CONFIG_MATCHED_DYNAMIC_LIBS)
        list(APPEND V8_LINK_LIBS v8 v8_libplatform dl)
        list(APPEND V8_LINK_DIRS
            $<$<CONFIG:Debug>:${V8_DEBUG_DYNAMIC_LIBRARY_DIR}>
            $<$<NOT:$<CONFIG:Debug>>:${V8_RELEASE_DYNAMIC_LIBRARY_DIR}>
        )
        set(V8_BUILD_RPATH
            "$<$<CONFIG:Debug>:${V8_DEBUG_DYNAMIC_LIBRARY_DIR}>;$<$<NOT:$<CONFIG:Debug>>:${V8_RELEASE_DYNAMIC_LIBRARY_DIR}>"
        )
    elseif(V8_DYNAMIC_LIBRARY_DIR)
        list(APPEND V8_LINK_LIBS v8 v8_libplatform dl)
        list(APPEND V8_LINK_DIRS ${V8_DYNAMIC_LIBRARY_DIR})
        set(V8_BUILD_RPATH "${V8_DYNAMIC_LIBRARY_DIR}")
    else()
        message(FATAL_ERROR
            "Linux V8 libraries not found. Expected either ${V8_STATIC_LIBRARY} or libv8.so under "
            "${JS_ENGINE_ROOT}/Lib/LinuxDynamic[/Linux], "
            "${CMAKE_SOURCE_DIR}/../v8Debug/Lib/Linux, or "
            "${CMAKE_SOURCE_DIR}/../v8Release/Lib/Linux."
        )
    endif()

    target_link_libraries(jsenv PRIVATE ${V8_LINK_LIBS})

    if(V8_LINK_DIRS)
        target_link_directories(jsenv PRIVATE ${V8_LINK_DIRS})
    endif()

    if(V8_BUILD_RPATH)
        set_target_properties(jsenv PROPERTIES BUILD_RPATH "${V8_BUILD_RPATH}")
    endif()

    target_compile_definitions (helloworld PRIVATE PLATFORM_LINUX)
    target_compile_definitions (jsenv PRIVATE PLATFORM_LINUX)
else ()
    if ( JS_ENGINE STREQUAL "v8")
        set(LIBS_NEEDED winmm.lib dbghelp.lib shlwapi.lib)
    endif()
    if("${CMAKE_SIZEOF_VOID_P}" STREQUAL "4")
        target_link_libraries(jsenv
            ${JS_ENGINE_ROOT}/Lib/Win32/${LIB_NAME}.lib
            ${LIBS_NEEDED}
        )
    else ()
        target_link_libraries(jsenv
            ${JS_ENGINE_ROOT}/Lib/Win64/${LIB_NAME}.lib
            ${LIBS_NEEDED}
        )
    endif ()
    target_compile_definitions (helloworld PRIVATE PLATFORM_WINDOWS)
    target_compile_definitions (jsenv PRIVATE PLATFORM_WINDOWS)
endif ( )
