# PDM Example — nRF54L15

A minimal Zephyr application that demonstrates continuous PDM audio capture using the Zephyr DMIC API on the nRF54L15.

No microphone is required — the example uses a loopback wire to feed the PDM clock signal back into the data input, producing deterministic non-zero samples that confirm the full capture pipeline is working.

## What it does

1. **Configures** the `pdm20` peripheral via the Zephyr DMIC API:
   - 16 kHz sample rate, 16-bit mono
   - 20 ms blocks (320 samples = 640 bytes)
   - 4-block memory slab for DMA double-buffering headroom

2. **Starts continuous capture** with `DMIC_TRIGGER_START`.

3. **Reads blocks** and prints min/max/average sample statistics for each block:
   ```
   [PDM] PDM example – nRF54L15 DK
   [PDM] Peripheral : pdm20
   [PDM] CLK pin    : P1.12 (clock pin)
   [PDM] DIN pin    : P1.11  (wire to P1.12 for loopback)
   [PDM] Block size : 640 bytes  (20 ms at 16000 Hz, 1 ch)
   [PDM] PDM device pdm20 ready
   [PDM] PDM started – reading continuously
   [PDM]   [ 0] 320 samples  min=-412   max=411    avg=0
   [PDM]   [ 1] 320 samples  min=-398   max=405    avg=1
   ...
   ```

4. **Loops indefinitely**, freeing each block back to the slab after printing.

## Hardware setup

| Step | Detail |
|---|---|
| Board | nRF54L15 DK |
| Loopback wire | Connect **P1.12** to **P1.11** on the DK P1 expansion header |
| No microphone needed | The PDM clock feeds back into DIN, producing non-zero samples |

> P1.11 and P1.12 are clock-capable pins with no default function on the DK (no conflict with UART, LEDs, or buttons).

## Platform

| Item | Value |
|---|---|
| Board | `nrf54l15dk/nrf54l15/cpuapp` |
| SDK | nRF Connect SDK v3.3.0-rc2 |
| Zephyr | v4.3.99-22eb89901c98 |
| Toolchain | Zephyr SDK 0.17.0 (ARM GCC 12.2.0) |

## Pin assignment

Configured in [boards/nrf54l15dk_nrf54l15_cpuapp.overlay](boards/nrf54l15dk_nrf54l15_cpuapp.overlay).

| Signal | Pin | Direction |
|---|---|---|
| PDM_CLK | P1.12 | Output from SoC |
| PDM_DIN | P1.11 | Input to SoC (wire to P1.12) |

## Building and flashing

```
west build -b nrf54l15dk/nrf54l15/cpuapp --pristine
west flash
```

## Serial output

Connect to VCOM1 (e.g. COM81) at **115200 baud**. Expected output after reset:

```
[PDM] PDM example – nRF54L15 DK
[PDM] Peripheral : pdm20
[PDM] CLK pin    : P1.12 (clock pin)
[PDM] DIN pin    : P1.11  (wire to P1.12 for loopback)
[PDM] Block size : 640 bytes  (20 ms at 16000 Hz, 1 ch)
[PDM] PDM device pdm20 ready
[PDM] PDM started – reading continuously
[PDM]   [ 0] 320 samples  min=-412   max=411    avg=0
[PDM]   [ 1] 320 samples  min=-398   max=405    avg=1
```

Serial settings:
- Baud rate: 115200
- Data bits: 8
- Stop bits: 1
- Parity: None

## nRF54L15 PDM notes

- The nRF54L15 has two PDM peripherals: `pdm20` and `pdm21`. This example uses `pdm20`.
- The PDM clock frequency is derived from the 32 MHz peripheral clock (`clock-source = "PCLK32M"`), which can generate valid PDM clock rates (1–3.25 MHz) without a fractional divider.
- Only mono capture is used here (`PDM_CHAN_LEFT`). Stereo requires two microphones wired to the same PDM bus with opposite clock polarity.
- `dmic_read()` uses a 100 ms timeout — 5× the 20 ms block period — to tolerate startup jitter.

## Project structure

```
pdm_example/
├── CMakeLists.txt
├── prj.conf
├── README.md
├── boards/
│   └── nrf54l15dk_nrf54l15_cpuapp.overlay
└── src/
    └── main.c
```

## How it works

1. **`make_cfg()`**: fills a `dmic_cfg` and `pcm_stream_cfg` with the audio parameters and PDM clock frequency range, then calls `dmic_configure()`.

2. **`print_stats()`**: iterates over a 16-bit PCM block computing min, max, and average sample values and logs them via `LOG_INF`.

3. **Main loop**: configures → triggers start → reads blocks in a `while(true)` loop → prints stats → frees the slab entry. On any read error the loop breaks and `DMIC_TRIGGER_STOP` is called.

## Troubleshooting

| Symptom | Likely cause |
|---|---|
| `PDM device not ready` | Overlay not applied or wrong node label (`pdm20`) |
| `dmic_configure failed: -22` | Invalid clock frequency range in overlay or `prj.conf` |
| `dmic_read failed: -11` (ENOMEM) | Slab exhausted — increase `BLOCK_COUNT` |
| `dmic_read failed: -116` (ETIMEDOUT) | No loopback wire; PDM data line not driven |
| All samples zero | Loopback wire not connected or wrong pins |
| No serial output | Wrong COM port or baud rate; try VCOM1 at 115200 |

## License

SPDX-License-Identifier: Apache-2.0
