# GNSS Binary Dump Format

## File layout

Each `.bin` file contains a sequence of fixed-size records concatenated
back-to-back, with no header and no footer:

```
[Record][Record][Record]...[Record]
```

- **Record size**: 196 bytes
- **Byte order**: little-endian (ARM Cortex-M, x86)
- **Packing**: no padding — all structures use `#pragma pack(1)` / `__attribute__((packed))`
- **Records per file** = `file_size / 196` (exact; files never contain partial records)

The number of records is 1 Hz — each record represents one second of telemetry.

## Directory structure

```
<YYMMDD>/<HH>/<MM>.bin
```

- `YYMMDD` — UTC date (e.g. `260317` = 2026-03-17)
- `HH` — UTC hour (24-hour, zero-padded)
- `MM` — starting UTC minute of the chunk; each file covers roughly 10 minutes

## Record structure

```cpp
#pragma pack(1)
struct Record
{
    uint64_t timeStamp;                     //  +0,  8 bytes — system tick (ms since boot)
    uint64_t datetime;                      //  +8,  8 bytes — Unix epoch time (seconds UTC)
    uint8_t  navValid;                      // +16,  1 byte  — NavStatus enum (see below)
    int32_t  latitude;                      // +17,  4 bytes — degrees × 1,000,000 (signed)
    int32_t  longitude;                     // +21,  4 bytes — degrees × 1,000,000 (signed)
    int16_t  height;                        // +25,  2 bytes — meters above MSL (signed)
    uint16_t speed;                         // +27,  2 bytes — km/h × 10 (fixed-point)
    uint16_t course;                        // +29,  2 bytes — degrees × 10 (0..3599)
    uint8_t  HDOP;                          // +31,  1 byte  — HDOP × 10 (0..255)
    uint8_t  PDOP;                          // +32,  1 byte  — PDOP × 10 (0..255)
    uint8_t  satCount;                      // +33,  1 byte  — total satellites in use
    GnssSatInfo satInfo[4];                 // +34, 16 bytes — per-system sat info (4 × 4 bytes)
    uint32_t sysVoltage;                    // +50,  4 bytes — system voltage, millivolts
    uint32_t ignInVoltage;                  // +54,  4 bytes — ignition input, millivolts
    AccelerometerResult acc;                // +58, 40 bytes — accelerometer data (see below)
    uint8_t  moveState;                     // +98,  1 byte  — MoveState bitmask (see below)
    uint8_t  availableStates;               // +99,  1 byte  — MoveState sources available (bitmask)
    SatSnrRecord overallArraySnr[96];       //+100, 96 bytes — per-PRN SNR records
};                                          // total: 196 bytes
#pragma pack()
```

### Field semantics

| Field             | Unit / encoding                                                          |
|-------------------|--------------------------------------------------------------------------|
| `timeStamp`       | Milliseconds since device boot. Monotonic, wraps at 2⁶⁴.                |
| `datetime`        | Seconds since Unix epoch (1970-01-01 UTC).                              |
| `latitude`        | Divide by 1,000,000 to get degrees. Range [-90, +90].                   |
| `longitude`       | Divide by 1,000,000 to get degrees. Range [-180, +180].                 |
| `height`          | Meters above mean sea level.                                            |
| `speed`           | Divide by 10 to get km/h.                                               |
| `course`          | Divide by 10 to get degrees (0 = North, 90 = East).                     |
| `HDOP` / `PDOP`   | Divide by 10 to get DOP value.                                          |
| `sysVoltage`      | System rail voltage in millivolts (e.g. 12500 = 12.5 V).                |
| `ignInVoltage`    | Ignition line voltage in millivolts.                                    |

### `NavStatus` enum (1 byte)

```cpp
enum class NavStatus : uint8_t
{
    STATUS_VALID = 0,  // GPS-valid fix
    RCVR_ERR     = 1,  // Receiver error
    LBS          = 2,  // Cell-tower positioning (LBS)
    EL           = 3,  // External location source
    FIRST_START  = 0xFF // Boot state, last-known position
};
```

### `GnssSatInfo` (4 bytes per system)

```cpp
#pragma pack(1)
struct GnssSatInfo
{
    uint8_t inView;   // Satellites visible
    uint8_t inUse;    // Satellites used in solution
    uint8_t avgSnr;   // Average SNR (dB-Hz)
    uint8_t maxSnr;   // Peak SNR (dB-Hz)
};
#pragma pack()
```

Indexed by navigation system (array of 4 entries):

| Index | System  | Description       |
|-------|---------|-------------------|
| 0     | GPS     | USA GPS L1        |
| 1     | GLNS    | GLONASS L1        |
| 2     | GLLO    | Galileo E1        |
| 3     | BEDU    | BeiDou B1         |

### `AccelerometerResult` (40 bytes)

```cpp
#pragma pack(1)
struct vect3d { float x; float y; float z; };  // 12 bytes

struct AccelerometerResult
{
    vect3d   lastRaw;               // Raw accelerometer reading (g)
    vect3d   lowPassFilterLowFactor;  // Low-cut LPF — noise-reduced signal
    vect3d   lowPassFilterHighFactor; // High-cut LPF — tracks gravity vector
    uint32_t timestamp;              // Sample timestamp (ms since boot)
};
#pragma pack()
```

Units: `g` (standard gravity, 9.80665 m/s²). Float, IEEE-754 32-bit.

### `MoveState` flags (1 byte bitmask)

```cpp
enum class MoveState : uint8_t
{
    NO_MOVING          = 0,  // Stopped
    MOVING_BY_ACC      = 1,  // Movement detected by accelerometer
    MOVING_BY_POWER    = 2,  // Movement detected by external power source
    MOVING_BY_IGNITION = 4   // Ignition is on
};
```

Multiple flags may be set simultaneously (e.g. `5 = ACC | IGNITION`).

`availableStates` uses the same bitmask and indicates which detection
sources are physically available on this unit.

### `SatSnrRecord` (1 byte per satellite)

Bit-packed per-satellite SNR record:

```cpp
#pragma pack(1)
struct SatSnrRecord
{
    uint8_t snr   : 7;  // SNR (dB-Hz), 0..127
    uint8_t isUse : 1;  // 1 = used in position solution, 0 = visible only
};
#pragma pack()
```

The `overallArraySnr[96]` field is logically split into two per-system arrays:

| Array index range | System   | PRN assignment            |
|-------------------|----------|---------------------------|
| `[ 0 .. 63]`      | GPS      | Array index + 1 = GPS PRN (1..64) |
| `[64 .. 95]`      | GLONASS  | (Array index - 64) + 1 = GLONASS slot number (1..32) |

## Parsing example (C)

```c
#include <stdio.h>
#include <stdint.h>

#pragma pack(push, 1)
typedef struct
{
    uint8_t inView, inUse, avgSnr, maxSnr;
} GnssSatInfo;

typedef struct { float x, y, z; } vect3d;

typedef struct
{
    vect3d   lastRaw;
    vect3d   lpfLow;
    vect3d   lpfHigh;
    uint32_t timestamp;
} AccelerometerResult;

typedef struct
{
    uint8_t snr_and_use;  // bit 7: isUse, bits 0..6: snr
} SatSnrRecord;

typedef struct
{
    uint64_t            timeStamp;
    uint64_t            datetime;
    uint8_t             navValid;
    int32_t             latitude;
    int32_t             longitude;
    int16_t             height;
    uint16_t            speed;
    uint16_t            course;
    uint8_t             HDOP;
    uint8_t             PDOP;
    uint8_t             satCount;
    GnssSatInfo         satInfo[4];
    uint32_t            sysVoltage;
    uint32_t            ignInVoltage;
    AccelerometerResult acc;
    uint8_t             moveState;
    uint8_t             availableStates;
    SatSnrRecord        overallArraySnr[96];
} Record;
#pragma pack(pop)

_Static_assert(sizeof(Record) == 196, "Record must be 196 bytes");

int main(int argc, char** argv)
{
    FILE* f = fopen(argv[1], "rb");
    Record r;
    while (fread(&r, sizeof(r), 1, f) == 1)
    {
        double lat = r.latitude  / 1e6;
        double lon = r.longitude / 1e6;
        double kph = r.speed / 10.0;
        printf("%lld %.6f %.6f %.1f km/h sats=%u\n",
               (long long)r.datetime, lat, lon, kph, r.satCount);
    }
    fclose(f);
    return 0;
}
```

## Parsing example (Python)

```python
import struct, sys

RECORD_FMT = "<QQB i i h H H B B B" + "BBBB" * 4 + "I I" + "fff fff fff I" + "BB" + "B" * 96
RECORD_SIZE = struct.calcsize(RECORD_FMT)
assert RECORD_SIZE == 196, f"got {RECORD_SIZE}"

with open(sys.argv[1], "rb") as f:
    while chunk := f.read(RECORD_SIZE):
        if len(chunk) < RECORD_SIZE:
            break
        fields = struct.unpack(RECORD_FMT, chunk)
        timestamp, datetime_, nav, lat, lon, h, spd, crs, hdop, pdop, satCount = fields[:11]
        print(f"t={datetime_} lat={lat/1e6:.6f} lon={lon/1e6:.6f} "
              f"spd={spd/10:.1f}kph sats={satCount}")
```

## Notes on endianness & alignment

- All multi-byte integers are **little-endian**.
- All `float`s are **IEEE-754 32-bit, little-endian**.
- The record is fully packed — there is no padding between fields.
  Compilers must honor `#pragma pack(1)` or the equivalent attribute.
- `uint64_t` fields are 8-byte aligned in the source struct but the file
  layout is still fully packed; the `timeStamp` and `datetime` fields
  happen to start at file offset 0 and 8 which are naturally aligned.
