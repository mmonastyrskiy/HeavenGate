cmake_minimum_required(VERSION 3.15)
project(heavengate VERSION 1.0.0 LANGUAGES CXX)

# Установка стандарта C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Исходные файлы
set(SRC_FILES
    main.cpp
    AppManager/AppManager.cpp
    DataBus/DataBus.cpp
    LoadBalancer/LoadBalancer.cpp
    common/Argparcer.cpp
    common/logger.cpp
    common/Confparcer.cpp
    API/dashboardAPI.cpp
)

set(HEADER_FILES
    AppManager/AppComponent.h
    AppManager/AppManager.h
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
    common/generic.h
    API/dashboardAPI.h
)

# ==================== GO DASHBOARD ====================

# Проверяем наличие Go
find_program(GO_EXECUTABLE go)
if(GO_EXECUTABLE)
    # Получаем абсолютный путь к директории Go проекта
    get_filename_component(GO_DASHBOARD_DIR 
        "${CMAKE_SOURCE_DIR}/go-services/Dashboard" 
        ABSOLUTE
    )
    
    # Проверяем существует ли go.mod файл
    if(EXISTS "${GO_DASHBOARD_DIR}/go.mod")
        # Создаем директорию для выходных файлов Go
        set(GO_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin/go-apps")
        
        add_custom_command(
            OUTPUT ${GO_OUTPUT_DIR}/dashboard
            COMMAND ${CMAKE_COMMAND} -E make_directory ${GO_OUTPUT_DIR}
            COMMAND ${GO_EXECUTABLE} build -o ${GO_OUTPUT_DIR}/dashboard .
            WORKING_DIRECTORY ${GO_DASHBOARD_DIR}
            COMMENT "Building Go Dashboard"
            VERBATIM
        )
        
        add_custom_target(go-dashboard ALL
            DEPENDS ${GO_OUTPUT_DIR}/dashboard
        )
        
        # Установка Go бинарника
        install(
            FILES ${GO_OUTPUT_DIR}/dashboard
            DESTINATION bin/go-apps
            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )
        
    else()
        message(WARNING "go.mod not found in ${GO_DASHBOARD_DIR}. Go dashboard will not be built.")
        add_custom_target(go-dashboard COMMENT "Go dashboard skipped - no go.mod found")
    endif()
else()
    message(WARNING "Go compiler not found. Go dashboard will not be built.")
    add_custom_target(go-dashboard COMMENT "Go dashboard skipped - Go not found")
endif()

# Основной исполняемый файл
add_executable(heavengate
    ${SRC_FILES}
    ${HEADER_FILES}
)

if(GO_EXECUTABLE AND EXISTS "${GO_DASHBOARD_DIR}/go.mod")
    add_dependencies(heavengate go-dashboard)
endif()

# ==================== HEADER-ONLY БИБЛИОТЕКИ ====================

# JSON библиотека (nlohmann/json)
add_library(json_header_only INTERFACE)
target_include_directories(json_header_only INTERFACE
    ${CMAKE_SOURCE_DIR}/thirdparty
)

# ColorText библиотека
add_library(colorText INTERFACE)
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
find_package(CURL QUIET)

if(NOT CURL_FOUND)
    # Fallback: ручной поиск если find_package не сработал
    find_path(CURL_INCLUDE_DIR curl/curl.h
        PATHS ${CURL_PATHS}
        PATH_SUFFIXES include
    )

    find_library(CURL_LIBRARY 
        NAMES curl libcurl
        PATHS ${CURL_PATHS}
        PATH_SUFFIXES lib
    )
    
    if(CURL_INCLUDE_DIR AND CURL_LIBRARY)
        set(CURL_FOUND TRUE)
        message(STATUS "Found CURL: ${CURL_LIBRARY}")
    endif()
endif()

if(CURL_FOUND)
    if(TARGET CURL::libcurl)
        # Используем импортированную цель если доступна
        set(CURL_TARGET CURL::libcurl)
    else()
        # Создаем интерфейсную цель для CURL
        add_library(curl_lib INTERFACE)
        target_include_directories(curl_lib INTERFACE ${CURL_INCLUDE_DIR})
        target_link_libraries(curl_lib INTERFACE ${CURL_LIBRARY})
        set(CURL_TARGET curl_lib)
    endif()
else()
    message(WARNING "CURL not found. Some features may be unavailable.")
endif()

# Заголовочные файлы
target_include_directories(heavengate PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/thirdparty
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Зависимости
target_link_libraries(heavengate PRIVATE
    pthread
    ${CURL_TARGET}
    ${PLATFORM_LIBS}
    json_header_only
    colorText
)

# Компиляционные флаги
if(MSVC)
    target_compile_options(heavengate PRIVATE /W4)
else()
    target_compile_options(heavengate PRIVATE -Wall -Wextra -pedantic)
endif()

# Выходные директории
set_target_properties(heavengate PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

# Установка
install(TARGETS heavengate DESTINATION bin)