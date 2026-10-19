#!/bin/bash
# Создание и публикация релизного тега SemVer с полной pre-tag проверкой.
# Использование: ./scripts/tag_release.sh 1.0.1 ["Сообщение"]
set -e

VERSION="$1"

if [ -z "$VERSION" ]; then
    echo "Использование: $0 <версия> [сообщение]"
    echo "Пример: $0 1.0.1"
    exit 1
fi

MESSAGE="${2:-Release v$VERSION}"
TAG="v${VERSION}"

echo "=== Pre-tag проверка тега $TAG ==="

# 0. Тег не должен существовать
if git rev-parse "$TAG" >/dev/null 2>&1; then
    echo "ОШИБКА: тег $TAG уже существует"
    exit 1
fi

# 0. Нет незакоммиченных изменений (включая неотслеживаемые файлы,
#    иначе тег может не содержать файлов, участвовавших в проверке)
if [ -n "$(git status --porcelain)" ]; then
    echo "ОШИБКА: есть незакоммиченные изменения"
    git status --short
    exit 1
fi

# 1. Форматирование
echo "--- 1/5: clang-format ---"
cmake -B build_host >/dev/null 2>&1
cmake --build build_host --target format-check

# 2. Статический анализ
echo "--- 2/5: cppcheck ---"
cmake --build build_host --target cppcheck

# 3. Кросс-сборка C8T6
echo "--- 3/5: сборка C8T6 ---"
cmake -B build_c8t6 -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake -DMCU=C8T6 >/dev/null
cmake --build build_c8t6

# 4. Кросс-сборка CBT6
echo "--- 4/5: сборка CBT6 ---"
cmake -B build_cbt6 -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake -DMCU=CBT6 >/dev/null
cmake --build build_cbt6

# 5. Тесты
echo "--- 5/5: тесты ---"
cmake --build build_host --target check

# Все проверки пройдены
echo ""
echo "=== Все проверки пройдены. Создание тега $TAG ==="
git tag -a "$TAG" -m "$MESSAGE"
git push origin "$TAG"
echo ""
echo "Тег $TAG отправлен. Релиз собирается в Actions:"
echo "  https://github.com/spectrespc/stm32_acpi-ec/actions"
echo ""
echo "Не забудьте обновить docs/changelog.md после публикации релиза."
