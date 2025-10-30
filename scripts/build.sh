#!/bin/bash

set -e

BUILD_TYPE=${1:-Release}
BUILD_DIR="build-ninja"
GENERATOR="Ninja"

echo "🔨 Сборка HeavenGate с Ninja (${BUILD_TYPE})..."

# Создание директории сборки
mkdir -p ${BUILD_DIR}
cd ${BUILD_DIR}

# Конфигурация CMake с Ninja
cmake -G "${GENERATOR}" .. -DCMAKE_BUILD_TYPE=${BUILD_TYPE}

# Сборка с Ninja
ninja -v

echo "✅ Сборка завершена!"
echo "📁 Исполняемые файлы в: ${BUILD_DIR}/bin/"
echo "📁 Библиотеки в: ${BUILD_DIR}/lib/"