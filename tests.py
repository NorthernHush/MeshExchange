# ======== FOR BUILD UNIT TESTS =========
import os
import subprocess
from pathlib import Path
from datetime import datetime
from colorama import Fore, Style, init

init(autoreset=True)

CC = "gcc"
CFLAGS = ["-Wall", "-Wextra", "-O2", "-g"]
LDFLAGS = ["-lm", "-lpthread", "-lssl", "-lcrypto"]

PROJECT_ROOT = Path(__file__).resolve().parent
SRC_DIR = PROJECT_ROOT / "src"
TEST_DIR = PROJECT_ROOT / "tests"
BUILD_DIR = PROJECT_ROOT / "build" / "tests"
LOG_FILE = PROJECT_ROOT / "test_report.log"


def log(message: str, color=None, to_file=True):
    """Выводит сообщение в консоль и записывает в лог"""
    if color:
        print(color + message + Style.RESET_ALL)
    else:
        print(message)

    if to_file:
        with open(LOG_FILE, "a", encoding="utf-8") as f:
            f.write(message + "\n")


def compile_test(source_file: Path):
    """Компилирует один тестовый файл .c"""
    target_file = BUILD_DIR / source_file.stem
    cmd = [CC, *CFLAGS, str(source_file), "-o", str(target_file), *LDFLAGS]

    log(f"🔧 Компиляция: {source_file.name}", Fore.CYAN)
    result = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

    if result.returncode == 0:
        log(f"✅ Успешно скомпилирован: {target_file}", Fore.GREEN)
    else:
        log(f"❌ Ошибка компиляции {source_file.name}:", Fore.RED)
        log(result.stderr)
    return target_file if result.returncode == 0 else None


def run_test(binary: Path):
    """Запускает один тест и логирует результат"""
    log(f"\n▶️  Запуск теста: {binary.name}", Fore.MAGENTA)
    result = subprocess.run([str(binary)], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

    if result.returncode == 0:
        log(f"✅ Тест {binary.name} прошёл успешно!", Fore.GREEN)
    else:
        log(f"❌ Тест {binary.name} провален (код {result.returncode})", Fore.RED)

    if result.stdout.strip():
        log("--- STDOUT ---", Fore.BLUE)
        log(result.stdout.strip())
    if result.stderr.strip():
        log("--- STDERR ---", Fore.YELLOW)
        log(result.stderr.strip())

    return result.returncode == 0


def main():
    # Очистка старого лога
    if LOG_FILE.exists():
        LOG_FILE.unlink()

    log("=" * 80)
    log(f"🧪 Запуск тестов MeshExchange — {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    log("=" * 80 + "\n")

    if not TEST_DIR.exists():
        log(f"❌ Папка tests не найдена!", Fore.RED)
        return

    BUILD_DIR.mkdir(parents=True, exist_ok=True)

    test_files = list(TEST_DIR.glob("*.c"))
    if not test_files:
        log("⚠️  Тестовых файлов (.c) не найдено.", Fore.YELLOW)
        return

    log(f"Найдено тестов: {len(test_files)}\n")

    compiled = []
    for test_file in test_files:
        binary = compile_test(test_file)
        if binary:
            compiled.append(binary)

    log(f"\n🚀 Запуск {len(compiled)} тестов...\n")
    passed = 0

    for binary in compiled:
        ok = run_test(binary)
        if ok:
            passed += 1

    log("\n" + "=" * 60)
    log(f"✅ Пройдено: {passed}/{len(compiled)} тестов")
    log("=" * 60 + "\n")

    log(f"📄 Лог сохранён в: {LOG_FILE}", Fore.CYAN)


if __name__ == "__main__":
    main()
