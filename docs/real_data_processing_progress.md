# Real Data Processing — Progress Tracker

## Точка восстановления

Последний рабочий коммит: `8237dd4` (199/199 тестов).
При сбое: `git checkout 8237dd4` возвращает рабочую версию.

## Общий план

| # | Шаг | Тип | Статус | Коммит |
|---|-----|-----|--------|--------|
| 1 | Config.h: новые cfg-секции | code | ✅ | ba0ad92 | — |
| 2 | ConfigLoader + тесты | GREEN | ✅ | ba0ad92 | — |
| 3 | QualityFilter (TDD) | GREEN | ✅ | b635ace | — |
| 4 | HeightFilter (TDD) | GREEN | ✅ | b635ace | — |
| 5 | JumpSuppressFilter (TDD) | GREEN | ✅ | b635ace | — |
| 6 | ParkingFilter (TDD) | GREEN | ✅ | b635ace | — |
| 7 | SatelliteFilter extended | GREEN | ✅ | b451fcd | — |
| 8 | buildFilters() wiring | code | ✅ | 1160d3f | — |
| 9 | 4 JSON-конфига | config | ✅ | 708d9ec | — |
| 10 | Обработка 4 датасетов | run | ✅ | 9b3472d | — |
| 11 | Docs + финальные тесты | validate | ✅ | HEAD | — |
| 12 | — | — | — | — | — |

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
