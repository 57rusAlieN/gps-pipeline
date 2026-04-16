# GNSS Filter Configuration

This document describes the GNSS post-processing filter settings that were
active on the devices that produced the binary dumps in this package.

## Source

The settings were extracted from device config file
`867782070214874.gcm` (device IMEI 867782070214874, URBE_14874 unit).
All four devices in this dataset use the same filter preset.

Relevant GCM keys:

```
GNSS.FPRESETLD 255
GPS.CORRECT    1, 5, 5, 150, 6, 20, 4
GPS.CORRECT2   10, 4, 3
GPS.CORRECT3   -50, 2000, 50
```

- `GNSS.FPRESETLD = 255` — custom preset (no built-in preset applied;
  the values below are exactly what the firmware uses).

## Filter pipeline

Filters are applied to each GNSS point in the following order. A point
rejected by any filter is marked invalid and is not emitted on the
filtered track.

| Order | Filter              | Parameters                       | Purpose                                  |
|-------|---------------------|----------------------------------|------------------------------------------|
| 1     | SatellitesFilter    | wait=10 s, startCount=4, minCount=3 | Ignore points right after cold start until enough satellites are locked |
| 2     | QualityFilter       | maxHdop=5.0                      | Reject points with bad SNR / high HDOP   |
| 3     | SpeedFilter         | maxSpeed=150 km/h                | Reject implausibly fast samples          |
| 4     | HeightFilter        | min=-50 m, max=2000 m, jump=50 m | Reject out-of-range or jumping altitudes |
| 5     | JumpSuppressFilter  | maxAcc=6, maxJump=20, maxWrong=5 | Reject position jumps / impossible acceleration |
| 6     | ParkingFilter       | parkingSpeed=4 km/h              | Freeze position while vehicle is parked  |

## Parameter reference

### `GPS.CORRECT` — main filter block (7 values)

| Index | Name          | Value | Units              | Applies to                      |
|-------|---------------|-------|--------------------|----------------------------------|
| 0     | isEnabled     | 1     | bool               | Master enable for the pipeline   |
| 1     | maxWrong      | 5     | points             | JumpSuppressFilter — after rejecting this many in a row, accept the next unconditionally |
| 2     | maxHdop       | 5     | HDOP units         | QualityFilter — reject if `HDOP >= 5.0` |
| 3     | maxSpeed      | 150   | km/h               | SpeedFilter — reject if `speed > 150` |
| 4     | maxAcc        | 6     | m/s²               | JumpSuppressFilter — reject if implied acceleration exceeds this |
| 5     | maxJump       | 20    | m/s (delta-v cap)  | JumpSuppressFilter — reject if single-step velocity change exceeds this |
| 6     | parkingSpeed  | 4     | km/h               | ParkingFilter — treat as parked if `speed < 4` |

### `GPS.CORRECT2` — satellite gate (3 values)

| Index | Name          | Value | Units   | Applies to                            |
|-------|---------------|-------|---------|----------------------------------------|
| 0     | satWaitTime   | 10    | seconds | SatellitesFilter — grace period after boot |
| 1     | satStartCount | 4     | sats    | SatellitesFilter — required count to start emitting points |
| 2     | satMinCount   | 3     | sats    | SatellitesFilter — minimum ongoing satellite count |

### `GPS.CORRECT3` — altitude gate (3 values)

| Index | Name          | Value | Units   | Applies to                       |
|-------|---------------|-------|---------|-----------------------------------|
| 0     | heightMin     | -50   | meters  | HeightFilter — reject if below    |
| 1     | heightMax     | 2000  | meters  | HeightFilter — reject if above    |
| 2     | heightJump    | 50    | meters  | HeightFilter — reject single-step altitude jumps above this |

## QualityFilter internals

QualityFilter has additional hard-coded SNR thresholds (not in the GCM file)
that are worth noting because they govern most of the drift-related rejections:

| Constant                    | Value  | Purpose                                                          |
|-----------------------------|--------|------------------------------------------------------------------|
| `BASE_QUALITY_MIN_SAT_SNR`  | 19 dB-Hz | Minimum SNR to treat a satellite as usable                      |
| `BASE_QUALITY_NORMAL_SAT_SNR` | 24 dB-Hz | SNR threshold for "normal" quality                              |
| `BASE_QUALITY_GOOD_SAT_SNR` | 28 dB-Hz | SNR threshold for "good" quality                                |
| `MIN_SATELLITES_COUNT`      | 3        | Minimum satellites with minimum SNR for LOW quality             |
| `NORMAL_SATELLITES_COUNT`   | 4        | Required count of GOOD-SNR satellites for NORMAL/GOOD quality   |
| `DEFAULT_ADAPTIVE_K`        | 100 %    | Nominal SNR-threshold scaling factor                            |
| `MIN_ADAPTIVE_K`            | 70 %     | Lowest that adaptive scaling can drop the thresholds            |
| `STEP_DOWN_SIZE`            | 3 %      | Adaptive step-down per minute during prolonged poor signal      |
| `STEP_DOWN_INTERVAL`        | 60000 ms | Time without normal SNR (while moving) before step-down         |
| `NORMAL_QUALITY_LIMIT`      | 4 points | After quality loss, wait this many points for NORMAL before expecting GOOD |
| `ERROR_POINTS_LIMIT`        | 60 points | Retry LOW threshold once after this many rejected points       |
| `SKIP_POINTS_AFTER_START`   | 5 points | Ignore this many points after system boot                       |
| `MOVE_DETECT_SPEED`         | 4 km/h   | Speed threshold for "moving" state (adaptive threshold gate)    |

Quality levels emitted by the filter:

```cpp
enum class Quality
{
    BAD,           // Point rejected
    LOW,
    BELOW_NORMAL,
    NORMAL,
    BELOW_GOOD,
    GOOD
};
```

The filter rejects points with `Quality::BAD` and also skips one good
point after a streak of bad ones (hysteresis) to avoid emitting spurious
positions during signal recovery.

## Cross-reference: which data was produced with which preset

All four cases in this package used the filter configuration above:

| Date (UTC) | Dump location            | Case reference           |
|------------|--------------------------|--------------------------|
| 2026-03-06 | `260306/15/`             | ANALYSIS_260306_15       |
| 2026-03-17 | `260317/16/`             | ANALYSIS_17.03_16.36     |
| 2026-03-18 | `260318/15/`             | ANALYSIS_18.03_15.42     |
| 2026-03-20 | `260320/16/02.bin`       | URBE_14874 parking drift |
