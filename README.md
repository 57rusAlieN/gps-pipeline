# gps-pipeline

Консольное C++ приложение, обрабатывающее GPS-трек в формате NMEA 0183.
Читает файл построчно, парсит пары предложений `$GPRMC` / `$GPGGA`, прогоняет
точки через настраиваемую цепочку фильтров и выводит результат в stdout.

Разработан по методологии **TDD** (Red -> Green -> Refactor): каждая функциональность
появлялась сначала как провальный тест, затем как минимальная реализация.
Покрыто **137 автотестов** (GoogleTest).

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

Ожидаемый результат: **137/137 passed**.

---

## Архитектура

Приложение построено на принципах SOLID: каждый слой взаимодействует с соседним
только через интерфейс. Зависимости передаются в `Pipeline` через конструктор (DI).

```
NMEA-file (line by line)
         |
         v
 +-------+-------+     +----------------+
 |    Pipeline   |<----|  ConfigLoader  |<-- --config / CLI
 | (orchestrator)|     | config/        |
 +-------+-------+     +----------------+
         |          processLine():
         |          1. skip empty lines / comments
         |          2. validate NMEA checksum  -> writeError
         |          3. parser.parse()           -> optional<GpsPoint>
         |          4. filter.process()         -> Pass / Reject
         |          5. output.write*()
         |
   +-----+-------------------------------+
   |                                     |
   v                                     v
INmeaParser                         IGpsFilter (FilterChain)
  |                                   +-- SatelliteFilter
  +-- NmeaParser                      +-- SpeedFilter
       RMC + GGA (stateful),          +-- CoordinateJumpFilter (Haversine)
       GPGSV -> sat. in view,         +-- StopFilter
       DDMM->decimal, kn->km/h        +-- LPF [optional]:
                                           MovingAverageFilter (PIF)
                                           FirLowPassFilter    (FIR)
                                                 |
                                                 v
                                              IOutput
                                                +-- ConsoleOutput  (stdout)
                                                +-- FileOutput     (file + rotation)
```

### Слои и пакеты

| Пакет | Заголовки | Исходники | Описание |
|---|---|---|---|
| **types** | `GpsPoint.h` | --- | Агрегат данных точки + `SatelliteInfo` |
| **parser** | `INmeaParser.h`, `NmeaParser.h`, `ChecksumValidator.h` | `NmeaParser.cpp`, `ChecksumValidator.cpp` | Парсинг NMEA (RMC, GGA, GSV), валидация XOR-checksum |
| **filter** | `IGpsFilter.h`, `FilterChain.h`, `*Filter.h` | `*Filter.cpp` | Chain of Responsibility, сглаживающие фильтры |
| **config** | `Config.h`, `ConfigLoader.h` | `ConfigLoader.cpp` | JSON-конфигурация (nlohmann/json), defaults |
| **output** | `IOutput.h`, `ConsoleOutput.h`, `FileOutput.h` | `ConsoleOutput.cpp`, `FileOutput.cpp` | Форматированный вывод в `std::ostream`, ротация файлов |
| **pipeline** | `Pipeline.h` | `Pipeline.cpp` | Оркестратор, composition root в `main.cpp` |

### Тестовые модули

| Файл | Тестов | Что покрывает |
|---|---|---|
| `test_types.cpp` | 9 | GpsPoint, SatelliteInfo, интерфейсы, FilterResult |
| `test_checksum.cpp` | 13 | XOR-вычисление, validate, граничные случаи |
| `test_parser.cpp` | 24 | RMC+GGA stateful, GPGSV спутники, coord/speed, ошибки |
| `test_filters.cpp` | 39 | Все фильтры, FilterChain, ПИФ/КИХ поведение |
| `test_output.cpp` | 15 | Формат строк, stopped-маркировка, S/W координаты |
| `test_pipeline.cpp` | 12 | Unit + E2E по всем 6 сценариям `sample.nmea` |
| `test_config.cpp` | 14 | JSON defaults, парсинг фильтров и output, ошибки |
| `test_fileoutput.cpp` | 10 | Запись в файл, ротация, ограничение числа файлов |
| **Итого** | **137** | |

---

## Конфигурация

### Параметры командной строки

Параметры задаются двумя способами: **CLI-флагами** (для разовых запусков) или
**JSON-конфиг файлом** (`--config`, воспроизводимо). При наличии `--config`
флаги `-lpf` и `-cf` игнорируются.

```
gps_pipeline <nmea_file> [--config <cfg.json>] [-lpf <pif|fir>] [-cf <0.01..1>]
```

| Параметр | Значение | По умолчанию |
|---|---|---|
| `<nmea_file>` | Путь к NMEA-файлу | _обязателен_ |
| `--config <path>` | JSON-конфигурация (все параметры) | _не задан_ |
| `-lpf pif` | Сглаживание: прямоугольный (moving average) | `pif` |
| `-lpf fir` | Сглаживание: КИХ windowed-sinc, окно Хэмминга | --- |
| `-cf 0.01..1` | Частота среза, нормированная к Найквисту | `0.2` |
| `-cf 1` | Отключить сглаживание (all-pass) | --- |

---

### JSON-конфигурация (`--config`)

JSON-файл содержит секции `filters` и `output`. Все ключи опциональны —
пропущенные сохраняют значение по умолчанию.

```bash
gps_pipeline track.nmea --config config/default.json
```

#### Полная схема

```json
{
  "filters": {
    "satellite": { "enabled": true,  "min_satellites": 4     },
    "speed":     { "enabled": true,  "max_speed_kmh":  200.0 },
    "jump":      { "enabled": true,  "max_distance_m": 500.0 },
    "stop":      { "enabled": true,  "threshold_kmh":  3.0   },
    "lpf": {
      "enabled": false,
      "type":    "pif",
      "cutoff":  0.2
    }
  },
  "output": {
    "type":     "console",
    "path":     "gps_output.log",
    "rotation": {
      "max_size_kb": 1024,
      "max_files":   5
    }
  }
}
```

#### Значения по умолчанию

| JSON-ключ | Тип | Умолчание | Описание |
|---|---|---|---|
| `filters.satellite.enabled` | bool | `true` | Включить фильтр по числу спутников |
| `filters.satellite.min_satellites` | int | `4` | Минимум спутников для валидной точки |
| `filters.speed.enabled` | bool | `true` | Включить фильтр по скорости |
| `filters.speed.max_speed_kmh` | double | `200.0` | Максимальная допустимая скорость (км/ч) |
| `filters.jump.enabled` | bool | `true` | Включить фильтр прыжков координат |
| `filters.jump.max_distance_m` | double | `500.0` | Максимальный шаг между точками (м) |
| `filters.stop.enabled` | bool | `true` | Включить детектор стоянки |
| `filters.stop.threshold_kmh` | double | `3.0` | Порог остановки (км/ч) |
| `filters.lpf.enabled` | bool | `false` | Включить сглаживающий фильтр |
| `filters.lpf.type` | string | `"pif"` | `"pif"` — moving average, `"fir"` — windowed-sinc |
| `filters.lpf.cutoff` | double | `0.2` | Частота среза \[0.01..1\], Найквист-нормированная |
| `output.type` | string | `"console"` | `"console"` или `"file"` |
| `output.path` | string | `"gps_output.log"` | Путь к файлу (только для `type=file`) |
| `output.rotation.max_size_kb` | uint | `1024` | Макс. размер файла в КБ (0 = без ограничения) |
| `output.rotation.max_files` | int | `5` | Кол-во архивных файлов (0 = хранить все) |

---

### Вывод в файл с ротацией

При `"output.type": "file"` данные пишутся в указанный файл. Когда файл
достигает `max_size_kb` КБ, он переименовывается по схеме logrotate:

```
gps_output.log      <- текущий файл
gps_output.log.1    <- предыдущий
gps_output.log.2    <- более ранний
...
gps_output.log.N    <- старейший (удаляется при следующей ротации)
```

`max_files = 0` — архивные файлы не удаляются.
`max_size_kb = 0` — ротация отключена (один файл неограниченного размера).

Пример (`config/file_output.json`):

```json
{
  "filters": {
    "lpf": { "enabled": true, "type": "fir", "cutoff": 0.15 }
  },
  "output": {
    "type": "file",
    "path": "gps_output.log",
    "rotation": { "max_size_kb": 512, "max_files": 3 }
  }
}
```

```bash
gps_pipeline track.nmea --config config/file_output.json
```

---

### Готовые конфиг-файлы

| Файл | Описание |
|---|---|
| `config/default.json` | Все фильтры включены, без сглаживания, вывод в консоль |
| `config/file_output.json` | КИХ-сглаживание (fc=0.15), вывод в файл 512 КБ × 3 шт. |

---

### Пороги фильтров (CLI — значения по умолчанию)

| Фильтр | Параметр | Значение |
|---|---|---|
| `SatelliteFilter` | `min_satellites` | 4 |
| `SpeedFilter` | `max_speed_kmh` | 200.0 km/h |
| `CoordinateJumpFilter` | `max_distance_m` | 500.0 m |
| `StopFilter` | `threshold_kmh` | 3.0 km/h |

### Примеры

```bash
# Готовый конфиг (консоль)
gps_pipeline track.nmea --config config/default.json

# Файловый вывод с КИХ-сглаживанием
gps_pipeline track.nmea --config config/file_output.json

# CLI: без сглаживания
gps_pipeline track.nmea -cf 1

# CLI: ПИФ, умеренное сглаживание (window = 4)
gps_pipeline track.nmea -lpf pif -cf 0.2

# CLI: КИХ, сильное сглаживание (21 отвод)
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
3. Добавить в `config/Config.h` поле конфига и логику в `buildFilters()` в `main.cpp`

---

## Формат NMEA

Обрабатываются предложения `$GPRMC`, `$GPGGA` и `$GPGSV`. Точка формируется
из совпадающей по времени пары RMC+GGA; GSV-данные прикрепляются к следующей
точке в поле `satellites_in_view`. Строки с `#` и пустые строки игнорируются.

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
| `$GPGSV` PRN/elev/az/SNR | `GpsPoint::satellites_in_view` (`SatelliteInfo`) |
---

---

## Покрытие тестами

Оценка основана на 137 unit-тестах (GTest), покрывающих все методы библиотеки.

`main.cpp` (сомпозиционный root) намеренно не покрыт unit-тестами;
его поведение проверяется E2E-тестом `test_pipeline.cpp::FullSampleNmeaSequence`.

### Оценка по модулям

| Модуль | LOC | Тесты | Оценка |
|---|---|---|---|
| ChecksumValidator | 15 | 13 | 100% |
| NmeaParser (RMC+GGA+GSV) | 100 | 24 | ~95% |
| SatelliteFilter | 12 | 3 | 100% |
| SpeedFilter | 12 | 3 | 100% |
| CoordinateJumpFilter | 45 | 6 | ~100% |
| StopFilter | 12 | 3 | 100% |
| FilterChain | 20 | 5 | 100% |
| MovingAverageFilter | 35 | 6 | ~95% |
| FirLowPassFilter | 55 | 8 | ~90% |
| ConsoleOutput | 50 | 15 | ~100% |
| FileOutput | 70 | 10 | ~90% |
| ConfigLoader | 80 | 14 | ~95% |
| Pipeline | 55 | 12 | ~95% |
| **Итого (библиотека)** | **~561** | **137** | **>93%** |

### coverage-отчёт (Linux / GCC + lcov)

bash
cmake --preset=linux-coverage
cmake --build --preset=linux-coverage --target coverage
xdg-open out/build/linux-coverage/coverage_html/index.html

Пресет linux-coverage добавляет флаги --coverage -O0, запускает lcov и genhtml,
отчёт появляется в out/build/linux-coverage/coverage_html/.
