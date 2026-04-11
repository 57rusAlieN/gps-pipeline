# gps-pipeline

Консольное C++ приложение, обрабатывающее GPS-трек в формате NMEA 0183.
Читает файл построчно, парсит пары предложений `$GPRMC` / `$GPGGA`, прогоняет
точки через настраиваемую цепочку фильтров и выводит результат в stdout.

Разработан по методологии **TDD** (Red -> Green -> Refactor): каждая функциональность
появлялась сначала как провальный тест, затем как минимальная реализация.
Покрыто **109 автотестов** (GoogleTest).

---

## Сборка

### Требования

| Инструмент | Версия |
|---|---|
| CMake | >= 3.14 |
| Компилятор | MSVC 19+ (x64) / GCC 11+ / Clang 14+ |
| Ninja | любая |
| Интернет | для скачивания GoogleTest при первой сборке |

### Команды

```bash
# 1. Сконфигурировать (Debug)
cmake --preset=x64-debug

# 2. Собрать
cmake --build --preset=x64-debug

# или напрямую через Ninja:
cd out/build/x64-debug && ninja
```

> **Windows / MSVC:** запускать команды из _Developer Command Prompt_ (x64), либо
> предварительно выполнить `vcvars64.bat`.

Результат:
- `out/build/x64-debug/gps_pipeline.exe` --- основное приложение
- `out/build/x64-debug/tests/gps_pipeline_tests.exe` --- набор тестов

---

## Запуск

### Демо на тестовых данных

```bash
gps_pipeline data/sample.nmea
```

Ожидаемый вывод:

```
[12:00:00] Coordinates: 55.75206 N, 37.65946 E
           Speed: 46.3 km/h, Course: 45.0
           Altitude: 150.0 m, Satellites: 10, HDOP: 0.8
[12:00:01] Coordinates: 55.75206 N, 37.65946 E
           Speed: 2.2 km/h (stopped), Course: 45.0
           Altitude: 150.0 m, Satellites: 10, HDOP: 0.8
[12:00:02] Point rejected: coordinate jump detected (1893.0 m > 500.0 m)
[12:00:03] Point rejected: insufficient satellites (3 < 4)
[12:00:04] No valid GPS fix
[12:00:05] Parse error: invalid checksum
```

### Автотесты

```bash
# Через CTest (рекомендуется)
ctest --preset=x64-debug --output-on-failure

# Или напрямую
out/build/x64-debug/tests/gps_pipeline_tests.exe
```

Ожидаемый результат: **109/109 passed**.

---

## Архитектура

Приложение построено на принципах SOLID: каждый слой взаимодействует с соседним
только через интерфейс. Зависимости передаются в `Pipeline` через конструктор (DI).

```
NMEA-file (line by line)
         |
         v
 +-------+-------+
 |    Pipeline   |  processLine():
 | (orchestrator)|  1. skip empty lines / comments
 +-------+-------+  2. validate NMEA checksum  -> writeError
         |          3. parser.parse()           -> optional<GpsPoint>
         |          4. filter.process()         -> Pass / Reject
         |          5. output.write*()
         |
   +-----+-----------------------+
   |                             |
   v                             v
INmeaParser                 IGpsFilter (FilterChain)
  |                           +-- SatelliteFilter  (min satellites)
  +-- NmeaParser              +-- SpeedFilter       (max speed)
       stateful RMC+GGA,      +-- CoordinateJumpFilter (Haversine)
       DDMM->decimal,         +-- StopFilter        (low speed flag)
       knots->km/h            +-- LPF [optional]:
                                   MovingAverageFilter (PIF)
                                   FirLowPassFilter    (FIR)
                                         |
                                         v
                                      IOutput
                                        +-- ConsoleOutput
```

### Слои и пакеты

| Пакет | Заголовки | Исходники | Описание |
|---|---|---|---|
| **types** | `GpsPoint.h` | --- | Агрегат данных точки |
| **parser** | `INmeaParser.h`, `NmeaParser.h`, `ChecksumValidator.h` | `NmeaParser.cpp`, `ChecksumValidator.cpp` | Парсинг NMEA, валидация XOR-checksum |
| **filter** | `IGpsFilter.h`, `FilterChain.h`, `*Filter.h` | `*Filter.cpp` | Chain of Responsibility, сглаживающие фильтры |
| **output** | `IOutput.h`, `ConsoleOutput.h` | `ConsoleOutput.cpp` | Форматированный вывод в `std::ostream` |
| **pipeline** | `Pipeline.h` | `Pipeline.cpp` | Оркестратор, composition root в `main.cpp` |

### Тестовые модули

| Файл | Тестов | Что покрывает |
|---|---|---|
| `test_types.cpp` | 9 | GpsPoint, интерфейсы, FilterResult |
| `test_checksum.cpp` | 13 | XOR-вычисление, validate, граничные случаи |
| `test_parser.cpp` | 18 | RMC+GGA stateful, coord/speed конвертация, ошибки |
| `test_filters.cpp` | 39 | Все фильтры, FilterChain, ПИФ/КИХ поведение |
| `test_output.cpp` | 15 | Формат строк, stopped-маркировка, S/W координаты |
| `test_pipeline.cpp` | 12 | Unit + E2E по всем 6 сценариям `sample.nmea` |

---

## Конфигурация

### Параметры командной строки

```
gps_pipeline <nmea_file> [-lpf <pif|fir>] [-cf <0.01..1>]
```

| Параметр | Значение | По умолчанию |
|---|---|---|
| `<nmea_file>` | Путь к NMEA-файлу | _обязателен_ |
| `-lpf pif` | Сглаживание: прямоугольный (moving average) | `pif` |
| `-lpf fir` | Сглаживание: КИХ windowed-sinc, окно Хэмминга | --- |
| `-cf 0.01..1` | Частота среза, нормированная к Найквисту | `0.2` |
| `-cf 1` | Отключить сглаживание (all-pass) | --- |

### Пороги фильтров (задаются в `main.cpp`)

| Фильтр | Параметр | Значение по умолчанию |
|---|---|---|
| `SatelliteFilter` | `minSatellites` | 4 |
| `SpeedFilter` | `maxSpeedKmh` | 200.0 km/h |
| `CoordinateJumpFilter` | `maxDistanceM` | 500.0 m |
| `StopFilter` | `thresholdKmh` | 3.0 km/h |

### Примеры

```bash
# Без сглаживания
gps_pipeline track.nmea -cf 1

# ПИФ, умеренное сглаживание (window = 4 при cf=0.2)
gps_pipeline track.nmea -lpf pif -cf 0.2

# КИХ, сильное сглаживание (21 отвод, fc = 0.1 x Nyquist)
gps_pipeline track.nmea -lpf fir -cf 0.1
```

### Расчёт размера окна ПИФ

```
N = max(3, round(0.886 / fc))
```

Формула соответствует -3 dB точке прямоугольного фильтра (fc нормирована к Найквисту).

### Добавление нового фильтра

1. Создать класс, наследующий `IGpsFilter`
2. Реализовать `FilterResult process(GpsPoint& point)`
3. Добавить `filters.add(std::make_unique<MyFilter>(...))` в `main.cpp`

---

## Формат NMEA

Обрабатываются предложения `$GPRMC` и `$GPGGA` --- одна точка формируется из
совпадающей по времени пары. Строки с `#` и пустые строки игнорируются.

```
$GPRMC,120000,A,5545.1234,N,03739.5678,E,025.0,045.0,270124,,,A*70
$GPGGA,120000,5545.1234,N,03739.5678,E,1,10,0.8,150.0,M,14.0,M,,*4E
```

| Поле NMEA | Преобразование |
|---|---|
| `DDMM.MMMM N/S` | Десятичные градусы; S -> отрицательные |
| `DDDMM.MMMM E/W` | Десятичные градусы; W -> отрицательные |
| Скорость в узлах | km/h (x 1.852) |
| Статус `V` в RMC | "No valid GPS fix" |
| Неверный XOR-checksum | "Parse error: invalid checksum" |
---

---

## Покрытие тестами

Оценка основана на 109 unit-тестах (GTest), покрывающих все методы библиотеки.

main.cpp (~90 строк) является composition root и намеренно не покрывается unit-тестами; его поведение проверяется E2E-тестом test_pipeline.cpp::FullSampleNmeaSequence.

### Оценка по модулям

| Модуль | LOC | Тесты | Оценка |
|---|---|---|---|
| ChecksumValidator | 15 | 13 | 100% |
| NmeaParser | 90 | 18 | ~95% |
| SatelliteFilter | 12 | 3 | 100% |
| SpeedFilter | 12 | 3 | 100% |
| CoordinateJumpFilter | 45 | 6 | ~100% |
| StopFilter | 12 | 3 | 100% |
| FilterChain | 20 | 5 | 100% |
| MovingAverageFilter | 35 | 6 | ~95% |
| FirLowPassFilter | 55 | 8 | ~90% |
| ConsoleOutput | 45 | 15 | ~100% |
| Pipeline | 55 | 12 | ~95% |
| **Итого (библиотека)** | **~396** | **109** | **>95%** |

### coverage-отчёт (Linux / GCC + lcov)

bash
cmake --preset=linux-coverage
cmake --build --preset=linux-coverage --target coverage
xdg-open out/build/linux-coverage/coverage_html/index.html

Пресет linux-coverage добавляет флаги --coverage -O0, запускает lcov и genhtml,
отчёт появляется в out/build/linux-coverage/coverage_html/.
