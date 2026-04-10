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

### Этап 3 — Парсер NMEA (TDD) `[ ]`
- [ ] Тест: `test_parser.cpp` — RMC: валидный, невалидный fix, отсутствующие поля
- [ ] Тест: GGA: валидный, нет fix quality, конвертация координат DDMM→decimal
- [ ] Тест: неизвестный тип сообщения → `std::nullopt`
- [ ] `include/parser/NmeaParser.h` + `src/parser/NmeaParser.cpp`
- [ ] Тесты зелёные

### Этап 4 — Фильтры (TDD) `[ ]`
- [ ] Тест + реализация `SatelliteFilter` (мин. количество спутников, дефолт 4)
- [ ] Тест + реализация `SpeedFilter` (макс. скорость, дефолт 200 км/ч)
- [ ] Тест + реализация `CoordinateJumpFilter` (макс. расстояние между точками)
- [ ] Тест + реализация `StopFilter` (скорость < порога → помечать как "stopped")
- [ ] Тест + реализация `FilterChain` (цепочка с приоритетами, первый reject выигрывает)
- [ ] `include/filter/FilterChain.h` + `src/filter/FilterChain.cpp`

### Этап 5 — Output (TDD) `[ ]`
- [ ] `include/output/IOutput.h` — интерфейс
- [ ] Тест: `test_output.cpp` — форматирование строки для pass/reject/error
- [ ] `include/output/ConsoleOutput.h` + `src/output/ConsoleOutput.cpp`
- [ ] Mock-реализация `MockOutput` в `tests/` для изоляции тестов

### Этап 6 — Pipeline (TDD) `[ ]`
- [ ] Тест: `test_pipeline.cpp` — E2E: подать строки, получить ожидаемый вывод (через MockOutput)
- [ ] `include/pipeline/Pipeline.h` + `src/pipeline/Pipeline.cpp`
- [ ] DI через конструктор: `Pipeline(INmeaParser&, std::vector<IGpsFilter*>, IOutput&)`
- [ ] Тесты зелёные

### Этап 7 — Main / CLI `[ ]`
- [ ] `src/main.cpp` — чтение файла построчно, создание Pipeline с реальными зависимостями
- [ ] Принимает путь к NMEA-файлу как аргумент командной строки
- [ ] Обработка ошибок: файл не найден, пустой файл

### Этап 8 — Тестовые данные `[✅]`
- [x] `data/sample.nmea` — NMEA-последовательность из ТЗ с корректно рассчитанными checksum
- [ ] Проверить вывод вручную на соответствие примеру из ТЗ (после Stage 7)

### Этап 9 — README.md `[ ]`
- [ ] Описание проекта (1-2 абзаца)
- [ ] Команды сборки
- [ ] Команды запуска (demo + tests)
- [ ] Архитектура (краткое описание слоёв)
- [ ] Конфигурация фильтров

### Этап 10 — Финальная проверка `[ ]`
- [ ] `ctest --output-on-failure` — все тесты зелёные
- [ ] Покрытие ≥80% (gcov/llvm-cov)
- [ ] Осмысленные commit messages, видна TDD-практика в истории

---

## Бонусные этапы (опционально)
- [ ] Парсинг GSV (спутники в зоне видимости)
- [ ] `SmoothinigFilter` с настраиваемой частотой среза
- [ ] JSON-конфигурация пайплайна (nlohmann/json через FetchContent)
- [ ] Вывод в файл с ротацией (`FileOutput`)

---

## Прогресс

| Этап | Статус | Коммит |
|------|--------|--------|
| 0 — Scaffolding    | ✅ Завершён | — |
| 1 — Типы данных    | ✅ Завершён | a74a51b |
| 2 — Checksum       | ✅ Завершён | ab1edb0 |
| 3 — Парсер NMEA    | ⬜ Не начат | — |
| 4 — Фильтры        | ⬜ Не начат | — |
| 5 — Output         | ⬜ Не начат | — |
| 6 — Pipeline       | ⬜ Не начат | — |
| 7 — Main / CLI     | ⬜ Не начат | — |
| 8 — Тестовые данные| ✅ Завершён | — |
| 9 — README.md      | ⬜ Не начат | — |
| 10 — Финал         | ⬜ Не начат | — |

Обозначения: ⬜ Не начат · 🔄 В работе · ✅ Завершён · ❌ Заблокирован
