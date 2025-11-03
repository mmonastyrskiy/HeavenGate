# Исходные файлы
set(SRC_FILES
    main.cpp
    DataBus/DataBus.cpp
    LoadBalancer/LoadBalancer.cpp
)

set(HEADER_FILES
    DataBus/BusEvent.h
    DataBus/DataBusMetrics.h
    DataBus/subscriptionID.h
    LoadBalancer/LoadBalancer.h
    ../thirdparty/json.hpp
)

# Основной исполняемый файл
add_executable(heavengate
    ${SRC_FILES}
    ${HEADER_FILES}
)

# Заголовочные файлы
target_include_directories(heavengate PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/thirdparty
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Зависимости
target_link_libraries(heavengate
    pthread
)

# Выходные директории
set_target_properties(heavengate PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Установка (только исполняемый файл)
install(TARGETS heavengate DESTINATION bin)