# Refactoring Plan: IRecordReader + Directory Scan + Config "input"

## Цель

Устранить дублирование `BinaryPipeline` и `Pipeline` (одинаковый цикл
`filter → output`), добавить единый `IRecordReader`, рекурсивный обход
каталогов и секцию `"input"` в JSON-конфиг.

## Архитектура после рефакторинга

```
IRecordReader
├── hasNext() → bool
└── readNext() → ParsedRecord { optional<GpsPoint>, string error }

     NmeaRecordReader          BinaryRecordReader
     istream + NmeaParser      istream + GnssBinaryParser
     (NMEA-specific errors)    (navValid errors)

     MultiFileRecordReader  — Composite
     ├── sorted list of fs::path
     ├── factory: path → unique_ptr<IRecordReader>
     └── advances file when current reader exhausted

     Pipeline(IRecordReader&, IGpsFilter&, IOutput&)
     └── run()   ← единственный цикл, общий для всех типов данных
```

Удаляется:
- `BinaryPipeline` (логика переезжает в `BinaryRecordReader`)
- `IBinaryParser` (RECORD_SIZE переезжает в `GnssBinaryParser`)
- `INmeaParser` (остаётся как интерфейс для unit-тестов `NmeaParser`)

## JSON Config — новая секция "input"

```json
{
  "input": {
    "type":      "auto",        // "auto" | "nmea" | "binary"
    "path":      "data/260317", // файл или корневой каталог
    "recursive": true           // рекурсивный обход подкаталогов
  },
  "filters": { ... },
  "output":  { ... }
}
```

`"auto"` — определяет тип по расширению (`.bin` → binary, иначе nmea).
Порядок файлов: лексикографически по пути (= хронологически для
структуры `YYMMDD/HH/MM.bin`).

## Шаги и статусы

| # | Шаг | Тип | Статус | Коммит |
|---|-----|-----|--------|--------|
| 1 | `InputCfg` в `Config.h` | code | ✅ | b6a70de |
| 2 | Тесты `InputCfg` в `test_config.cpp` | RED | ✅ | b6a70de |
| 3 | `ConfigLoader`: парсинг секции `"input"` | GREEN | ✅ | f5f0bf8 |
| 4 | `IRecordReader.h` + `ParsedRecord` | code | ✅ | 6200a63 |
| 5 | `NmeaRecordReader.h/.cpp` | code | ✅ | a98190f |
| 6 | `BinaryRecordReader.h/.cpp` | code | ✅ | a98190f |
| 7 | `test_record_readers.cpp` | RED→GREEN | ✅ | a98190f |
| 8 | `MultiFileRecordReader.h/.cpp` (composite + fs scan) | code | ✅ | 4a8df29 |
| 9 | `test_multifile_reader.cpp` | RED→GREEN | ✅ | 4a8df29 |
| 10 | `Pipeline`: переписать под `IRecordReader`, метод `run()` | refactor | ✅ | 164cd88 |
| 11 | `test_pipeline.cpp`: адаптировать под `NmeaRecordReader` | refactor | ✅ | 164cd88 |
| 12 | Удалить `BinaryPipeline`, `IBinaryParser` | cleanup | ✅ | 164cd88 |
| 13 | `main.cpp`: `MultiFileRecordReader` + `InputCfg` | code | ✅ | 164cd88 |
| 14 | Финальный прогон тестов + обновление README | validate | ✅ | 164cd88 |

## Инварианты (не меняются)

- `NmeaParser::parse(const std::string&)` — API без изменений
- `GnssBinaryParser::parseRecord(const uint8_t*, size_t)` — API без изменений
- `test_parser.cpp`, `test_binary_parser.cpp` — тесты без изменений
- `IGpsFilter`, `IOutput`, `FilterChain`, все фильтры — без изменений
- `ConsoleOutput`, `FileOutput` — без изменений
- `ConfigLoader` — расширяется, старые тесты не ломаются

## Файловая структура (итог)

```
include/
  pipeline/
    IRecordReader.h          ← NEW
    NmeaRecordReader.h       ← NEW  (NMEA-логика из Pipeline.cpp)
    BinaryRecordReader.h     ← NEW  (binary-логика из BinaryPipeline.cpp)
    MultiFileRecordReader.h  ← NEW
    Pipeline.h               ← MODIFIED (IRecordReader& вместо INmeaParser&)
    BinaryPipeline.h         ← DELETE
  parser/
    IBinaryParser.h          ← DELETE
    INmeaParser.h            ← KEEP (unit-тесты NmeaParser)
src/
  pipeline/
    NmeaRecordReader.cpp     ← NEW
    BinaryRecordReader.cpp   ← NEW
    MultiFileRecordReader.cpp ← NEW
    Pipeline.cpp             ← MODIFIED
    BinaryPipeline.cpp       ← DELETE
```

## Точка восстановления

Текущее рабочее состояние: коммит `9896e90`, **167/167 тестов**.
При сбое — `git checkout 9896e90` возвращает полностью рабочую версию.
