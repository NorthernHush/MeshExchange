#!/bin/bash

# Цвета для вывода
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Артефакты для удаления (как в вашем скрипте)
ARTIFACTS=(
    "exchange-daemon"
    "client"
    "server"
    "mongo_client"
    "tests/test_runner"
    "obfuscator"
    "client.o"
    "mongo_ops_server.o"
    "server.o"
    "mongo_ops.o"
    "utils.o"
    "aes_gcm.o"
    "blake3.o"
    "blake3_dispatch.o"
    "blake3_portable.o"
    "blake3_sse2.o"
    "blake3_sse41.o"
    "blake3_avx2.o"
    "blake3_avx512.o"
)

echo -e "${BLUE}🧹 Очистка артефактов сборки...${NC}"

# Удаляем артефакты
for artifact in "${ARTIFACTS[@]}"; do
    if [ -f "$artifact" ] || [ -d "$artifact" ]; then
        rm -f "$artifact"
        echo "  ✅ Удалён: $artifact"
    fi
done

# Удаляем директорию .vscode (как в вашем скрипте)
if [ -d ".vscode" ]; then
    rm -rf ".vscode"
    echo "  ✅ Удалена директория: .vscode"
else
    echo "  ℹ️  Директория .vscode не найдена — пропускаю."
fi

echo -e "${GREEN}✓ Артефакты удалены.${NC}"

echo -e "${BLUE}🔍 Проверка изменений в репозитории...${NC}"
CHANGES=$(git status --porcelain)

if [ -z "$CHANGES" ]; then
    echo -e "${YELLOW}ℹ️  Нет изменений — коммит не требуется.${NC}"
    exit 0
fi

echo -e "${GREEN}✓ Обнаружены локальные изменения.${NC}"

echo -e "${BLUE}📦 Индексация всех файлов...${NC}"
git add .

if [ $? -ne 0 ]; then
    echo -e "${RED}❌ Ошибка при добавлении файлов в индекс.${NC}"
    exit 1
fi

echo -e "${GREEN}✓ Файлы добавлены в индекс.${NC}"

echo -e "${BLUE}📝 Создание коммита...${NC}"
COMMIT_MSG="build(clean): remove build artifacts and sync state [auto]"

git commit -m "$COMMIT_MSG"

if [ $? -ne 0 ]; then
    echo -e "${YELLOW}⚠️  Не удалось создать коммит (возможно, нет изменений).${NC}"
    exit 0
fi

echo -e "${GREEN}✓ Коммит успешно создан.${NC}"

echo -e "${BLUE}🚀 Отправка в origin/main...${NC}"

git push origin main

if [ $? -eq 0 ]; then
    echo -e "${GREEN}🎉 Изменения успешно отправлены в репозиторий!${NC}"
else
    echo -e "${YELLOW}⚠️  Не удалось отправить изменения (возможно, нет доступа или конфликт).${NC}"
    exit 1
fi