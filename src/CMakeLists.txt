add_compile_options(-Wno-c++17-extensions)
# Автоматический поиск исходных файлов
file(GLOB_RECURSE SRC_FILES 
    "*.cpp"
)

file(GLOB_RECURSE HEADER_FILES
    "*.h"
    "*.hpp"
)

# Статическая библиотека компонентов
add_library(heavengate_components STATIC
    ${SRC_FILES}
)

# Настройка целевого проекта
target_include_directories(heavengate_components PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/thirdparty
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(heavengate_components
    Threads::Threads
)

# Свойства для Ninja
set_target_properties(heavengate_components PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

# Создание исполняемого файла когда появится main.cpp
add_executable(heavengate_exe)

# Пока пустой, будет добавлен позже
target_sources(heavengate_exe PRIVATE $<$<BOOL:${CMAKE_SOURCE_DIR}/src/main.cpp>:${CMAKE_SOURCE_DIR}/src/main.cpp>)

target_link_libraries(heavengate_exe
    heavengate_components
)

set_target_properties(heavengate_exe PROPERTIES
    OUTPUT_NAME "heavengate"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)