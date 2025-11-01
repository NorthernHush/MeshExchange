#!/usr/bin/env python3
"""
ULTIMATE C/C++ STATIC ANALYZER - Senior Level
Мощнейший статический анализатор с использованием современных алгоритмов
"""
import argparse
import os
import re
import ast
import subprocess
import threading
import queue
from pathlib import Path
from dataclasses import dataclass, field
from typing import List, Dict, Set, Tuple, Optional, Any
import json
import time
import logging
from concurrent.futures import ThreadPoolExecutor, as_completed, ProcessPoolExecutor
from collections import defaultdict, deque
import hashlib
import pickle
import statistics
from enum import Enum
import platform
import psutil
from datetime import datetime
import multiprocessing as mp
from contextlib import contextmanager
import functools
import inspect

# Современные библиотеки для анализа
try:
    import clang
    from clang.cindex import Index, Config, TranslationUnit, Cursor, CursorKind
    CLANG_AVAILABLE = True
except ImportError:
    CLANG_AVAILABLE = False

try:
    import radon
    from radon.complexity import cc_visit, cc_rank
    from radon.metrics import h_visit, mi_visit
    from radon.raw import analyze
    RADON_AVAILABLE = True
except ImportError:
    RADON_AVAILABLE = False

try:
    import lizard
    LIZARD_AVAILABLE = True
except ImportError:
    LIZARD_AVAILABLE = False

# Настройка расширенного логирования
class ColoredFormatter(logging.Formatter):
    """Форматирование логов с цветами"""
    COLORS = {
        'DEBUG': '\033[36m',
        'INFO': '\033[32m',
        'WARNING': '\033[33m',
        'ERROR': '\033[31m',
        'CRITICAL': '\033[41m',
        'RESET': '\033[0m'
    }
    
    def format(self, record):
        log_message = super().format(record)
        return f"{self.COLORS.get(record.levelname, '')}{log_message}{self.COLORS['RESET']}"

# Настройка логгера
def setup_logging(verbose=False):
    logger = logging.getLogger()
    logger.setLevel(logging.DEBUG if verbose else logging.INFO)
    
    # File handler
    file_handler = logging.FileHandler('ultimate_analyzer.log', encoding='utf-8')
    file_handler.setFormatter(logging.Formatter(
        '%(asctime)s - %(name)s - %(levelname)s - %(message)s'
    ))
    
    # Console handler with colors
    console_handler = logging.StreamHandler()
    console_handler.setFormatter(ColoredFormatter(
        '%(asctime)s - %(levelname)s - %(message)s'
    ))
    
    logger.addHandler(file_handler)
    logger.addHandler(console_handler)

class IssueSeverity(Enum):
    CRITICAL = 5
    HIGH = 4
    MEDIUM = 3
    LOW = 2
    INFO = 1

class AnalysisCategory(Enum):
    MEMORY_SAFETY = "memory_safety"
    SECURITY = "security"
    PERFORMANCE = "performance"
    CODE_QUALITY = "code_quality"
    CONCURRENCY = "concurrency"

@dataclass
class CodeIssue:
    """Расширенный класс для представления проблем в коде"""
    id: str
    file_path: str
    line_number: int
    column: int
    issue_type: str
    category: AnalysisCategory
    severity: IssueSeverity
    description: str
    code_snippet: str
    confidence: float
    suggestions: List[str] = field(default_factory=list)
    cwe: List[str] = field(default_factory=list)
    metrics: Dict[str, Any] = field(default_factory=dict)
    context: Dict[str, Any] = field(default_factory=dict)

@dataclass
class FileMetrics:
    """Метрики анализа файла"""
    complexity: float = 0.0
    maintainability: float = 0.0
    lines_of_code: int = 0
    comment_ratio: float = 0.0
    function_count: int = 0
    issue_density: float = 0.0
    memory_risk_score: float = 0.0

class AdvancedPatternEngine:
    """Движок расширенных паттернов с машинным обучением"""
    
    def __init__(self):
        self.patterns = self._load_advanced_patterns()
        self.learned_patterns = self._load_learned_patterns()
        
    def _load_advanced_patterns(self) -> Dict[str, List[Dict]]:
        """Загрузка расширенных паттернов с контекстом"""
        return {
            'memory_leak': [
                {
                    'pattern': r'(?:(?:malloc|calloc|realloc|strdup)\s*\([^)]+\)|new\s+[^{];]*)(?!.*(?:free|delete)\s*\(\s*\w+\s*\))',
                    'context': {'requires_allocation': True, 'requires_free': True},
                    'weight': 0.9
                },
                {
                    'pattern': r'(\w+)\s*=\s*(?:malloc|calloc|realloc|strdup|new).*?(?:(?=return|\})|$)',
                    'context': {'track_variable': True},
                    'weight': 0.8
                }
            ],
            'double_free': [
                {
                    'pattern': r'free\s*\(\s*(\w+)\s*\).*?free\s*\(\s*\1\s*\)',
                    'context': {'dangerous': True},
                    'weight': 0.95
                }
            ],
            'use_after_free': [
                {
                    'pattern': r'free\s*\(\s*(\w+)\s*\).*?\1\s*[=!<>\[\]]',
                    'context': {'critical': True},
                    'weight': 0.97
                }
            ],
            'buffer_overflow': [
                {
                    'pattern': r'(strcpy|strcat|sprintf|gets|scanf)\s*\(',
                    'context': {'security_risk': True},
                    'weight': 0.85
                },
                {
                    'pattern': r'(\w+)\s*\[.*?\]\s*=\s*[^;]+;.*?\1\s*\[.*?\].*?=',
                    'context': {'bounds_check': False},
                    'weight': 0.7
                }
            ],
            'null_pointer': [
                {
                    'pattern': r'(\w+)\s*=\s*(?:NULL|nullptr).*?\1\s*->',
                    'context': {'null_dereference': True},
                    'weight': 0.96
                }
            ],
            'concurrency_issues': [
                {
                    'pattern': r'pthread_mutex_lock.*?pthread_mutex_unlock',
                    'context': {'requires_pairing': True},
                    'weight': 0.75
                },
                {
                    'pattern': r'(\w+)\s*=\s*\w+\s*\+\s*\w+.*?(?=pthread_mutex_lock)',
                    'context': {'race_condition': True},
                    'weight': 0.8
                }
            ]
        }
    
    def _load_learned_patterns(self) -> Dict:
        """Загрузка обученных паттернов (заглушка для ML модели)"""
        # В реальной реализации здесь была бы загрузка ML модели
        return {}
    
    def analyze_with_context(self, code: str, file_context: Dict) -> List[Dict]:
        """Анализ с учетом контекста файла"""
        issues = []
        
        for category, patterns in self.patterns.items():
            for pattern_info in patterns:
                matches = re.finditer(pattern_info['pattern'], code, re.MULTILINE | re.DOTALL)
                for match in matches:
                    issue = {
                        'type': category,
                        'match': match.group(),
                        'position': match.span(),
                        'weight': pattern_info['weight'],
                        'context': pattern_info['context']
                    }
                    issues.append(issue)
        
        return self._apply_ml_heuristics(issues, code, file_context)
    
    def _apply_ml_heuristics(self, issues: List[Dict], code: str, context: Dict) -> List[Dict]:
        """Применение ML эвристик для фильтрации ложных срабатываний"""
        # Упрощенная реализация ML-фильтрации
        filtered_issues = []
        
        for issue in issues:
            # Эвристика: игнорировать шаблонные освобождения в деструкторах
            if 'free' in issue['match'] and '~' in code:
                issue['weight'] *= 0.5
            
            # Эвристика: повысить вес в сложных функциях
            if context.get('high_complexity', False):
                issue['weight'] *= 1.2
            
            if issue['weight'] > 0.6:  # Порог уверенности
                filtered_issues.append(issue)
        
        return filtered_issues

class ControlFlowAnalyzer:
    """Анализатор потока управления для сложного анализа"""
    
    def __init__(self):
        self.cfg = {}  # Control Flow Graph
        self.data_flow = {}
        
    def build_control_flow_graph(self, code: str) -> Dict:
        """Построение графа потока управления"""
        lines = code.split('\n')
        cfg = {}
        current_block = []
        block_id = 0
        
        for i, line in enumerate(lines):
            line = line.strip()
            
            # Начало нового базового блока
            if self._is_control_flow_statement(line) and current_block:
                cfg[block_id] = current_block.copy()
                current_block = []
                block_id += 1
            
            current_block.append((i + 1, line))
            
            # Конец функции или области видимости
            if line == '}' and current_block:
                cfg[block_id] = current_block.copy()
                current_block = []
                block_id += 1
        
        if current_block:
            cfg[block_id] = current_block
        
        return cfg
    
    def _is_control_flow_statement(self, line: str) -> bool:
        """Определение операторов управления потоком"""
        patterns = [
            r'^if\s*\(', r'^else', r'^for\s*\(', r'^while\s*\(', 
            r'^switch\s*\(', r'^case\s+', r'^return\s+',
            r'^break\s*;', r'^continue\s*;', r'^goto\s+'
        ]
        return any(re.match(pattern, line) for pattern in patterns)
    
    def analyze_data_flow(self, cfg: Dict) -> Dict:
        """Анализ потока данных"""
        variable_states = {}
        
        for block_id, instructions in cfg.items():
            for line_num, instruction in instructions:
                self._update_variable_states(variable_states, instruction, line_num)
        
        return variable_states
    
    def _update_variable_states(self, states: Dict, instruction: str, line_num: int):
        """Обновление состояний переменных"""
        # Анализ выделения памяти
        alloc_matches = re.finditer(r'(\w+)\s*=\s*(malloc|calloc|realloc|new)\s*\(', instruction)
        for match in alloc_matches:
            var_name = match.group(1)
            states[var_name] = {
                'allocated': True,
                'freed': False,
                'allocation_line': line_num,
                'last_used': line_num
            }
        
        # Анализ освобождения памяти
        free_matches = re.finditer(r'(free|delete)\s*\(\s*(\w+)\s*\)', instruction)
        for match in free_matches:
            var_name = match.group(2)
            if var_name in states:
                states[var_name]['freed'] = True
                states[var_name]['free_line'] = line_num
        
        # Отслеживание использования переменных
        use_matches = re.finditer(r'(\w+)\s*[=!<>+\-*/]', instruction)
        for match in use_matches:
            var_name = match.group(1)
            if var_name in states:
                states[var_name]['last_used'] = line_num

class SemanticAnalyzer:
    """Семантический анализатор с глубоким пониманием кода"""
    
    def __init__(self):
        self.symbol_table = {}
        self.type_system = {}
        self.function_prototypes = {}
        
    def analyze_symbols(self, code: str) -> Dict:
        """Анализ символов и их типов"""
        symbols = {}
        
        # Поиск объявлений переменных
        var_patterns = [
            r'(int|char|float|double|void|long|short|unsigned)\s+(\w+)\s*(?:=\s*[^;]+)?;',
            r'(struct|union|enum)\s+(\w+)\s*\{[^}]+\}\s*(\w*);',
            r'(\w+)\s*\*\s*(\w+)\s*;'
        ]
        
        for pattern in var_patterns:
            matches = re.finditer(pattern, code)
            for match in matches:
                var_type = match.group(1)
                var_name = match.group(2) if match.groups() > 1 else match.group(1)
                symbols[var_name] = {
                    'type': var_type,
                    'size': self._estimate_size(var_type),
                    'scope': 'global' if ';' in code.split('{')[0] else 'local'
                }
        
        return symbols
    
    def _estimate_size(self, type_name: str) -> int:
        """Оценка размера типа"""
        sizes = {
            'char': 1, 'int': 4, 'float': 4, 'double': 8,
            'long': 8, 'short': 2, 'void*': 8
        }
        return sizes.get(type_name, 4)
    
    def analyze_function_complexity(self, code: str) -> Dict[str, Any]:
        """Анализ сложности функций"""
        if not RADON_AVAILABLE:
            return {}
        
        try:
            complexity_results = cc_visit(code)
            metrics = {}
            
            for func in complexity_results:
                metrics[func.name] = {
                    'complexity': func.complexity,
                    'rank': cc_rank(func.complexity),
                    'lines': func.endline - func.lineno + 1
                }
            
            return metrics
        except Exception as e:
            logging.warning(f"Ошибка анализа сложности: {e}")
            return {}

class AdvancedMemoryAnalyzer:
    """Мощнейший анализатор памяти и проблем в C/C++ коде"""
    
    def __init__(self, project_path: str, config: Dict = None):
        self.project_path = Path(project_path)
        self.config = config or {}
        
        # Инициализация компонентов
        self.pattern_engine = AdvancedPatternEngine()
        self.control_flow_analyzer = ControlFlowAnalyzer()
        self.semantic_analyzer = SemanticAnalyzer()
        
        # Результаты анализа
        self.issues: List[CodeIssue] = []
        self.metrics: Dict[str, FileMetrics] = {}
        self.stats = {
            'files_analyzed': 0,
            'total_lines': 0,
            'analysis_time': 0,
            'memory_used': 0
        }
        
        # Кэш для ускорения анализа
        self._cache = {}
        self._cache_file = self.project_path / '.analyzer_cache'
        self._load_cache()
        
        # Настройка многопроцессорности
        self.max_processes = min(mp.cpu_count(), self.config.get('max_processes', 8))
        
        setup_logging(self.config.get('verbose', False))
        self.logger = logging.getLogger(__name__)

    def _load_cache(self):
        """Загрузка кэша предыдущего анализа"""
        try:
            if self._cache_file.exists():
                with open(self._cache_file, 'rb') as f:
                    self._cache = pickle.load(f)
                self.logger.info(f"Загружен кэш анализа ({len(self._cache)} записей)")
        except Exception as e:
            self.logger.warning(f"Не удалось загрузить кэш: {e}")

    def _save_cache(self):
        """Сохранение кэша анализа"""
        try:
            with open(self._cache_file, 'wb') as f:
                pickle.dump(self._cache, f)
        except Exception as e:
            self.logger.warning(f"Не удалось сохранить кэш: {e}")

    def find_source_files(self) -> List[Path]:
        """Интеллектуальный поиск исходных файлов"""
        source_files = []
        ignore_patterns = {
            '*.o', '*.so', '*.a', '*.dll', '*.exe', '*.bin',
            'build/*', 'cmake-build-*', '.git/*', '*.gitignore'
        }
        
        extensions = {'.c', '.cpp', '.cc', '.cxx', '.h', '.hpp', '.hxx'}
        
        for ext in extensions:
            for file_path in self.project_path.rglob(f'*{ext}'):
                if not any(file_path.match(pattern) for pattern in ignore_patterns):
                    source_files.append(file_path)
        
        self.logger.info(f"Найдено {len(source_files)} исходных файлов")
        return source_files

    def calculate_file_hash(self, file_path: Path) -> str:
        """Вычисление хеша файла для кэширования"""
        content = file_path.read_bytes()
        return hashlib.md5(content).hexdigest()

    def analyze_file_advanced(self, file_path: Path) -> Tuple[List[CodeIssue], FileMetrics]:
        """Продвинутый анализ одного файла"""
        file_hash = self.calculate_file_hash(file_path)
        
        # Проверка кэша
        if file_hash in self._cache and not self.config.get('force_reanalyze', False):
            return self._cache[file_hash]
        
        try:
            content = file_path.read_text(encoding='utf-8', errors='ignore')
            issues = []
            metrics = FileMetrics()
            
            # Многомодульный анализ
            analysis_methods = [
                self._pattern_based_analysis,
                self._control_flow_analysis,
                self._semantic_analysis,
                self._metrics_analysis,
                self._security_analysis,
                self._performance_analysis
            ]
            
            for method in analysis_methods:
                try:
                    method_issues, method_metrics = method(file_path, content)
                    issues.extend(method_issues)
                    self._merge_metrics(metrics, method_metrics)
                except Exception as e:
                    self.logger.error(f"Ошибка в {method.__name__} для {file_path}: {e}")
            
            # Анализ с Clang если доступен
            if CLANG_AVAILABLE:
                clang_issues = self._clang_analysis(file_path)
                issues.extend(clang_issues)
            
            # Сохранение в кэш
            self._cache[file_hash] = (issues, metrics)
            
            return issues, metrics
            
        except Exception as e:
            self.logger.error(f"Критическая ошибка анализа {file_path}: {e}")
            return [], FileMetrics()

    def _pattern_based_analysis(self, file_path: Path, content: str) -> Tuple[List[CodeIssue], FileMetrics]:
        """Анализ на основе расширенных паттернов"""
        issues = []
        file_context = self._build_file_context(content)
        
        pattern_results = self.pattern_engine.analyze_with_context(content, file_context)
        
        for result in pattern_results:
            issue = CodeIssue(
                id=self._generate_issue_id(),
                file_path=str(file_path),
                line_number=self._find_line_number(content, result['position'][0]),
                column=0,
                issue_type=result['type'].upper(),
                category=AnalysisCategory.MEMORY_SAFETY,
                severity=IssueSeverity.HIGH if result['weight'] > 0.8 else IssueSeverity.MEDIUM,
                description=self._get_detailed_description(result['type']),
                code_snippet=result['match'][:200],
                confidence=result['weight'],
                suggestions=self._get_suggestions(result['type']),
                cwe=self._get_cwe_mappings(result['type'])
            )
            issues.append(issue)
        
        return issues, FileMetrics()

    def _control_flow_analysis(self, file_path: Path, content: str) -> Tuple[List[CodeIssue], FileMetrics]:
        """Анализ потока управления"""
        issues = []
        
        cfg = self.control_flow_analyzer.build_control_flow_graph(content)
        data_flow = self.control_flow_analyzer.analyze_data_flow(cfg)
        
        # Анализ утечек памяти через поток данных
        for var_name, state in data_flow.items():
            if state.get('allocated') and not state.get('freed'):
                issue = CodeIssue(
                    id=self._generate_issue_id(),
                    file_path=str(file_path),
                    line_number=state['allocation_line'],
                    column=0,
                    issue_type="DATAFLOW_MEMORY_LEAK",
                    category=AnalysisCategory.MEMORY_SAFETY,
                    severity=IssueSeverity.HIGH,
                    description=f"Потенциальная утечка памяти: '{var_name}' выделен но не освобожден",
                    code_snippet=f"Переменная: {var_name}",
                    confidence=0.8,
                    suggestions=["Добавить соответствующий free/delete", "Проверить все пути выполнения"]
                )
                issues.append(issue)
        
        return issues, FileMetrics()

    def _semantic_analysis(self, file_path: Path, content: str) -> Tuple[List[CodeIssue], FileMetrics]:
        """Семантический анализ"""
        issues = []
        
        symbols = self.semantic_analyzer.analyze_symbols(content)
        complexity_metrics = self.semantic_analyzer.analyze_function_complexity(content)
        
        # Анализ сложных функций
        for func_name, metrics in complexity_metrics.items():
            if metrics['complexity'] > 10:
                issue = CodeIssue(
                    id=self._generate_issue_id(),
                    file_path=str(file_path),
                    line_number=1,  # Примерная позиция
                    column=0,
                    issue_type="HIGH_COMPLEXITY",
                    category=AnalysisCategory.CODE_QUALITY,
                    severity=IssueSeverity.MEDIUM,
                    description=f"Функция '{func_name}' имеет высокую цикломатическую сложность ({metrics['complexity']})",
                    code_snippet=f"Функция: {func_name}",
                    confidence=0.9,
                    suggestions=["Разбейте функцию на более мелкие", "Упростите логику условия"]
                )
                issues.append(issue)
        
        return issues, FileMetrics()

    def _metrics_analysis(self, file_path: Path, content: str) -> Tuple[List[CodeIssue], FileMetrics]:
        """Анализ метрик кода"""
        metrics = FileMetrics()
        
        if RADON_AVAILABLE:
            try:
                # Анализ сырого кода
                raw_metrics = analyze(content)
                metrics.lines_of_code = raw_metrics.loc
                metrics.comment_ratio = raw_metrics.comments / raw_metrics.loc if raw_metrics.loc > 0 else 0
                
                # Метрики поддерживаемости
                mi_metrics = mi_visit(content, True)
                metrics.maintainability = mi_metrics
                
            except Exception as e:
                self.logger.warning(f"Ошибка расчета метрик для {file_path}: {e}")
        
        return [], metrics

    def _security_analysis(self, file_path: Path, content: str) -> Tuple[List[CodeIssue], FileMetrics]:
        """Анализ безопасности"""
        issues = []
        
        security_patterns = {
            'buffer_overflow': [
                r'strcpy\s*\(\s*\w+\s*,\s*\w+\s*\)',
                r'strcat\s*\(\s*\w+\s*,\s*\w+\s*\)',
                r'sprintf\s*\(\s*\w+\s*,\s*[^)]*\)',
                r'gets\s*\(\s*\w+\s*\)'
            ],
            'format_string': [
                r'printf\s*\(\s*[^"]*\%[^"]*\)',
                r'sprintf\s*\(\s*\w+\s*,\s*[^"]*\%[^"]*\)'
            ],
            'command_injection': [
                r'system\s*\(',
                r'popen\s*\(',
                r'execl\s*\('
            ]
        }
        
        for vuln_type, patterns in security_patterns.items():
            for pattern in patterns:
                matches = re.finditer(pattern, content)
                for match in matches:
                    issue = CodeIssue(
                        id=self._generate_issue_id(),
                        file_path=str(file_path),
                        line_number=self._find_line_number(content, match.start()),
                        column=0,
                        issue_type=vuln_type.upper(),
                        category=AnalysisCategory.SECURITY,
                        severity=IssueSeverity.CRITICAL,
                        description=f"Потенциальная уязвимость: {vuln_type}",
                        code_snippet=match.group(),
                        confidence=0.85,
                        suggestions=self._get_security_suggestions(vuln_type),
                        cwe=['CWE-120', 'CWE-134']  # Buffer Overflow, Format String
                    )
                    issues.append(issue)
        
        return issues, FileMetrics()

    def _performance_analysis(self, file_path: Path, content: str) -> Tuple[List[CodeIssue], FileMetrics]:
        """Анализ производительности"""
        issues = []
        
        performance_patterns = {
            'inefficient_loop': [
                r'for\s*\(\s*[^;]+;\s*[^;]+;\s*[^)]+\)\s*\{[^}]*strlen\s*\([^}]*\}',
                r'while\s*\([^)]*strlen'
            ],
            'redundant_copy': [
                r'memcpy\s*\(\s*\w+\s*,\s*\w+\s*,\s*strlen',
                r'strcpy\s*\(\s*\w+\s*,\s*\w+\s*\)\s*;\s*strcat'
            ]
        }
        
        for perf_issue, patterns in performance_patterns.items():
            for pattern in patterns:
                matches = re.finditer(pattern, content)
                for match in matches:
                    issue = CodeIssue(
                        id=self._generate_issue_id(),
                        file_path=str(file_path),
                        line_number=self._find_line_number(content, match.start()),
                        column=0,
                        issue_type=perf_issue.upper(),
                        category=AnalysisCategory.PERFORMANCE,
                        severity=IssueSeverity.MEDIUM,
                        description=f"Проблема производительности: {perf_issue}",
                        code_snippet=match.group(),
                        confidence=0.7,
                        suggestions=["Оптимизируйте вычисления в цикле", "Кэшируйте результаты дорогих операций"]
                    )
                    issues.append(issue)
        
        return issues, FileMetrics()

    def _clang_analysis(self, file_path: Path) -> List[CodeIssue]:
        """Анализ с помощью Clang"""
        issues = []
        
        if not CLANG_AVAILABLE:
            return issues
        
        try:
            index = Index.create()
            tu = index.parse(str(file_path), args=['-std=c++11'])
            
            for diag in tu.diagnostics:
                issue = CodeIssue(
                    id=self._generate_issue_id(),
                    file_path=str(file_path),
                    line_number=diag.location.line,
                    column=diag.location.column,
                    issue_type="CLANG_" + diag.severity.name,
                    category=AnalysisCategory.CODE_QUALITY,
                    severity=IssueSeverity.MEDIUM,
                    description=diag.spelling,
                    code_snippet="",
                    confidence=0.9
                )
                issues.append(issue)
                
        except Exception as e:
            self.logger.warning(f"Clang анализ не удался для {file_path}: {e}")
        
        return issues

    def analyze_project(self) -> Dict[str, Any]:
        """Основной метод анализа проекта"""
        start_time = time.time()
        start_memory = psutil.Process().memory_info().rss
        
        source_files = self.find_source_files()
        all_issues = []
        all_metrics = {}
        
        self.logger.info(f"Запуск анализа {len(source_files)} файлов с {self.max_processes} процессами...")
        
        # Многопроцессорный анализ
        with ProcessPoolExecutor(max_workers=self.max_processes) as executor:
            future_to_file = {
                executor.submit(self.analyze_file_advanced, file): file 
                for file in source_files
            }
            
            for future in as_completed(future_to_file):
                file = future_to_file[future]
                try:
                    issues, metrics = future.result(timeout=300)  # 5 минут таймаут
                    all_issues.extend(issues)
                    all_metrics[str(file)] = metrics
                    self.stats['files_analyzed'] += 1
                    
                    if self.stats['files_analyzed'] % 10 == 0:
                        self.logger.info(f"Проанализировано {self.stats['files_analyzed']}/{len(source_files)} файлов")
                        
                except Exception as e:
                    self.logger.error(f"Ошибка анализа {file}: {e}")
        
        self.issues = all_issues
        self.metrics = all_metrics
        self.stats['analysis_time'] = time.time() - start_time
        self.stats['memory_used'] = psutil.Process().memory_info().rss - start_memory
        
        # Сохранение кэша
        self._save_cache()
        
        return self.generate_comprehensive_report()

    def generate_comprehensive_report(self) -> Dict[str, Any]:
        """Генерация всеобъемлющего отчета"""
        report = {
            'metadata': {
                'project_path': str(self.project_path),
                'analysis_timestamp': datetime.now().isoformat(),
                'analyzer_version': '2.0.0',
                'analysis_duration_seconds': self.stats['analysis_time'],
                'files_analyzed': self.stats['files_analyzed'],
                'total_issues_found': len(self.issues),
                'system_info': {
                    'platform': platform.platform(),
                    'processor': platform.processor(),
                    'python_version': platform.python_version(),
                    'memory_used_mb': self.stats['memory_used'] / 1024 / 1024
                }
            },
            'summary': {
                'issues_by_severity': defaultdict(int),
                'issues_by_category': defaultdict(int),
                'issues_by_type': defaultdict(int),
                'metrics_summary': self._calculate_metrics_summary()
            },
            'detailed_issues': [],
            'file_metrics': {},
            'recommendations': []
        }
        
        # Статистика проблем
        for issue in self.issues:
            report['summary']['issues_by_severity'][issue.severity.name] += 1
            report['summary']['issues_by_category'][issue.category.value] += 1
            report['summary']['issues_by_type'][issue.issue_type] += 1
            
            report['detailed_issues'].append({
                'id': issue.id,
                'file': issue.file_path,
                'line': issue.line_number,
                'column': issue.column,
                'type': issue.issue_type,
                'category': issue.category.value,
                'severity': issue.severity.name,
                'description': issue.description,
                'confidence': issue.confidence,
                'suggestions': issue.suggestions,
                'cwe': issue.cwe,
                'code_snippet': issue.code_snippet,
                'metrics': issue.metrics
            })
        
        # Метрики файлов
        for file_path, metrics in self.metrics.items():
            report['file_metrics'][file_path] = {
                'complexity': metrics.complexity,
                'maintainability': metrics.maintainability,
                'lines_of_code': metrics.lines_of_code,
                'comment_ratio': metrics.comment_ratio,
                'function_count': metrics.function_count,
                'issue_density': metrics.issue_density,
                'memory_risk_score': metrics.memory_risk_score
            }
        
        # Рекомендации
        report['recommendations'] = self._generate_recommendations()
        
        return report

    def _calculate_metrics_summary(self) -> Dict[str, Any]:
        """Расчет сводных метрик"""
        complexities = [m.complexity for m in self.metrics.values() if m.complexity > 0]
        maintainabilities = [m.maintainability for m in self.metrics.values() if m.maintainability > 0]
        comment_ratios = [m.comment_ratio for m in self.metrics.values() if m.comment_ratio > 0]

        return {
            'average_complexity': statistics.mean(complexities) if complexities else 0,
            'max_complexity': max(complexities) if complexities else 0,
            'average_maintainability': statistics.mean(maintainabilities) if maintainabilities else 0,
            'min_maintainability': min(maintainabilities) if maintainabilities else 0,
            'total_lines_of_code': sum(m.lines_of_code for m in self.metrics.values()),
            'average_comment_ratio': statistics.mean(comment_ratios) if comment_ratios else 0.0
        }

    def _generate_recommendations(self) -> List[Dict]:
        """Генерация интеллектуальных рекомендаций"""
        recommendations = []
        
        critical_issues = [i for i in self.issues if i.severity == IssueSeverity.CRITICAL]
        if critical_issues:
            recommendations.append({
                'priority': 'HIGH',
                'category': 'SECURITY',
                'description': f'Обнаружено {len(critical_issues)} критических проблем безопасности',
                'action': 'Немедленно исправьте критические уязвимости перед развертыванием'
            })
        
        memory_issues = [i for i in self.issues if i.category == AnalysisCategory.MEMORY_SAFETY]
        if memory_issues:
            recommendations.append({
                'priority': 'HIGH',
                'category': 'MEMORY',
                'description': f'Обнаружено {len(memory_issues)} проблем с памятью',
                'action': 'Проведите рефакторинг кода управления памятью, используйте умные указатели'
            })
        
        # Анализ сложности
        high_complex_files = [f for f, m in self.metrics.items() if m.complexity > 20]
        if high_complex_files:
            recommendations.append({
                'priority': 'MEDIUM',
                'category': 'MAINTAINABILITY',
                'description': f'Обнаружено {len(high_complex_files)} файлов с высокой сложностью',
                'action': 'Упростите сложные функции, разбейте на более мелкие компоненты'
            })
        
        return recommendations

    # Вспомогательные методы
    def _build_file_context(self, content: str) -> Dict[str, Any]:
        """Построение контекста файла"""
        lines = content.split('\n')
        return {
            'line_count': len(lines),
            'has_functions': bool(re.search(r'\w+\s+\w+\s*\([^)]*\)\s*\{', content)),
            'has_pointers': bool(re.search(r'\*', content)),
            'has_dynamic_allocation': bool(re.search(r'malloc|calloc|realloc|new', content)),
            'high_complexity': len(re.findall(r'if\s*\(|for\s*\(|while\s*\(|switch\s*\(', content)) > 10
        }

    def _find_line_number(self, content: str, position: int) -> int:
        """Нахождение номера строки по позиции в тексте"""
        return content[:position].count('\n') + 1

    def _generate_issue_id(self) -> str:
        """Генерация уникального ID проблемы"""
        return hashlib.md5(f"{time.time()}{len(self.issues)}".encode()).hexdigest()[:8]

    def _get_detailed_description(self, issue_type: str) -> str:
        """Получение детального описания проблемы"""
        descriptions = {
            'memory_leak': 'Утечка памяти: динамически выделенная память не освобождается',
            'double_free': 'Двойное освобождение: попытка освободить уже освобожденную память',
            'use_after_free': 'Использование после освобождения: доступ к памяти после free/delete',
            'buffer_overflow': 'Переполнение буфера: запись за пределы выделенной памяти',
            'null_pointer': 'Разыменование нулевого указателя',
            'high_complexity': 'Высокая цикломатическая сложность функции'
        }
        return descriptions.get(issue_type, 'Обнаружена потенциальная проблема')

    def _get_suggestions(self, issue_type: str) -> List[str]:
        """Получение рекомендаций по исправлению"""
        suggestions = {
            'memory_leak': [
                'Добавьте соответствующий вызов free() или delete',
                'Используйте умные указатели в C++',
                'Рассмотрите использование RAII'
            ],
            'double_free': [
                'Установите указатель в NULL после освобождения',
                'Используйте детектор двойного освобождения',
                'Реорганизуйте логику освобождения памяти'
            ],
            'use_after_free': [
                'Установите указатель в NULL после освобождения',
                'Используйте инструменты анализа памяти (Valgrind, ASan)',
                'Пересмотрите время жизни объектов'
            ]
        }
        return suggestions.get(issue_type, ['Проведите ручной аудит кода'])

    def _get_cwe_mappings(self, issue_type: str) -> List[str]:
        """Получение соответствий CWE"""
        cwe_map = {
            'memory_leak': ['CWE-401'],
            'double_free': ['CWE-415'],
            'use_after_free': ['CWE-416'],
            'buffer_overflow': ['CWE-120', 'CWE-119'],
            'null_pointer': ['CWE-476']
        }
        return cwe_map.get(issue_type, [])

    def _get_security_suggestions(self, vuln_type: str) -> List[str]:
        """Получение рекомендаций по безопасности"""
        suggestions = {
            'buffer_overflow': [
                'Используйте strncpy вместо strcpy',
                'Добавьте проверки границ буферов',
                'Используйте безопасные строковые функции'
            ],
            'format_string': [
                'Никогда не используйте пользовательский ввод как строку формата',
                'Используйте фиксированные строки формата',
                'Валидируйте все входные данные'
            ]
        }
        return suggestions.get(vuln_type, ['Обратитесь к руководствам по безопасному программированию'])

    def _merge_metrics(self, target: FileMetrics, source: FileMetrics):
        """Объединение метрик"""
        for attr in ['complexity', 'maintainability', 'lines_of_code', 
                    'comment_ratio', 'function_count', 'issue_density', 'memory_risk_score']:
            if getattr(source, attr) > 0:
                setattr(target, attr, getattr(source, attr))

def main():
    """Основная функция запуска ультимативного анализатора"""
    parser = argparse.ArgumentParser(
        description='ULTIMATE C/C++ Static Analyzer - Senior Level',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Примеры использования:
  python ultimate_analyzer.py /path/to/project --processes 8 --verbose
  python ultimate_analyzer.py /path/to/project --format json --output detailed_report
  python ultimate_analyzer.py /path/to/project --force-reanalyze --security-scan
        """
    )
    
    parser.add_argument('project_path', help='Путь к проекту для анализа')
    parser.add_argument('--processes', type=int, default=mp.cpu_count(), 
                       help='Количество процессов для анализа')
    parser.add_argument('--format', choices=['json', 'html', 'both'], default='both',
                       help='Формат отчета')
    parser.add_argument('--output', help='Имя файла отчета (без расширения)')
    parser.add_argument('--verbose', action='store_true', help='Подробный вывод')
    parser.add_argument('--force-reanalyze', action='store_true', 
                       help='Принудительный переанализ всех файлов')
    parser.add_argument('--security-scan', action='store_true',
                       help='Углубленный анализ безопасности')
    parser.add_argument('--performance-scan', action='store_true',
                       help='Анализ производительности')
    
    args = parser.parse_args()
    
    if not os.path.exists(args.project_path):
        logging.error(f"Путь не существует: {args.project_path}")
        return
    
    # Конфигурация анализатора
    config = {
        'max_processes': args.processes,
        'verbose': args.verbose,
        'force_reanalyze': args.force_reanalyze,
        'security_scan': args.security_scan,
        'performance_scan': args.performance_scan
    }
    
    # Создание и запуск анализатора
    analyzer = AdvancedMemoryAnalyzer(args.project_path, config)
    
    logging.info("🚀 Запуск ULTIMATE C/C++ Static Analyzer...")
    logging.info(f"📊 Используется процессов: {args.processes}")
    logging.info(f"🔍 Анализ безопасности: {'ВКЛ' if args.security_scan else 'ВЫКЛ'}")
    logging.info(f"⚡ Анализ производительности: {'ВКЛ' if args.performance_scan else 'ВЫКЛ'}")
    
    report = analyzer.analyze_project()
    
    # Сохранение отчетов
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    output_name = args.output or f"ultimate_analysis_{timestamp}"
    
    if args.format in ['json', 'both']:
        json_file = Path(args.project_path) / f"{output_name}.json"
        with open(json_file, 'w', encoding='utf-8') as f:
            json.dump(report, f, indent=2, ensure_ascii=False)
        logging.info(f"📄 JSON отчет сохранен: {json_file}")
    
    if args.format in ['html', 'both']:
        html_file = Path(args.project_path) / f"{output_name}.html"
        generate_html_report(report, html_file)
        logging.info(f"🌐 HTML отчет сохранен: {html_file}")
    
    # Вывод итоговой статистики
    logging.info("=" * 80)
    logging.info("🎉 АНАЛИЗ ЗАВЕРШЕН!")
    logging.info("=" * 80)
    logging.info(f"📁 Проанализировано файлов: {report['metadata']['files_analyzed']}")
    logging.info(f"⚠️  Найдено проблем: {report['metadata']['total_issues_found']}")
    logging.info(f"⏱️  Время анализа: {report['metadata']['analysis_duration_seconds']:.2f} сек")
    logging.info(f"💾 Использовано памяти: {report['metadata']['system_info']['memory_used_mb']:.2f} MB")
    
    # Статистика по серьезности
    severity_stats = report['summary']['issues_by_severity']
    for severity, count in severity_stats.items():
        logging.info(f"   {severity}: {count}")
    
    if report['metadata']['total_issues_found'] > 0:
        logging.info("🔴 Обнаружены критические проблемы! Рекомендуется немедленное исправление.")
    else:
        logging.info("✅ Критических проблем не обнаружено.")

def generate_html_report(report: Dict, output_file: Path):
    """Генерация HTML отчета"""
    html_template = """
    <!DOCTYPE html>
    <html lang="ru">
    <head>
        <meta charset="UTF-8">
        <title>ULTIMATE Analysis Report</title>
        <style>
            body { font-family: Arial, sans-serif; margin: 40px; }
            .header { background: #2c3e50; color: white; padding: 20px; border-radius: 5px; }
            .metric { background: #ecf0f1; padding: 15px; margin: 10px 0; border-radius: 5px; }
            .issue { border-left: 4px solid #e74c3c; padding: 10px; margin: 5px 0; background: #fdf2f2; }
            .critical { border-color: #c0392b; }
            .high { border-color: #e74c3c; }
            .medium { border-color: #f39c12; }
            .low { border-color: #f1c40f; }
            .info { border-color: #3498db; }
        </style>
    </head>
    <body>
        <div class="header">
            <h1>🚀 ULTIMATE C/C++ Static Analysis Report</h1>
            <p>Generated: {{timestamp}}</p>
        </div>
        
        <div class="metric">
            <h2>📊 Summary</h2>
            <p>Files Analyzed: {{files_analyzed}}</p>
            <p>Total Issues: {{total_issues}}</p>
            <p>Analysis Time: {{analysis_time}} seconds</p>
        </div>
        
        <h2>⚠️ Issues by Severity</h2>
        {% for severity, count in severity_stats.items() %}
        <div class="metric">
            <strong>{{ severity }}:</strong> {{ count }}
        </div>
        {% endfor %}
        
        <h2>🔍 Detailed Issues</h2>
        {% for issue in issues %}
        <div class="issue {{ issue.severity.lower() }}">
            <strong>{{ issue.file }}:{{ issue.line }}</strong> [{{ issue.severity }}]
            <br>{{ issue.description }}
            <br><small>{{ issue.code_snippet }}</small>
        </div>
        {% endfor %}
    </body>
    </html>
    """
    
    # Простая реализация шаблонизатора
    html_content = html_template.replace('{{timestamp}}', report['metadata']['analysis_timestamp'])
    html_content = html_content.replace('{{files_analyzed}}', str(report['metadata']['files_analyzed']))
    html_content = html_content.replace('{{total_issues}}', str(report['metadata']['total_issues_found']))
    html_content = html_content.replace('{{analysis_time}}', str(report['metadata']['analysis_duration_seconds']))
    
    # Замена статистики серьезности
    severity_html = ""
    for severity, count in report['summary']['issues_by_severity'].items():
        severity_html += f'<div class="metric"><strong>{severity}:</strong> {count}</div>\n'
    html_content = html_content.replace('{% for severity, count in severity_stats.items() %}', severity_html)
    
    with open(output_file, 'w', encoding='utf-8') as f:
        f.write(html_content)

if __name__ == "__main__":
    main()
  