
# Sensor Data Compression

A lightweight embedded C codec for compressing multi-rate sensor data into **exactly 4 bytes per 100 ms record** for SD-card logging.

The project is designed for resource-constrained embedded systems such as STM32-based payloads, while keeping the compression algorithm platform-independent and testable on a PC.

---

## Table of Contents

- [Project Overview](#project-overview)
- [Problem](#problem)
- [Solution](#solution)
- [Repository Structure](#repository-structure)
- [Requirements](#requirements)
- [Getting Started](#getting-started)
- [How to Compile](#how-to-compile)
- [How to Run](#how-to-run)
- [How the Encoder Works](#how-the-encoder-works)
- [How the Decoder Works](#how-the-decoder-works)
- [Packet Format](#packet-format)
- [Timestamp Handling](#timestamp-handling)
- [Sensor Encoding](#sensor-encoding)
- [Using the Codec in Your Own Program](#using-the-codec-in-your-own-program)
- [STM32 Integration](#stm32-integration)
- [Testing](#testing)
- [Design Trade-offs](#design-trade-offs)
- [Limitations](#limitations)

---

# Project Overview

This project implements a C-based compression codec for a sensor payload with multiple sampling rates.

The system must read four sensor streams and store the information on an SD card. However, each SD-card record is limited to exactly **4 bytes** and is written every **100 ms**.

The codec therefore uses:

- Bit-level packing
- Sampling-rate-aware encoding
- Temporal redundancy
- Floating-point quantisation
- Implicit timestamp reconstruction
- Fixed-size 4-byte packets

The implementation is written in standard C and does not depend on STM32 HAL or any external library.

---

# Problem

The payload contains four sensors:

| Sensor | Sampling Rate | Raw Data |
|---|---:|---:|
| NO₂ | 500 ms | `0` or `23` |
| Barometer | 250 ms | Unsigned 8-bit |
| Temperature | 1000 ms | Signed 8-bit |
| Acceleration | 100 ms | 32-bit `float` |

The SD card accepts one record every **100 ms**.

Therefore:

```text
10 records/second
````

The original sensor information contains:

```text
Timestamp       16 bits
NO₂              1 bit
Barometer        8 bits
Temperature      8 bits
Acceleration    32 bits
--------------------------------
Total            65 bits
```

However, only:

```text
4 bytes = 32 bits
```

are available per record.

Therefore, the data must be compressed from **65 bits to 32 bits**.

This requires a compression ratio of approximately:

```text
65 / 32 = 2.03 : 1
```

---

# Solution

The codec exploits two important properties of the sensor stream.

## 1. The timestamp is predictable

Records are written every 100 ms:

```text
Record 0 → 0 ms
Record 1 → 100 ms
Record 2 → 200 ms
Record 3 → 300 ms
...
```

Therefore, the timestamp does not need to occupy 16 bits in every packet.

The decoder reconstructs the timestamp from the record number.

---

## 2. Slow sensors do not produce new data every 100 ms

The sensor update rates are:

```text
NO₂          2 samples/second
Barometer    4 samples/second
Temperature  1 sample/second
Acceleration 10 samples/second
```

The slow sensors therefore generate:

```text
2 + 4 + 1 = 7 slow-sensor events/second
```

while the SD card provides:

```text
10 records/second
```

This gives enough capacity to transmit every slow-sensor sample while acceleration is included in every record.

---

# Packet Format

Every record is exactly:

```text
32 bits = 4 bytes
```

The packet is divided into:

```text
┌─────────────────────────────── 32 bits ───────────────────────────────┐
│       Acceleration (22 bits)       │       Sensor Event (10 bits)      │
├────────────────────────────────────┼──────────────────────────────────┤
│        Compressed FP32             │ Event Type (2) │ Value (8)       │
└────────────────────────────────────┴──────────────────────────────────┘
```

## Bit Allocation

| Field        |   Bits | Purpose                    |
| ------------ | -----: | -------------------------- |
| Acceleration |     22 | Quantised `float32`        |
| Event Type   |      2 | Identifies the slow sensor |
| Event Value  |      8 | Sensor value               |
| **Total**    | **32** | **4 bytes**                |

---

# Acceleration Compression

The original acceleration value is a 32-bit IEEE-754 floating-point number.

The codec reduces it from:

```text
32 bits → 22 bits
```

The stored representation retains:

```text
1 sign bit
8 exponent bits
13 fraction bits
```

The lowest 10 fraction bits are discarded.

Therefore:

```text
Original:      32-bit float
Compressed:    22-bit representation
Reconstructed: 32-bit float
```

This is a **lossy** compression step.

The main advantage is that the full 8-bit exponent is retained, giving the compressed representation a wide dynamic range while using only 22 bits.

---

# Sensor Event Encoding

The remaining 10 bits are used for slow-sensor updates.

```text
Bits 9–8 → Event Type
Bits 7–0 → Sensor Value
```

Event types:

| Binary | Event       | Value           |
| ------ | ----------- | --------------- |
| `00`   | No event    | Unused          |
| `01`   | NO₂         | `0` or `23`     |
| `10`   | Barometer   | 8-bit raw value |
| `11`   | Temperature | 8-bit raw value |

For example:

```text
NO₂ = 23
```

is encoded as:

```text
Event Type = 01
Value      = 23
```

The encoder keeps track of pending sensor updates and sends them one at a time.

---

# Repository Structure

```text
sensor-data-compression/
│
├── README.md
├── codec.h
├── codec.c
└── main.c
```

## `README.md`

Project documentation and usage instructions.

## `codec.h`

Contains:

* Encoder state
* Decoder state
* Event definitions
* Public function declarations

## `codec.c`

Contains:

* Acceleration compression
* Acceleration reconstruction
* Sensor event scheduling
* Bit packing
* Bit unpacking
* Timestamp reconstruction

## `main.c`

A standalone test program that:

* Generates example sensor data
* Calls the encoder
* Displays the 4-byte packets
* Calls the decoder
* Displays reconstructed values

---

# Requirements

## PC Testing

You only need:

* GCC, Clang or another C11-compatible compiler
* Standard C library

No external dependencies are required.

The codec does **not** require:

* STM32 HAL
* Arduino libraries
* FreeRTOS
* External compression libraries

---

# Getting Started

## 1. Clone the Repository

```bash
git clone https://github.com/YOUR_USERNAME/sensor-data-compression.git
```

Then enter the project directory:

```bash
cd sensor-data-compression
```

Replace:

```text
YOUR_USERNAME
```

with your GitHub username.

---

# How to Compile

## GCC

Compile the complete project with:

```bash
gcc -std=c11 -Wall -Wextra -O2 codec.c main.c -o sensor_codec
```

### Explanation

```text
-std=c11
```

Uses the C11 standard.

```text
-Wall -Wextra
```

Enables useful compiler warnings.

```text
-O2
```

Enables compiler optimisation.

```text
codec.c main.c
```

Compiles both source files.

```text
-o sensor_codec
```

Creates the executable named `sensor_codec`.

---

# How to Run

## Linux / macOS

```bash
./sensor_codec
```

## Windows

```bash
sensor_codec.exe
```

The program will generate simulated sensor data, compress it into 4-byte records, decode the records and print the results.

Example output:

```text
t=   0 ms | packet=41 1C 01 80 | accel=9.80975 | NO2=0 | Baro=128 | Temp=50
t= 100 ms | packet=41 32 02 80 | accel=9.90918 | NO2=0 | Baro=128 | Temp=50
t= 200 ms | packet=41 48 03 32 | accel=10.00916 | NO2=0 | Baro=128 | Temp=50
```

The exact packet values depend on the simulated sensor input.

---

# How the Encoder Works

The encoder is called once every 100 ms.

```c
uint8_t packet[4];

encode(
    &encoder,
    timestamp_msec,
    no2_data,
    baro_data,
    temp_data,
    acceleration,
    packet
);
```

The encoder performs the following steps:

```text
Sensor data
     │
     ▼
Check which sensors have new samples
     │
     ▼
Store pending slow-sensor event
     │
     ▼
Compress acceleration to 22 bits
     │
     ▼
Select one pending sensor event
     │
     ▼
Pack 22-bit acceleration + 10-bit event
     │
     ▼
4-byte packet
```

The resulting packet can then be written directly to an SD-card buffer.

---

# How the Decoder Works

The decoder receives exactly 4 bytes:

```c
decode(
    &decoder,
    packet,
    &timestamp,
    &no2,
    &baro,
    &temp,
    &acceleration
);
```

The decoder:

1. Reconstructs the 32-bit packet
2. Extracts the 22-bit acceleration
3. Reconstructs the approximate `float`
4. Extracts the sensor event
5. Updates the corresponding sensor state
6. Reconstructs the timestamp from the frame number

The process is:

```text
4-byte packet
     │
     ▼
Unpack 32 bits
     │
     ├───────────────┐
     ▼               ▼
22-bit acceleration  10-bit event
     │               │
     ▼               ▼
Reconstruct float    Identify sensor
                     │
                     ▼
              Update sensor state
                     │
                     ▼
              Reconstruct timestamp
```

---

# Using the Encoder in Another Program

Include the header:

```c
#include "codec.h"
```

Create an encoder state:

```c
EncoderState encoder;

encoder_init(&encoder);
```

Create a 4-byte packet buffer:

```c
uint8_t packet[RECORD_SIZE];
```

Then call:

```c
encode(
    &encoder,
    timestamp_msec,
    no2_data,
    baro_data,
    temp_data,
    acceleration,
    packet
);
```

After this call:

```text
packet[0]
packet[1]
packet[2]
packet[3]
```

contain the complete compressed record.

The packet can then be sent to:

* SD card
* Flash memory
* UART
* SPI
* CAN
* Radio telemetry
* Other storage or communication interfaces

---

# Using the Decoder in Another Program

Include:

```c
#include "codec.h"
```

Create the decoder:

```c
DecoderState decoder;

decoder_init(&decoder);
```

Create output variables:

```c
uint16_t timestamp;
uint8_t no2;
uint8_t baro;
int8_t temp;
float acceleration;
```

Then call:

```c
decode(
    &decoder,
    packet,
    &timestamp,
    &no2,
    &baro,
    &temp,
    &acceleration
);
```

The decoded values are returned through the output variables.

---

# Sensor Conversion

The codec stores the raw sensor values.

The physical quantities can be reconstructed using the linear mappings from the payload specification.

## Barometer

The raw barometer value is:

```text
0 → 120 kPa
255 → 75 kPa
```

Therefore:

```text
Pressure = 120 - (45 × raw_value / 255)
```

Example:

```text
raw_value = 128

Pressure ≈ 97.41 kPa
```

---

## Temperature

The raw temperature value is:

```text
0   → -10 °C
255 → 150 °C
```

Therefore:

```text
Temperature = -10 + (160 × raw_value / 255)
```

Example:

```text
raw_value = 128

Temperature ≈ 70.31 °C
```

---

## NO₂

The NO₂ sensor has two possible output states:

```text
0  → No NO₂ detected
23 → NO₂ detected
```

Therefore, the original sensor state can be reconstructed exactly.

---

# Timestamp Reconstruction

A timestamp is not stored explicitly in each 4-byte packet.

Instead, the decoder uses the record number.

Since the logger writes every 100 ms:

```text
timestamp = record_number × 100 ms
```

Example:

| Record | Timestamp |
| -----: | --------: |
|      0 |      0 ms |
|      1 |    100 ms |
|      2 |    200 ms |
|      3 |    300 ms |
|      4 |    400 ms |

This saves 16 bits per record.

## Important Assumption

This requires the decoder to process records sequentially.

Therefore:

* Records must not be reordered.
* Missing records will cause the implicit timestamp to become incorrect.
* The stream should start at timestamp 0.

For a real deployment where packet loss is possible, a periodic synchronisation record or explicit sequence number should be added.

---

# Testing

The included `main.c` provides a basic end-to-end test.

The test performs:

```text
Generate sensor data
       ↓
     encode()
       ↓
   4-byte packet
       ↓
     decode()
       ↓
Compare reconstructed data
```

## Recommended Test Cases

The codec should be tested with:

### Acceleration

* `0.0`
* Positive values
* Negative values
* Very small values
* Large values
* Rapid changes
* Constant acceleration

### Barometer

* `0`
* `255`
* Mid-range values
* Rapid changes
* Constant values

### Temperature

* `0`
* `255`
* Mid-range values
* Negative physical temperatures
* Constant values

### NO₂

* `0`
* `23`

### Timing

Test sensor updates at:

```text
0 ms
100 ms
200 ms
250 ms
300 ms
500 ms
750 ms
1000 ms
```

This is particularly important because the sensor periods are not all multiples of 100 ms.

---

# Design Trade-offs

The main trade-off is between acceleration precision and storage capacity.

The original acceleration uses:

```text
32 bits
```

The compressed representation uses:

```text
22 bits
```

This introduces quantisation error.

However, the codec retains:

```text
22 bits of acceleration
10 bits of sensor-event information
-------------------------------------
32 bits total
```

The slower sensor values remain represented using their original 8-bit values.

This provides a better balance than simply truncating several sensor values independently.

---

# Why C?

The codec is intentionally written in C because it is suitable for embedded systems.

Advantages include:

* Fixed-width integer types
* Deterministic memory usage
* No dynamic allocation
* No STL or runtime framework
* Direct bit manipulation
* Easy integration with STM32 firmware
* Low overhead

The same source can be tested on a PC and later integrated into an STM32 project.

---

# STM32 Integration

The codec itself does not require an STM32.

It can first be tested on a PC.

For a real embedded payload, the architecture would be:

```text
┌──────────────┐
│    Sensors   │
└──────┬───────┘
       │
       ▼
┌──────────────┐
│     STM32    │
│ Sensor Read  │
└──────┬───────┘
       │
       ▼
┌──────────────┐
│    encode()  │
└──────┬───────┘
       │
       ▼
┌──────────────┐
│  4-byte data │
└──────┬───────┘
       │
       ▼
┌──────────────┐
│    SD Card   │
└──────────────┘
```

The STM32 application would be responsible for:

* Reading the sensors
* Maintaining the 100 ms logging schedule
* Calling `encode()`
* Writing the resulting 4-byte packet to the SD card

The compression codec remains independent of the STM32 hardware drivers.

---

# STM32 Example

In an STM32 project, the codec can be added directly:

```c
#include "codec.h"

EncoderState encoder;

encoder_init(&encoder);

uint8_t packet[RECORD_SIZE];

encode(
    &encoder,
    timestamp_ms,
    no2_value,
    baro_value,
    temperature_value,
    acceleration_value,
    packet
);

/* Write packet to SD card */
HAL_SD_WriteBlocks(
    &hsd,
    packet,
    block_address,
    1,
    HAL_MAX_DELAY
);
```

The exact SD-card implementation depends on the STM32 project configuration and filesystem/driver being used.

---

# Memory and Runtime Considerations

The codec uses fixed-size state structures and does not dynamically allocate memory.

No:

```text
malloc()
free()
new
delete
```

are required.

The packet size is always:

```text
4 bytes
```

The encoder and decoder also operate on a fixed amount of state, making them suitable for microcontroller environments with limited RAM.

---

# Limitations

The current implementation assumes:

1. Records are generated every 100 ms.
2. Sensor sampling follows the specified fixed intervals.
3. Records are processed sequentially.
4. No records are lost or reordered.
5. Acceleration can tolerate the quantisation introduced by the 22-bit representation.
6. The acceleration value fits within the representable IEEE-754 exponent range.

The acceleration representation is **lossy**.

The NO₂, barometer and temperature sensor values are stored using their original 8-bit representations.

---


# Project Goal

This project demonstrates how **bit-level data representation, temporal redundancy and sensor sampling rates** can be used to fit multiple sensor streams into a strict **4-byte storage constraint**.

The implementation focuses on embedded-system requirements:

* Fixed packet size
* Fixed memory usage
* Deterministic execution
* No dynamic memory allocation
* No external dependencies
* Portable C
* Suitable for STM32 integration
* Explicit compression/reconstruction trade-offs

---

```
```
