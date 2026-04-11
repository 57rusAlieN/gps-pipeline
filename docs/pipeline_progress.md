# GPS Pipeline — Анализ ТЗ и план разработки

## Анализ технического задания

### Что нужно сделать
Консольное C++20-приложение, которое читает NMEA-файл и прогоняет точки через
цепочку фильтров, выводя результат в stdout.

### Критерии оценки (по весам)
| Критерий   | Вес | Ключевое требование |
|------------|-----|---------------------|
| SOLID      | 25% | Расширяемость, абстракции, разделение слоёв |
| TDD        | 25% | История коммитов показывает test-first |
| Тесты      | 20% | Покрытие ≥80%, все этапы + граничные случаи + моки |
| Архитектура| 15% | Чёткие границы между этапами, DI |
| Код        | 15% | Именование, читаемость, обработка ошибок |

**Вывод:** TDD — главный приоритет. Каждый коммит должен добавлять тест ДО реализации.

---

## Архитектурные решения

### Слои
```
NMEA-файл
    │
    ▼
┌─────────────┐
│   Parser    │  INmeaParser → NmeaParser
│             │  Checksum, RMC, GGA → std::optional<GpsPoint>
└──────┬──────┘
       │
       ▼
┌─────────────┐
│  Filters    │  IGpsFilter (chain of responsibility)
│             │  SatelliteFilter, SpeedFilter,
│             │  CoordinateJumpFilter, StopFilter
└──────┬──────┘
       │
       ▼
┌─────────────┐
│   Output    │  IOutput → ConsoleOutput / MockOutput
└─────────────┘
       │
       ▼
┌─────────────┐
│  Pipeline   │  Оркестратор (принимает интерфейсы через DI)
└─────────────┘
```

### Ключевые типы
- `GpsPoint` — агрегат данных точки (координаты, скорость, курс, высота, спутники, HDOP, время)
- `ParseResult` = `std::variant<GpsPoint, ParseError>` — результат парсинга
- `FilterResult` — enum `{Pass, Reject}` + строка причины
- `INmeaParser` — интерфейс парсера
- `IGpsFilter` — интерфейс фильтра, `FilterChain` — цепочка с приоритетами
- `IOutput` — интерфейс вывода

### Тестовый фреймворк
GoogleTest (подключается через CMake `FetchContent`).

### Структура директорий
```
gps-pipeline/
├── CMakeLists.txt
├── README.md
├── src/
│   ├── parser/
│   │   ├── NmeaParser.cpp
│   │   └── ChecksumValidator.cpp
│   ├── filter/
│   │   ├── FilterChain.cpp
│   │   ├── SatelliteFilter.cpp
│   │   ├── SpeedFilter.cpp
│   │   ├── CoordinateJumpFilter.cpp
│   │   └── StopFilter.cpp
│   ├── output/
│   │   └── ConsoleOutput.cpp
│   └── pipeline/
│       └── Pipeline.cpp
├── include/
│   ├── GpsPoint.h
│   ├── parser/
│   │   ├── INmeaParser.h
│   │   ├── NmeaParser.h
│   │   └── ChecksumValidator.h
│   ├── filter/
│   │   ├── IGpsFilter.h
│   │   ├── FilterChain.h
│   │   ├── SatelliteFilter.h
│   │   ├── SpeedFilter.h
│   │   ├── CoordinateJumpFilter.h
│   │   └── StopFilter.h
│   ├── output/
│   │   ├── IOutput.h
│   │   └── ConsoleOutput.h
│   └── pipeline/
│       └── Pipeline.h
├── tests/
│   ├── CMakeLists.txt
│   ├── test_checksum.cpp
│   ├── test_parser.cpp
│   ├── test_filters.cpp
│   ├── test_output.cpp
│   └── test_pipeline.cpp
├── data/
│   └── sample.nmea
└── docs/
    ├── cpp_test_assignment (1).docx
    └── pipeline_progress.md
```

---

## Поэтапный план разработки

### Этап 0 — Scaffolding (инфраструктура) `[✅]`
- [x] Создать структуру директорий `src/`, `include/`, `tests/`, `data/`
- [x] Переработать `CMakeLists.txt`: библиотека `gps_pipeline_lib` + exe + тесты
- [x] Подключить GoogleTest через `FetchContent`
- [x] Убедиться, что пустые тесты собираются и `ctest` проходит
- [x] Удалить/переместить `gps-pipeline.cpp` / `gps-pipeline.h` в новую структуру

### Этап 1 — Типы данных `[✅]`
- [x] `include/GpsPoint.h` — struct `GpsPoint` (lat, lon, speed, course, altitude, satellites, hdop, time)
- [x] `include/parser/INmeaParser.h` — интерфейс парсера
- [x] `include/filter/IGpsFilter.h` — интерфейс фильтра + `FilterStatus` + `FilterResult`
- [x] `include/output/IOutput.h` — интерфейс вывода
- [x] Тест: `tests/test_types.cpp` — 9 тестов, 10/10 GREEN

### Этап 2 — Checksum (TDD) `[✅]`
- [x] Тест: `test_checksum.cpp` — 13 тестов (compute, validate, edge cases)
- [x] `include/parser/ChecksumValidator.h` + `src/parser/ChecksumValidator.cpp`
- [x] Тесты зелёные (23/23)

### Этап 3 — Парсер NMEA (TDD) `[✅]`
- [x] Тест: `tests/test_parser.cpp` — 17 тестов
- [x] `include/parser/NmeaParser.h` + `src/parser/NmeaParser.cpp`
- [x] RMC+GGA stateful парсер, конвертация DDMM→decimal, knots→km/h
- [x] Тесты зелёные (42/42)

### Этап 4 — Фильтры (TDD) `[✅]`
- [x] `SatelliteFilter` — отвергает при satellites < min (default 4)
- [x] `SpeedFilter` — отвергает при speed > max (default 200 км/ч)
- [x] `CoordinateJumpFilter` — stateful, формула гаверсинуса, default 500 м
- [x] `StopFilter` — устанавливает point.stopped=true, не отвергает
- [x] `FilterChain` — Composite, первый Reject останавливает цепочку
- [x] `MovingAverageFilter` (ПИФ) — прямоугольный фильтр, windowSize
- [x] `FirLowPassFilter` (КИХ) — windowed-sinc, Частота Найквиста-нормированная, окно Хэмминга
- [x] Тесты зелёные (83/83)

### Этап 5 — Output (TDD) `[✅]`
- [x] `include/output/IOutput.h` — интерфейс (Стаге 1)
- [x] `tests/test_output.cpp` — 15 тестов (format/stopped/S-W/rejected/error)
- [x] `include/output/ConsoleOutput.h` + `src/output/ConsoleOutput.cpp`
- [x] ConsoleOutput пишет в любой std::ostream, формат по ТЗ
- [x] Тесты зелёные (98/98)

### Этап 6 — Pipeline (TDD) `[✅]`
- [x] `tests/test_pipeline.cpp` — 12 тестов: unit + E2E по всем 6 сценариям sample.nmea
- [x] `include/pipeline/Pipeline.h` + `src/pipeline/Pipeline.cpp`
- [x] DI: `Pipeline(INmeaParser&, IGpsFilter&, IOutput&)`
- [x] processLine: checksum → parse → filter → output
- [x] Тесты зелёные (109/109)

### Этап 7 — Main / CLI `[✅]`
- [x] `src/main.cpp` — composition root: чтение файла, wiring всех зависимостей
- [x] Аргумент `argv[1]`, ошибки: нет аргумента / файл не открыт
- [x] `SetConsoleOutputCP(CP_UTF8)` + `_O_BINARY` для правильного UTF-8 вывода
- [x] `/utf-8` флаг MSVC для правильной кодировки `°` в строковых литералах
- [x] Smoke-тест на sample.nmea — вывод совпадает с ТЗ до символа

### Этап 8 — Тестовые данные `[✅]`
- [x] `data/sample.nmea` — все 6 сценариев, checksum верифицированы
- [x] Ручная проверка вывода — совпадает с примером из ТЗ

### Этап 9 — README.md `[✅]`
- [x] Описание проекта (1-2 абзаца)
- [x] Команды сборки
- [x] Команды запуска (demo + tests)
- [x] Архитектура: ASCII-диаграмма слоёв, таблица пакетов и тестов
- [x] Конфигурация фильтров (CLI + пороги + примеры + формула окна ПИФ)

### Этап 10 — Финальная проверка `[✅]`
- [x] `ctest --preset=x64-debug --output-on-failure` — 109/109 зелёных
- [x] Покрытие: >95% библиотечного кода (unit), main.cpp — composition root, покрыт E2E
- [x] coverage-таргет для Linux/GCC+lcov (пресет linux-coverage)
- [x] Осмысленные commit messages, TDD-цикл RED/GREEN виден в истории

---

## Бонусные этапы (опционально)
- [ ] Парсинг GSV (спутники в зоне видимости)
- [x] `SmoothingFilter` с настраиваемой частотой среза (-lpf/-cf CLI-флаги)
- [ ] JSON-конфигурация пайплайна (nlohmann/json через FetchContent)
- [ ] Вывод в файл с ротацией (`FileOutput`)

---

## Прогресс

| Этап | Статус | Коммит |
|------|--------|--------|
| 0 — Scaffolding    | ✅ Завершён | — |
| 1 — Типы данных    | ✅ Завершён | a74a51b |
| 2 — Checksum       | ✅ Завершён | ab1edb0 |
| 3 — Парсер NMEA    | ✅ Завершён | fa2e076 |
| 4 — Фильтры        | ✅ Завершён | edc3636 |
| 5 — Output         | ✅ Завершён | 55021f3 |
| 6 — Pipeline       | ✅ Завершён | 3d01b27 |
| 7 — Main / CLI     | ✅ Завершён | 73ce0c2 |
| 8 — Тестовые данные| ✅ Завершён | — |
| 9 — README.md      | ✅ Завершён | df9045e |
| 10 — Финал         | ✅ Завершён | HEAD |

Обозначения: ⬜ Не начат · 🔄 В работе · ✅ Завершён · ❌ Заблокирован
