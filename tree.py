import os

# Расширения, которые НЕ включать в отчёт (бинарные и чувствительные)
EXCLUDED_EXTENSIONS = {
    # Бинарные исполняемые и объектные файлы
    '.o', '.a', '.so', '.dylib', '.dll', '.exe', '.out', '.bin', '.elf',
    # Образы, архивы, данные
    '.png', '.jpg', '.jpeg', '.gif', '.bmp', '.ico', '.pdf', '.zip', '.tar',
    '.gz', '.bz2', '.xz', '.7z', '.rar', '.mp3', '.mp4', '.wav', '.avi',
    # Криптографические материалы и сертификаты
    '.crt', '.cert', '.pem', '.key', '.der', '.p12', '.pfx', '.csr', '.jks',
    # Другие потенциально бинарные или приватные форматы
    '.db', '.sqlite', '.lock', '.swp', '.swo', '.pyc', '.pyo', '.cache', '.vscode',
}

# Директории, которые нужно полностью пропускать (регистрозависимо)
EXCLUDED_DIRECTORIES = {
    'node_modules',
    '.git',
    # Можно добавить: '.hg', '.svn', '__pycache__', 'venv', 'dist', 'build' и т.д.
}

def is_binary_file(file_path, chunk_size=1024):
    """
    Проверяет, является ли файл бинарным, читая начало и ища нулевые байты
    или слишком много недекодируемых символов.
    """
    try:
        with open(file_path, 'rb') as f:
            chunk = f.read(chunk_size)
            if b'\x00' in chunk:
                return True
            # Попытка декодировать как UTF-8
            try:
                chunk.decode('utf-8')
                return False
            except UnicodeDecodeError:
                # Если много недекодируемых байт — считаем бинарным
                text_chars = bytes(range(32, 127)) + b'\n\r\t\f\b'
                return bool(chunk.translate(None, text_chars))
    except Exception:
        return True  # По умолчанию — считаем бинарным при ошибке доступа

def count_lines_in_file(file_path):
    """Считает количество строк в файле."""
    try:
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            return sum(1 for _ in f)
    except Exception as e:
        return f"Ошибка при чтении ({e})"

def read_file_content(file_path):
    """Читает содержимое файла."""
    try:
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            return f.read()
    except Exception as e:
        return f"⚠️ Ошибка при чтении файла: {e}"

def build_file_tree(root_dir, include_ext=None, level=0, output_lines=None, collected_files=None):
    """
    Рекурсивно обходит папку, считает строки и строит дерево файлов.
    include_ext — список расширений (например ['.py', '.js'])
    collected_files — список путей к файлам для последующего вывода их содержимого.
    """
    total_lines = 0
    indent = "│   " * level

    try:
        items = sorted(os.listdir(root_dir))
    except PermissionError:
        line = f"{indent}🚫 Нет доступа: {root_dir}"
        print(line)
        output_lines.append(line)
        return 0

    for item in items:
        path = os.path.join(root_dir, item)

        # Пропускаем исключённые директории (например, node_modules, .git)
        if os.path.isdir(path) and item in EXCLUDED_DIRECTORIES:
            continue

        if os.path.isfile(path):
            ext = os.path.splitext(item)[1].lower()

            # Пропускаем явно исключённые расширения
            if ext in EXCLUDED_EXTENSIONS:
                continue

            # Если задан фильтр по расширениям — проверяем соответствие
            if include_ext is not None and ext not in include_ext:
                continue

            # Пропускаем бинарные файлы даже при совпадении расширения
            if is_binary_file(path):
                continue

            lines = count_lines_in_file(path)
            line_info = f"{indent}├── 📄 {item} ({lines} строк)" if isinstance(lines, int) else f"{indent}├── 📄 {item} ({lines})"
            print(line_info)
            output_lines.append(line_info)
            if isinstance(lines, int):
                total_lines += lines
            collected_files.append(path)
        elif os.path.isdir(path):
            line_dir = f"{indent}📁 {item}/"
            print(line_dir)
            output_lines.append(line_dir)
            total_lines += build_file_tree(path, include_ext, level + 1, output_lines, collected_files)

    if level == 0:
        summary = f"\n📊 Итого строк кода: {total_lines}"
        print(summary)
        output_lines.append(summary)
    return total_lines


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(description="Подсчет строк кода, структура проекта и сохранение всего кода в файл.")
    parser.add_argument("path", help="Путь к проекту")
    parser.add_argument("--ext", nargs="*", help="Фильтр по расширениям (например .py .js .html)")
    args = parser.parse_args()

    root = os.path.abspath(args.path)
    report_path = os.path.join(root, "project_report.txt")

    print(f"🔍 Анализ проекта: {root}\n")

    output_lines = [f"🔍 Анализ проекта: {root}\n"]
    collected_files = []

    total = build_file_tree(root, include_ext=args.ext, output_lines=output_lines, collected_files=collected_files)
    output_lines.append(f"\n✅ Всего строк кода: {total}")

    # --- Добавляем все исходники в конец отчёта ---
    output_lines.append("\n" + "=" * 80)
    output_lines.append("📘 СОДЕРЖИМОЕ ВСЕХ ФАЙЛОВ")
    output_lines.append("=" * 80 + "\n")

    for file_path in collected_files:
        output_lines.append(f"\n\n# 📄 {file_path}\n")
        content = read_file_content(file_path)
        output_lines.append(content)

    # Записываем отчёт
    with open(report_path, "w", encoding="utf-8") as f:
        f.write("\n".join(output_lines))

    print(f"\n📄 Отчет сохранен в файл: {report_path}")
