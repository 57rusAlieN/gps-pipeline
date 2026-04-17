# gps-pipeline

Консольное C++ приложение для постобработки GPS-треков из двоичных дампов
GalileoSky и NMEA-файлов. Читает один файл **или целый каталог** (рекурсивно),
парсит записи, прогоняет точки через настраиваемую цепочку фильтров и выводит
результат в консоль или файл с ротацией.

Разработан по методологии **TDD** (Red → Green → Refactor): каждая функциональность
появлялась сначала как провальный тест, затем как минимальная реализация.
Покрыто **235 автотестов** (GoogleTest).

---

## Сборка

### Требования

| Инструмент | Версия |
|---|---|
| CMake | ≥ 3.14 |
| Компилятор | MSVC 19+ (x64) / GCC 11+ / Clang 14+ |
| Ninja | любая |
| Интернет | для скачивания GoogleTest при первой сборке |

### Команды

```bash
# 1. Сконфигурировать (Debug)
cmake --preset=x64-debug

# 2. Собрать
cmake --build --preset=x64-debug
```

> **Windows / MSVC:** запускать команды из _Developer Command Prompt_ (x64), либо
> предварительно выполнить `vcvars64.bat`.

Результат:
- `out/build/x64-debug/gps_pipeline.exe` — основное приложение
- `out/build/x64-debug/tests/gps_pipeline_tests.exe` — набор тестов

---

## Запуск

```
gps_pipeline [<path>] [--config <cfg.json>] [-lpf <pif|fir>] [-cf <0.01..1>]
```

| Параметр | Описание |
|---|---|
| `<path>` | Файл (`.nmea`, `.bin`) **или каталог** для рекурсивного сканирования |
| `--config <path>` | JSON-конфигурация (все параметры, включая пути) |
| `-lpf pif\|fir` | Сглаживание: moving average / КИХ windowed-sinc |
| `-cf 0.01..1` | Частота среза (нормир. к Найквисту); `1` = без сглаживания |

`<path>` и `--config` можно совмещать: CLI-путь переопределяет `input.path`
из конфига. Если передан только `--config` — путь берётся из `input.path`.

### Примеры

```bash
# NMEA-файл через CLI
gps_pipeline data/sample.nmea

# Двоичный дамп GalileoSky (автоопределение по расширению .bin)
gps_pipeline data/260320/16/02.bin

# Весь каталог рекурсивно (config содержит input.recursive = true)
gps_pipeline --config config/260317_16.json

# Каталог + конфиг: CLI-путь переопределяет config.input.path
gps_pipeline data/260306/15 --config config/260306_15.json

# КИХ-сглаживание через CLI
gps_pipeline track.nmea -lpf fir -cf 0.1
```

### Автотесты

```bash
# Через CTest
ctest --preset=x64-debug --output-on-failure

# Или напрямую
out/build/x64-debug/tests/gps_pipeline_tests.exe
```

Ожидаемый результат: **235/235 passed**.

---

## Архитектура

```
<path> (file / directory)
         │
         ▼
 MultiFileRecordReader          ConfigLoader ◄── --config / CLI
   ├─ NmeaRecordReader          (config/*.json)
   └─ BinaryRecordReader
         │  ParsedRecord { optional<GpsPoint>, error }
         ▼
      Pipeline::run()
         │
         ├── error?  ──► IOutput::writeError()
         │
         └── point  ──► IGpsFilter (FilterChain)
                              ├─ SatelliteFilter (wait + start + min)
                              ├─ QualityFilter   (HDOP + SNR)
                              ├─ SpeedFilter
                              ├─ HeightFilter    (range + jump)
                              ├─ CoordinateJumpFilter  (Haversine)
                              ├─ JumpSuppressFilter    (acc + delta-v)
                              ├─ StopFilter / ParkingFilter
                              └─ LPF [optional]: PIF / FIR
                                    │
                                    ▼
                                 IOutput
                                   ├─ ConsoleOutput  (stdout)
                                   └─ FileOutput     (файл + ротация)
```

### Форматы входных данных

| Расширение | Тип | Парсер |
|---|---|---|
| `.nmea`, `.txt`, прочее | NMEA 0183 text | `NmeaRecordReader` + `NmeaParser` |
| `.bin` | GalileoSky Binary Dump 196 байт | `BinaryRecordReader` + `GnssBinaryParser` |

При `"type": "auto"` (по умолчанию) тип определяется по расширению файла.
Явное указание: `"type": "nmea"` или `"type": "binary"`.

### Обработка каталогов (`MultiFileRecordReader`)

```bash
# В конфиге:
"input": { "type": "binary", "path": "data/260317/16", "recursive": true }
```

- Рекурсивно обходит каталог (`recursive: true`) или только корень (`false`).
- Файлы сортируются **лексикографически** — для структуры `YYMMDD/HH/MM.bin`
  это одновременно хронологический порядок.
- Файл-или-каталог — логика одна и та же: `fromDirectory()` принимает оба случая.

---

## Цепочка фильтров

Фильтры применяются в порядке объявления в `FilterChain`. Точка, отвергнутая
любым фильтром, помечается как `Rejected` и не передаётся в Output.

### Все фильтры

| Фильтр | Класс | Параметры конфига | Действие |
|---|---|---|---|
| **SatelliteFilter** | `SatelliteFilter` | `min_satellites`, `start_count`, `wait_seconds` | Отклоняет точки в grace-период после старта, требует `start_count` для первого lock, затем `min_satellites` |
| **QualityFilter** | `QualityFilter` | `max_hdop`, `min_snr` | Отклоняет если HDOP ≥ max или <3 спутников с SNR ≥ min_snr |
| **SpeedFilter** | `SpeedFilter` | `max_speed_kmh` | Отклоняет скорость > порога |
| **HeightFilter** | `HeightFilter` | `min_m`, `max_m`, `max_jump_m` | Отклоняет высоту вне диапазона или прыжок высоты |
| **CoordinateJumpFilter** | `CoordinateJumpFilter` | `max_distance_m` | Отклоняет прыжок координат > порога (формула Haversine) |
| **JumpSuppressFilter** | `JumpSuppressFilter` | `max_acc_ms2`, `max_jump_ms`, `max_wrong` | Отклоняет при превышении ускорения или резком delta-v; после `max_wrong` подряд — принудительный сброс |
| **StopFilter** | `StopFilter` | `threshold_kmh` | Устанавливает флаг `stopped = true`, точку не отклоняет |
| **ParkingFilter** | `ParkingFilter` | `speed_kmh` | Заморáживает координаты пока `speed < speed_kmh`, устанавливает `stopped = true` |
| **LpfFilter** | `MovingAverageFilter` / `FirLowPassFilter` | `type`, `cutoff` | Сглаживание координат; `pif` (moving average) или `fir` (windowed-sinc) |

---

## Конфигурация (JSON)

```bash
gps_pipeline --config config/260317_16.json
```

### Полная схема

```json
{
  "input": {
    "type":      "auto",
    "path":      "data/260317/16",
    "recursive": true
  },
  "filters": {
    "satellite": {
      "enabled":        true,
      "min_satellites": 3,
      "start_count":    4,
      "wait_seconds":   10
    },
    "quality": {
      "enabled":  true,
      "max_hdop": 5.0,
      "min_snr":  19
    },
    "speed": {
      "enabled":       true,
      "max_speed_kmh": 150.0
    },
    "height": {
      "enabled":    true,
      "min_m":      -50,
      "max_m":      2000,
      "max_jump_m": 50
    },
    "jump": {
      "enabled":        false,
      "max_distance_m": 500.0
    },
    "jump_suppress": {
      "enabled":     true,
      "max_acc_ms2": 6.0,
      "max_jump_ms": 20.0,
      "max_wrong":   5
    },
    "stop": {
      "enabled":       false,
      "threshold_kmh": 3.0
    },
    "parking": {
      "enabled":   true,
      "speed_kmh": 4.0
    },
    "lpf": {
      "enabled": false,
      "type":    "pif",
      "cutoff":  0.2
    }
  },
  "output": {
    "type": "file",
    "path": "output/260317_16.log",
    "rotation": {
      "max_size_kb": 1024,
      "max_files":   5
    }
  }
}
```

### Значения по умолчанию

#### `input`

| Ключ | Тип | Умолчание | Описание |
|---|---|---|---|
| `input.type` | string | `"auto"` | `"auto"` / `"nmea"` / `"binary"` |
| `input.path` | string | `""` | Файл или каталог |
| `input.recursive` | bool | `false` | Рекурсивный обход каталога |

#### `filters`

| Ключ | Тип | Умолчание | Описание |
|---|---|---|---|
| `satellite.enabled` | bool | `true` | |
| `satellite.min_satellites` | int | `4` | Текущий минимум спутников |
| `satellite.start_count` | int | `4` | Кол-во спутников для первого lock |
| `satellite.wait_seconds` | int | `0` | Grace-период (сек ≈ точки при 1 Гц) |
| `quality.enabled` | bool | `false` | |
| `quality.max_hdop` | double | `5.0` | Порог HDOP (отклонение ≥ значения) |
| `quality.min_snr` | int | `19` | Мин. SNR спутника (дБ-Гц) |
| `speed.enabled` | bool | `true` | |
| `speed.max_speed_kmh` | double | `200.0` | |
| `height.enabled` | bool | `false` | |
| `height.min_m` | double | `-50.0` | |
| `height.max_m` | double | `2000.0` | |
| `height.max_jump_m` | double | `50.0` | Макс. прыжок высоты |
| `jump.enabled` | bool | `true` | CoordinateJumpFilter |
| `jump.max_distance_m` | double | `500.0` | |
| `jump_suppress.enabled` | bool | `false` | |
| `jump_suppress.max_acc_ms2` | double | `6.0` | Макс. ускорение (м/с²) |
| `jump_suppress.max_jump_ms` | double | `20.0` | Макс. delta-v (м/с) |
| `jump_suppress.max_wrong` | int | `5` | Принудительный сброс после N отказов |
| `stop.enabled` | bool | `true` | Только флаг, не замораживает |
| `stop.threshold_kmh` | double | `3.0` | |
| `parking.enabled` | bool | `false` | |
| `parking.speed_kmh` | double | `4.0` | Замораживает координаты |
| `lpf.enabled` | bool | `false` | |
| `lpf.type` | string | `"pif"` | `"pif"` — moving average, `"fir"` — windowed-sinc |
| `lpf.cutoff` | double | `0.2` | Нормир. к Найквисту \[0.01..1\] |

#### `output`

| Ключ | Тип | Умолчание | Описание |
|---|---|---|---|
| `output.type` | string | `"console"` | `"console"` / `"file"` |
| `output.path` | string | `"gps_output.log"` | |
| `output.rotation.max_size_kb` | uint | `1024` | 0 = без ротации |
| `output.rotation.max_files` | int | `5` | 0 = хранить все |

---

### Вывод в файл с ротацией

```
gps_output.log      ← текущий
gps_output.log.1    ← предыдущий
...
gps_output.log.N    ← удаляется при следующей ротации
```

---

### Готовые конфиги

| Файл | Описание |
|---|---|
| `config/default.json` | Все legacy-фильтры включены, без сглаживания, консоль |
| `config/file_output.json` | КИХ fc=0.15, файл 512 КБ × 3 |
| `config/260306_15.json` | Пресет FILTER_CONFIG.md → `data/260306/15/` |
| `config/260317_16.json` | Пресет FILTER_CONFIG.md → `data/260317/16/` |
| `config/260318_15.json` | Пресет FILTER_CONFIG.md → `data/260318/15/` |
| `config/260320_16.json` | Пресет FILTER_CONFIG.md → `data/260320/16/` |

---

## Формат GalileoSky Binary Dump

Двоичные файлы содержат 196-байтовые записи (little-endian, 1 Гц).
Формат подробно описан в `docs/BINARY_FORMAT.md`.

Ключевые поля:

| Смещение | Тип | Поле |
|---|---|---|
| 0 | uint64 | Timestamp (ms since boot) |
| 8 | uint64 | Datetime (Unix epoch) |
| 16 | uint8 | navValid (0 = fix) |
| 17 | int32 | Latitude × 10⁻⁶ ° |
| 21 | int32 | Longitude × 10⁻⁶ ° |
| 25 | int16 | Height (m) |
| 27 | uint16 | Speed × 0.1 km/h |
| 31 | uint8 | HDOP × 0.1 |
| 33 | uint8 | Satellite count |
| 100 | uint8[96] | SNR array (GPS+GLONASS) |

---

## Обработанные датасеты

Пресет взят из `docs/FILTER_CONFIG.md` (устройство URBE_14874):
`SatWait=10s, StartSat=4, MinSat=3, HDOP<5, Speed<150, Height -50..2000/jump50,
JumpSuppress(acc=6,dv=20,N=5), Parking<4 km/h`.

| Дата | Каталог | Конфиг | Выход | Размер |
|---|---|---|---|---|
| 2026-03-06 | `data/260306/15/` (7 файлов) | `config/260306_15.json` | `output/260306_15.log` | ~2.5 MB |
| 2026-03-17 | `data/260317/16/` (6 файлов) | `config/260317_16.json` | `output/260317_16.log` | ~1.6 MB |
| 2026-03-18 | `data/260318/15/` (6 файлов) | `config/260318_15.json` | `output/260318_15.log` | ~3.0 MB |
| 2026-03-20 | `data/260320/16/` (1 файл) | `config/260320_16.json` | `output/260320_16.log` | ~0.5 MB |

---

## Архитектурные слои

| Пакет | Заголовки | Описание |
|---|---|---|
| **types** | `GpsPoint.h` | Агрегат данных точки + `SatelliteInfo` |
| **parser** | `INmeaParser.h`, `NmeaParser.h`, `GnssBinaryParser.h`, `ChecksumValidator.h` | NMEA (RMC/GGA/GSV) + Binary 196-byte |
| **pipeline** | `IRecordReader.h`, `NmeaRecordReader.h`, `BinaryRecordReader.h`, `MultiFileRecordReader.h`, `Pipeline.h` | Источники данных + оркестратор |
| **filter** | `IGpsFilter.h`, `FilterChain.h`, `*Filter.h` | Chain of Responsibility |
| **config** | `Config.h`, `ConfigLoader.h` | JSON-конфигурация (nlohmann/json) |
| **output** | `IOutput.h`, `ConsoleOutput.h`, `FileOutput.h` | Форматированный вывод, ротация |

---

## Покрытие тестами

| Файл | Тестов | Что покрывает |
|---|---|---|
| `test_types.cpp` | 9 | GpsPoint, SatelliteInfo, FilterResult |
| `test_checksum.cpp` | 13 | XOR, validate, граничные случаи |
| `test_parser.cpp` | 24 | RMC+GGA, GPGSV, coord/speed, ошибки |
| `test_binary_parser.cpp` | 21 | GnssBinaryParser, navValid, SNR, спутники |
| `test_filters.cpp` | 42 | Все фильтры вкл. SatelliteFilter wait/start |
| `test_new_filters.cpp` | 23 | QualityFilter, HeightFilter, JumpSuppressFilter, ParkingFilter |
| `test_output.cpp` | 15 | Формат строк, stopped, S/W координаты |
| `test_pipeline.cpp` | 11 | Unit + E2E через NmeaRecordReader |
| `test_config.cpp` | 33 | JSON defaults + все новые секции |
| `test_fileoutput.cpp` | 10 | Запись в файл, ротация |
| `test_record_readers.cpp` | 17 | NmeaRecordReader, BinaryRecordReader |
| `test_multifile_reader.cpp` | 13 | MultiFileRecordReader, recursive scan, sort |
| **Итого** | **235** | |

---

## Добавление нового фильтра

1. Создать класс, наследующий `IGpsFilter`, реализовать `FilterResult process(GpsPoint&)`
2. Добавить `XxxFilterCfg` в `include/config/Config.h` и `FiltersCfg`
3. Добавить `parseXxx()` в `src/config/ConfigLoader.cpp` и вызов в `parseFilters()`
4. Подключить в `buildFilters()` в `src/main.cpp`
5. Написать тесты в `tests/test_new_filters.cpp`
