cmake_minimum_required(VERSION 3.10)
project(heavengate)

# Установка стандарта C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Исходные файлы
set(SRC_FILES
    main.cpp
    DataBus/DataBus.cpp
    LoadBalancer/LoadBalancer.cpp
    common/Argparcer.cpp
    common/logger.cpp
    common/Confparcer.cpp
    API/dashboardAPI.cpp
)

set(HEADER_FILES
    DataBus/BusEvent.h
    DataBus/DataBusMetrics.h
    DataBus/subscriptionID.h
    LoadBalancer/LoadBalancer.h
    ../include/colorText.h
    ../include/strconv.h
    ../thirdparty/json.hpp
    common/Argparcer.h
    common/logger.h
    common/Confparcer.h
    API/dashboardAPI.h
)

# Основной исполняемый файл
add_executable(heavengate
    ${SRC_FILES}
    ${HEADER_FILES}
    common/logger.h common/logger.cpp
)

# ==================== HEADER-ONLY БИБЛИОТЕКИ ====================

# 1. JSON библиотека (nlohmann/json)
add_library(json_header_only INTERFACE)
add_library(colorText INTERFACE)
target_include_directories(json_header_only INTERFACE
    ${CMAKE_SOURCE_DIR}/thirdparty
)


target_include_directories(colorText INTERFACE
    ${CMAKE_SOURCE_DIR}/include
)

# ==================== КОНЕЦ HEADER-ONLY БИБЛИОТЕК ====================

# Определяем целевую ОС
if(WIN32)
    set(PLATFORM_LIBS ws2_32 crypt32 wldap32)
    set(CURL_PATHS 
        "C:/ProgramData/chocolatey/lib/curl/tools/curl-8.17.0_1-win64-mingw"
        "C:/Program Files/curl"
        "$ENV{CURL_ROOT}"
    )
else()
    set(PLATFORM_LIBS)
    set(CURL_PATHS 
        "/usr"
        "/usr/local"
        "$ENV{CURL_ROOT}"
    )
endif()

# Поиск CURL
find_path(CURL_INCLUDE_DIR curl/curl.h
    PATHS ${CURL_PATHS}
    PATH_SUFFIXES include
)

find_library(CURL_LIBRARY 
    NAMES curl libcurl
    PATHS ${CURL_PATHS}
    PATH_SUFFIXES lib
)

if(CURL_INCLUDE_DIR)
    message(STATUS "Found CURL include dir: ${CURL_INCLUDE_DIR}")
else()
    message(WARNING "CURL include directory not found")
endif()

if(CURL_LIBRARY)
    message(STATUS "Found CURL library: ${CURL_LIBRARY}")
else()
    message(WARNING "CURL library not found")
endif()

# Заголовочные файлы
target_include_directories(heavengate PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/thirdparty
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CURL_INCLUDE_DIR}
)

# Зависимости (ДОБАВЛЯЕМ HEADER-ONLY БИБЛИОТЕКИ)
target_link_libraries(heavengate PRIVATE
    pthread
    ${CURL_LIBRARY}
    ${PLATFORM_LIBS}
    json_header_only  # Добавляем header-only библиотеку
    colorText
)

# Компиляционные флаги
if(MSVC)
    target_compile_options(heavengate PRIVATE /W4)
else()
    target_compile_options(heavengate PRIVATE -Wall -Wextra)
endif()

# Выходные директории
set_target_properties(heavengate PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

# Установка (только исполняемый файл)
install(TARGETS heavengate DESTINATION bin)