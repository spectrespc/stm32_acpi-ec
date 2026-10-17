#!/bin/bash
# ==============================================================================
# Скрипт: gen_version.sh
# Описание: Генерирует заголовок версии из git describe (или 1.0.0).
#
# Использование:
#   scripts/gen_version.sh [выходной_файл]
#
# По умолчанию пишет в common/version.h. CMake при сборке формирует
# тот же заголовок сам (build/generated/version.h), поэтому скрипт
# нужен для ручной/автономной генерации.
#
# Кодирование FW_VERSION: major<<16 | minor<<8 | patch (как GET_INFO),
# значение по умолчанию 0x00010000.
# ==============================================================================

set -e

OUT="${1:-common/version.h}"

RAW="$(git describe --tags --dirty --always 2>/dev/null || echo "1.0.0")"

if [[ "$RAW" =~ ^v?([0-9]+)\.([0-9]+)\.([0-9]+) ]]; then
    MAJOR="${BASH_REMATCH[1]}"
    MINOR="${BASH_REMATCH[2]}"
    PATCH="${BASH_REMATCH[3]}"
else
    MAJOR=1
    MINOR=0
    PATCH=0
fi

STRING="${MAJOR}.${MINOR}.${PATCH}"
HEX="$(printf '%08X' "$(( (MAJOR << 16) | (MINOR << 8) | PATCH ))")"

mkdir -p "$(dirname "$OUT")"

cat > "$OUT" <<EOF
/* Сгенерировано scripts/gen_version.sh — не редактировать вручную. */
#ifndef COMMON_VERSION_H
#define COMMON_VERSION_H

#define FW_VERSION        0x${HEX}
#define FW_VERSION_STRING "${STRING}"

#endif /* COMMON_VERSION_H */
EOF

echo "Сгенерирован $OUT: FW_VERSION=0x${HEX} (${STRING})"