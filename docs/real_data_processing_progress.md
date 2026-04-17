# Real Data Processing — Progress Tracker

## Точка восстановления

Последний рабочий коммит: `8237dd4` (199/199 тестов).
При сбое: `git checkout 8237dd4` возвращает рабочую версию.

## Общий план

| # | Шаг | Тип | Статус | Коммит |
|---|-----|-----|--------|--------|
| 1 | Config.h: добавить QualityFilterCfg, HeightFilterCfg, JumpSuppressFilterCfg, ParkingFilterCfg; расширить SatelliteFilterCfg | code | ⬜ | — |
| 2 | test_config.cpp: тесты новых cfg-секций | RED | ⬜ | — |
| 3 | ConfigLoader.cpp: парсинг новых секций | GREEN | ⬜ | — |
| 4 | QualityFilter: header + impl + tests (TDD) | RED→GREEN | ⬜ | — |
| 5 | HeightFilter: header + impl + tests (TDD) | RED→GREEN | ⬜ | — |
| 6 | JumpSuppressFilter: header + impl + tests (TDD) | RED→GREEN | ⬜ | — |
| 7 | ParkingFilter: header + impl + tests (TDD) | RED→GREEN | ⬜ | — |
| 8 | SatelliteFilter: расширить wait + startCount (TDD) | RED→GREEN | ⬜ | — |
| 9 | main.cpp buildFilters(): подключить новые фильтры в правильном порядке | code | ⬜ | — |
| 10 | Создать 4 JSON-конфига в config/ | config | ⬜ | — |
| 11 | Создать output/ каталог, обработать 4 датасета | run | ⬜ | — |
| 12 | Обновить docs, финальный прогон тестов | validate | ⬜ | — |

## Параметры фильтров (из FILTER_CONFIG.md)

```
SatellitesFilter:  wait=10s, startCount=4, minCount=3
QualityFilter:     maxHdop=5.0, minSnr=19 dBHz
SpeedFilter:       maxSpeed=150 km/h
HeightFilter:      min=-50m, max=2000m, jump=50m
JumpSuppressFilter: maxAcc=6 m/s², maxJump=20 m/s, maxWrong=5
ParkingFilter:     parkingSpeed=4 km/h
```

## Порядок фильтров в pipeline (из FILTER_CONFIG.md)

1. SatellitesFilter
2. QualityFilter
3. SpeedFilter
4. HeightFilter
5. JumpSuppressFilter
6. ParkingFilter

## 4 датасета

| Дата | Каталог | Конфиг | Выход |
|------|---------|--------|-------|
| 2026-03-06 | data/260306/15/ | config/260306_15.json | output/260306_15.log |
| 2026-03-17 | data/260317/16/ | config/260317_16.json | output/260317_16.log |
| 2026-03-18 | data/260318/15/ | config/260318_15.json | output/260318_15.log |
| 2026-03-20 | data/260320/16/ | config/260320_16.json | output/260320_16.log |
