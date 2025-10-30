cmake_minimum_required(VERSION 3.16)
project(HeavenGate VERSION 1.0.0 LANGUAGES CXX)

# Настройка для Ninja
set(CMAKE_GENERATOR "Ninja" CACHE INTERNAL "")

# Настройка стандарта C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Оптимизации для Ninja
if(CMAKE_GENERATOR STREQUAL "Ninja")
    set(CMAKE_COLOR_DIAGNOSTICS ON)
    set(CMAKE_NINJA_FORCE_RESPONSE_FILE OFF)
endif()

# Настройка компилятора
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0 -DDEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")
    
    # Специфичные флаги для Clang с Ninja
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcolor-diagnostics")
    endif()
endif()

# Пути для поиска заголовков
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/thirdparty
    ${CMAKE_SOURCE_DIR}/src
)

# Поиск зависимостей
find_package(Threads REQUIRED)

# Подпроекты
add_subdirectory(src)

# Установка
install(TARGETS heavengate_components
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

if(TARGET heavengate)
    install(TARGETS heavengate
        RUNTIME DESTINATION bin
    )
endif()

# Установка конфигурационных файлов
install(DIRECTORY config/ DESTINATION etc/heavengate)
install(DIRECTORY docs/ DESTINATION share/doc/heavengate)