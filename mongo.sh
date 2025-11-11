#!/bin/bash

set -euo pipefail

COMPOSE_FILE="docker-compose.yml"
SERVICE_NAME="mongodb"
MONGO_URI="mongodb://localhost:27017"

error() { echo "❌ $*" >&2; exit 1; }
info()  { echo "ℹ️  $*"; }

# Проверка зависимостей
command -v docker-compose >/dev/null || error "docker-compose не установлен"
grep -q "services:" "$COMPOSE_FILE" 2>/dev/null || error "Файл $COMPOSE_FILE не найден или недействителен"

# Извлекаем имя сервиса MongoDB из docker-compose.yml (по образу mongo)
# Если у вас другое имя — замените вручную или задайте явно
if ! grep -q "^[[:space:]]*${SERVICE_NAME}:" "$COMPOSE_FILE"; then
    error "Сервис '$SERVICE_NAME' не найден в $COMPOSE_FILE. Убедитесь, что имя совпадает."
fi

info "Запуск MongoDB через docker-compose..."
docker-compose up -d "$SERVICE_NAME" || error "Не удалось запустить контейнер"

# Ждём, пока MongoDB станет доступна (макс. 10 сек)
info "Ожидание готовности MongoDB..."
for _ in {1..20}; do
    if docker-compose exec "$SERVICE_NAME" mongosh --eval "db.runCommand({ping:1})" >/dev/null 2>&1; then
        info "MongoDB готова к работе."
        break
    fi
    sleep 0.5
done || error "MongoDB не ответила за отведённое время"

# Попытка подключиться локально через mongosh (если есть)
if command -v mongosh >/dev/null; then
    info "Подключение через локальный mongosh..."
    mongosh "$MONGO_URI"
else
    info "mongosh не установлен локально."
    info "Подключаюсь через контейнер..."
    docker-compose exec -it "$SERVICE_NAME" mongosh
fi
