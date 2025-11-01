// db/mongo_ops.c
#include "mongo_ops.h"
#include <bits/time.h>
#include <mongoc/mongoc.h>
#include <bson/bson.h>
#include <string.h>

extern mongoc_collection_t *g_collection = NULL;

//* Глобальная переменная g_collection предполагается инициализированной 
//* в инициализации сервера
//* extern mongoc_collection_t *g_collection;

/**
 * @brief Конвертирует информацию об изменении файла в BSON-док.
 * Эта функция создает BSON-объект, описывающий тип изменения и размер файла после изменения
 * Используется для логирования изменений в монго
 * @param type  тип изменения(например "created"...).
 *              должен быть ненулевым
 * @param size_after  размер файла после изменения в байтах
 * @return указатель на новый БСОН-док. владелец обязан освободить память через bson_destroy();
 */


bson_t* change_info_to_bson(const char *type, int64_t size_after) {

    bson_t* doc = bson_new();

    BSON_APPEND_UTF8(doc, "type_of_changes", type);
    BSON_APPEND_INT64(doc, "size_after", size_after);


    return doc;
}

/**
 * @brief Преобразует структуру file_record_t в BSON-документ для хранения в MongoDB.
 *
 * Формирует полную запись о файле, включая имя, расширение, начальный и текущий размеры,
 * а также опциональный вложенный документ с информацией об изменениях.
 *
 * @param file Указатель на структуру file_record_t. Должен быть валидным.
 *
 * @return Указатель на новый BSON-документ. Владелец обязан вызвать bson_destroy().
 *
 * @warning Если file->changes != NULL, он должен указывать на валидный BSON-документ.
 *          Никакой валидации не проводится — это обязанность вызывающего кода.
 */

bson_t* file_overseer_to_bson(const file_record_t *file) {
    bson_t *doc = bson_new();
    BSON_APPEND_UTF8(doc, "filename", file->filename);

    BSON_APPEND_UTF8(doc, "extension", file->extension);

    BSON_APPEND_INT64(doc, "initial_size", file->initial_size);

    BSON_APPEND_INT64(doc, "actual_size", file->actual_size);

    if (file->changes) {
        //* вкладываем уже существующий бсон-док как поддокумент "changes"
        BSON_APPEND_DOCUMENT(doc, "changes", file->changes);

    }


    return doc;
}

/**
 * @brief Выполняет upsert (обновление или вставку) записи о файле в MongoDB.
 *
 * Использует оператор $set для обновления или создания документа с указанным именем файла.
 * Автоматически устанавливает временную метку last_modified в миллисекундах Unix-времени.
 *
 * @param filename  Имя файла (ключ поиска). Не должен быть NULL.
 * @param size      Размер файла в байтах.
 * @param mime      MIME-тип файла (например, "text/plain", "image/png").
 *
 * @return true в случае успеха, false — при ошибке (включая ошибки MongoDB).
 *
 * @note Безопасность:
 *       - Не проверяет длину или содержимое `filename` и `mime`.
 *         Вызывающий код обязан гарантировать их валидность и отсутствие внедрений.
 *       - Использует CLOCK_REALTIME для точной временной метки, совместимой с MongoDB.
 *
 * @warning Глобальная переменная g_collection должна быть инициализирована.
 *          В противном случае поведение неопределено.
 */

bool mongo_update_or_insert(const char *filename, uint64_t size, const char *mime) {
    //* формируем запрос, ищем документ по полю filename
    bson_t *query = bson_new();
    BSON_APPEND_UTF8(query, "filename", filename);

    //* подготавливаем апдейт через $set
    bson_t *update = bson_new();

    bson_t *set_fields = bson_new();


    BSON_APPEND_UTF8(set_fields, "filename", filename);
    BSON_APPEND_INT64(set_fields, "size", (int64_t)size);
    BSON_APPEND_UTF8(set_fields, "mime_type", mime);

    //* получаем текущее время с точностью до мил.секунд
    struct timespec ts;
    
    clock_gettime(CLOCK_REALTIME, &ts);

    int64_t now_ms = (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
    BSON_APPEND_DATE_TIME(set_fields, "last_modified", now_ms);

    //* вкладываем поля в оператор $set
    BSON_APPEND_DOCUMENT(update, "$set", set_fields);

    bson_destroy(set_fields);

    bson_t reply;

    bson_error_t error;

    bool success = mongoc_collection_update_one(

        g_collection,
        query,
        update,
        NULL,       //* опции (можно upset=true)
        NULL,      //* read prefs (не требуется для записи)
        &error

    );

    //* освобождаем ресурсы
    bson_destroy(query);
    bson_destroy(update);

    if(!success) {

        fprintf(stderr, "mongodb update fail for '%s': %s\n", filename, error.message);

    }

    return success;

}


/**
 * @brief Вставляет новую запись о файле в коллекцию MongoDB.
 *
 * Создаёт документ с метаданными: имя, MIME-тип, размер, флаг удаления и временную метку создания.
 *
 * @param filename     Имя файла. Не должен быть NULL.
 * @param size         Размер файла в байтах.
 * @param mime_type    MIME-тип файла.
 *
 * @return true при успешной вставке, false — в случае ошибки.
 *
 * @note Используется bson_get_monotonic_time() для created_at, что обеспечивает
 *       монотонность временной метки даже при изменениях системного времени.
 *       Однако MongoDB ожидает Unix-время в миллисекундах — делим на 1000.
 *
 * @warning Эта функция НЕ выполняет upsert. Если документ с таким filename уже существует,
 *          вставка завершится ошибкой дубликата (если есть уникальный индекс).
 *          Для безопасного добавления используйте mongo_update_or_insert или проверяйте существование.
 *
 * @pre g_collection != NULL
 */
bool mongo_insert(mongoc_collection_t *coll, const char *filename, uint64_t size, const char *mime) {
    if (!coll) {
        fprintf(stderr, "ERROR: mongo_insert called with NULL collection!\n");
        return false;
    }

    bson_t *doc = bson_new();
    BSON_APPEND_UTF8(doc, "filename", filename);
    BSON_APPEND_INT64(doc, "size", size);
    BSON_APPEND_UTF8(doc, "mime_type", mime);
    BSON_APPEND_BOOL(doc, "deleted", false);
    BSON_APPEND_DATE_TIME(doc, "uploaded_at", bson_get_monotonic_time() / 1000);

    bson_error_t error;
    bool success = mongoc_collection_insert_one(coll, doc, NULL, NULL, &error);
    
    if (!success) {
        fprintf(stderr, "MongoDB insert error: %s\n", error.message);
    }
    
    bson_destroy(doc);
    return success;
}