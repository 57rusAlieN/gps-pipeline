# GPS Pipeline — Контекст рабочей сессии

> Файл обновляется при каждом изменении исходников или документации.
> Используется для аварийного восстановления / переноса беседы в новый чат.

---

## Репозиторий

- **GitHub:** https://github.com/57rusAlieN/GalileoSky_gps-pipeline
- **Ветка:** `main`
- **Локальный путь:** `F:\_projects\GalileoSky\gps-pipeline\`

---

## Окружение

| Параметр | Значение |
|---|---|
| IDE | Visual Studio Community 2026 (18.4.3) |
| Компилятор | MSVC 19.50 (x64) |
| CMake | 4.2.3-msvc3, генератор Ninja |
| Стандарт | C++20 |
| Тест-фреймворк | GoogleTest v1.14.0 (FetchContent) |
| Shell | pwsh.exe |

**Важно:** cmake/build/ctest нужно запускать через:
```
cmd /c "call ""C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"" && cd /d <root> && <cmake-command>"
```

---

## Структура проекта (текущая)

```
gps-pipeline/
├── CMakeLists.txt          # библиотека gps_pipeline_lib + exe gps_pipeline + тесты
├── CMakePresets.json       # x64-debug / x64-release (Ninja, cl.exe)
├── .gitignore              # исключены: .vs/, out/, build/, *.sln*, *.vcxproj*
├── .gitattributes          # text=auto, eol=lf для cpp/h/md/cmake/json/nmea
├── README.md               # заглушка
├── src/
│   ├── main.cpp            # заглушка (Stage 7)
│   └── placeholder.cpp     # пустой исходник для компиляции lib (удалить на Stage 1)
├── include/
│   ├── GpsPoint.h              ✅ struct GpsPoint
│   ├── parser/
│   │   └── INmeaParser.h       ✅ интерфейс парсера
│   ├── filter/
│   │   └── IGpsFilter.h        ✅ FilterStatus, FilterResult, IGpsFilter
│   ├── output/
│   │   └── IOutput.h           ✅ интерфейс вывода
│   └── pipeline/               # пусто (Stage 6)
├── tests/
│   ├── CMakeLists.txt          # GoogleTest FetchContent, gps_pipeline_tests
│   ├── test_placeholder.cpp    # тривиальный тест scaffold
│   └── test_types.cpp          ✅ 9 тестов (GpsPoint, FilterResult, интерфейсы)
├── data/
│   └── sample.nmea         # 6 сценариев, checksum рассчитаны
└── docs/
    ├── cpp_test_assignment (1).docx
    ├── pipeline_progress.md
    └── workflow_context.md  # этот файл
```

---

## Команды сборки

```bash
# Configure
cmake --preset=x64-debug

# Build
cmake --build --preset=x64-debug

# Test
ctest --preset=x64-debug --output-on-failure
```

---

## Текущий прогресс

| Этап | Статус | Описание |
|---|---|---|
| **0 — Scaffolding** | ✅ Завершён | CMake + GoogleTest, структура директорий, sample.nmea |
| **1 — Типы данных** | ✅ Завершён | GpsPoint, FilterStatus/Result, IGpsFilter, IOutput, INmeaParser |
| **2 — Checksum** | ⬜ Не начат | TDD |
| **3 — Parser NMEA** | ⬜ Не начат | RMC, GGA, конвертация |
| **4 — Filters** | ⬜ Не начат | 4 фильтра + FilterChain |
| **5 — Output** | ⬜ Не начат | ConsoleOutput + MockOutput |
| **6 — Pipeline** | ⬜ Не начат | оркестратор, DI |
| **7 — Main/CLI** | ⬜ Не начат | чтение файла, аргументы |
| **8 — sample.nmea** | ✅ Завершён | создан вместе со Stage 0 |
| **9 — README** | ⬜ Не начат | — |
| **10 — Финал** | ⬜ Не начат | ctest, покрытие |

---

## Ключевые архитектурные решения

- **`GpsPoint`** — агрегат: `double lat, lon, speed_kmh, course, altitude; int satellites; double hdop; std::string time`
- **`std::optional<GpsPoint>`** — возврат из парсера (nullopt = ошибка / нет данных)
- **`FilterResult`** — `enum class Status { Pass, Reject }` + `std::string reason`
- **`IGpsFilter`** — чистый виртуальный интерфейс, `FilterChain` — цепочка с приоритетами
- **`IOutput`** — интерфейс вывода; `MockOutput` в `tests/` для изоляции
- **`Pipeline`** — DI через конструктор: `(INmeaParser&, std::vector<IGpsFilter*>, IOutput&)`
- **Библиотека `gps_pipeline_lib`** — вся логика; `main.cpp` только парсит аргументы и создаёт объекты

## TDD-практика

Порядок коммитов для каждого этапа:
1. `test: add failing tests for <компонент>` — красный тест
2. `feat: implement <компонент>` — минимальная реализация, тесты зелёные
3. `refactor: clean up <компонент>` — рефакторинг без изменения поведения

## sample.nmea — сценарии

| Время | Сценарий | Ожидаемый результат |
|---|---|---|
| 120000 | Нормальное движение | Pass, вывод координат/скорости |
| 120001 | Низкая скорость (1.2 узла) | Pass, пометка "stopped" (StopFilter) |
| 120002 | Скачок координат | Reject: coordinate jump |
| 120003 | 3 спутника (< 4) | Reject: insufficient satellites |
| 120004 | Status=V (no fix) | ParseError / nullopt |
| 120005 | Неверная checksum | ParseError: invalid checksum |

---

## Следующий шаг

**Этап 2 — Checksum-валидатор (TDD):**
- `tests/test_checksum.cpp` — тесты сначала: валидный checksum, невалидный, пустая строка, без `*`, без `$`
- `include/parser/ChecksumValidator.h` + `src/parser/ChecksumValidator.cpp`
- Алгоритм: XOR всех байт между `$` и `*` (не включая их), результат — двузначный HEX
- Добавить `src/parser/ChecksumValidator.cpp` в `gps_pipeline_lib` в `CMakeLists.txt`
