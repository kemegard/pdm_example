/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 * SPDX-License-Identifier: Apache-2.0
 *
 * PDM Example - nRF54L15 DK
 *
 * Demonstrates PDM audio capture using the Zephyr DMIC API with pdm20.
 * Pins: CLK=P1.12 (clock pin), DIN=P1.11.
 *
 * Hardware setup (loopback - no microphone required):
 *   Wire P1.12 to P1.11 on the DK P1 expansion header.
 *
 * Continuously reads 20 ms PCM blocks and prints min/max/average statistics.
 */

#include <zephyr/kernel.h>
#include <zephyr/audio/dmic.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(pdm_example, LOG_LEVEL_INF);

/* --------------------------------------------------------------------------
 * Audio parameters
 * -------------------------------------------------------------------------- */
#define SAMPLE_RATE      16000     /* Hz  */
#define SAMPLE_BIT_WIDTH 16        /* bits */
#define NUM_CHANNELS     1

/*
 * 20 ms block: 16-bit mono at 16 kHz = 320 samples = 640 bytes.
 * With a 20 ms block the CPU only needs to service the buffer ~50 times/s.
 */
#define BLOCK_MS   20
#define BLOCK_SIZE (sizeof(int16_t) * (SAMPLE_RATE / 1000) * BLOCK_MS * NUM_CHANNELS)

/*
 * Slab for normal operation: 4 blocks gives comfortable double-buffering
 * headroom (driver uses 2 for DMA ping-pong, app can hold 2 more).
 */
#define BLOCK_COUNT 4

K_MEM_SLAB_DEFINE_STATIC(normal_slab, BLOCK_SIZE, BLOCK_COUNT, 4);

static const struct device *const dmic_dev = DEVICE_DT_GET(DT_NODELABEL(pdm20));

/* --------------------------------------------------------------------------
 * Helpers
 * -------------------------------------------------------------------------- */

/** Fill a dmic_cfg pointing at the given slab.
 *  Both cfg and stream must be zero-initialised by the caller. */
static void make_cfg(struct dmic_cfg *cfg,
		     struct pcm_stream_cfg *stream,
		     struct k_mem_slab *slab)
{
	stream->pcm_rate   = SAMPLE_RATE;
	stream->pcm_width  = SAMPLE_BIT_WIDTH;
	stream->block_size = BLOCK_SIZE;
	stream->mem_slab   = slab;

	cfg->io.min_pdm_clk_freq = 1000000;
	cfg->io.max_pdm_clk_freq = 3250000;
	cfg->io.min_pdm_clk_dc   = 40;
	cfg->io.max_pdm_clk_dc   = 60;

	cfg->streams = stream;

	cfg->channel.req_num_streams = 1;
	cfg->channel.req_num_chan    = NUM_CHANNELS;
	cfg->channel.req_chan_map_lo =
		dmic_build_channel_map(0, 0, PDM_CHAN_LEFT);
	/* req_chan_map_hi must be 0 for a single-PDM mono/stereo stream */
	cfg->channel.req_chan_map_hi = 0;
}

/** Print min / max / average of a 16-bit PCM block. */
static void print_stats(const void *buf, uint32_t size, int idx)
{
	const int16_t *s  = (const int16_t *)buf;
	uint32_t       n  = size / sizeof(int16_t);
	int16_t        lo = INT16_MAX, hi = INT16_MIN;
	int64_t        sum = 0;

	for (uint32_t i = 0; i < n; i++) {
		if (s[i] < lo) {
			lo = s[i];
		}
		if (s[i] > hi) {
			hi = s[i];
		}
		sum += s[i];
	}

	LOG_INF("  [%2d] %u samples  min=%-6d max=%-6d avg=%lld",
		idx, n, lo, hi, (long long)(sum / (int64_t)n));
}

/* --------------------------------------------------------------------------
 * Normal continuous streaming
 * -------------------------------------------------------------------------- */
int main(void)
{
	int      ret;
	void    *buf;
	uint32_t size;
	int      block_idx = 0;

	LOG_INF("PDM example - nRF54L15 DK");
	LOG_INF("Peripheral : pdm20");
	LOG_INF("CLK pin    : P1.12 (clock pin)");
	LOG_INF("DIN pin    : P1.11  (wire to P1.12 for loopback)");
	LOG_INF("Block size : %u bytes  (%d ms at %d Hz, %d ch)",
		BLOCK_SIZE, BLOCK_MS, SAMPLE_RATE, NUM_CHANNELS);

	if (!device_is_ready(dmic_dev)) {
		LOG_ERR("PDM device %s not ready", dmic_dev->name);
		return -1;
	}
	LOG_INF("PDM device %s ready", dmic_dev->name);

	struct pcm_stream_cfg stream = {0};
	struct dmic_cfg cfg = {0};

	make_cfg(&cfg, &stream, &normal_slab);

	ret = dmic_configure(dmic_dev, &cfg);
	if (ret < 0) {
		LOG_ERR("dmic_configure failed: %d", ret);
		return ret;
	}

	ret = dmic_trigger(dmic_dev, DMIC_TRIGGER_START);
	if (ret < 0) {
		LOG_ERR("DMIC_TRIGGER_START failed: %d", ret);
		return ret;
	}

	LOG_INF("PDM started - reading continuously");

	while (true) {
		/* Generous timeout: 5× block period to tolerate startup jitter */
		ret = dmic_read(dmic_dev, 0, &buf, &size, 100);
		if (ret < 0) {
			LOG_ERR("dmic_read failed at block %d: %d", block_idx, ret);
			break;
		}
		print_stats(buf, size, block_idx++);
		k_mem_slab_free(&normal_slab, buf);
	}

	dmic_trigger(dmic_dev, DMIC_TRIGGER_STOP);
	return 0;
}
