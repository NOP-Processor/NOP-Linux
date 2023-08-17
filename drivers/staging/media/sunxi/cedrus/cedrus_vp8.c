// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Cedrus VPU driver
 *
 * Copyright (c) 2019 Jernej Skrabec <jernej.skrabec@siol.net>
 */

/*
 * VP8 in Cedrus shares same engine as H264.
 *
 * Note that it seems necessary to call bitstream parsing functions,
 * to parse frame header, otherwise decoded image is garbage. This is
 * contrary to what is driver supposed to do. However, values are not
 * really used, so this might be acceptable. It's possible that bitstream
 * parsing functions set some internal VPU state, which is later necessary
 * for proper decoding. Biggest suspect is "VP8 probs update" trigger.
 */

#include <linux/delay.h>
#include <linux/types.h>

#include <media/videobuf2-dma-contig.h>

#include "cedrus.h"
#include "cedrus_hw.h"
#include "cedrus_regs.h"

#define CEDRUS_ENTROPY_PROBS_SIZE 0x2400
#define VP8_PROB_HALF 128
#define QUANT_DELTA_COUNT 5

/*
 * This table comes from the concatenation of k_coeff_entropy_update_probs,
 * kf_ymode_prob, default_mv_context, etc. It is provided in this form in
 * order to avoid computing it every time the driver is initialised, and is
 * suitable for direct consumption by the hardware.
 */
static const u8 prob_table_init[] = {
	/* k_coeff_entropy_update_probs */
	/* block 0 */
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	0xB0, 0xF6, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xDF, 0xF1, 0xFC, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xF9, 0xFD, 0xFD, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	0xFF, 0xF4, 0xFC, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xEA, 0xFE, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xFD, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	0xFF, 0xF6, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xEF, 0xFD, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xFE, 0xFF, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	0xFF, 0xF8, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xFB, 0xFF, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	0xFF, 0xFD, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xFB, 0xFE, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xFE, 0xFF, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	0xFF, 0xFE, 0xFD, 0xFF, 0xFE, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xFA, 0xFF, 0xFE, 0xFF, 0xFE, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	/* block 1 */
	0xD9, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xE1, 0xFC, 0xF1, 0xFD, 0xFF, 0xFF, 0xFE, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xEA, 0xFA, 0xF1, 0xFA, 0xFD, 0xFF, 0xFD, 0xFE,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	0xFF, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xDF, 0xFE, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xEE, 0xFD, 0xFE, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	0xFF, 0xF8, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xF9, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	0xFF, 0xFD, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xF7, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	0xFF, 0xFD, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xFC, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	0xFF, 0xFE, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xFD, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	0xFF, 0xFE, 0xFD, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xFA, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	/* block 2 */
	0xBA, 0xFB, 0xFA, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xEA, 0xFB, 0xF4, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xFB, 0xFB, 0xF3, 0xFD, 0xFE, 0xFF, 0xFE, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	0xFF, 0xFD, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xEC, 0xFD, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xFB, 0xFD, 0xFD, 0xFE, 0xFE, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	0xFF, 0xFE, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xFE, 0xFE, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	0xFF, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xFE, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	/* block 3 */
	0xF8, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xFA, 0xFE, 0xFC, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xF8, 0xFE, 0xF9, 0xFD, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	0xFF, 0xFD, 0xFD, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xF6, 0xFD, 0xFD, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xFC, 0xFE, 0xFB, 0xFE, 0xFE, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	0xFF, 0xFE, 0xFC, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xF8, 0xFE, 0xFD, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xFD, 0xFF, 0xFE, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	0xFF, 0xFB, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xF5, 0xFB, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xFD, 0xFD, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	0xFF, 0xFB, 0xFD, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xFC, 0xFD, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xFF, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	0xFF, 0xFC, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xF9, 0xFF, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xFF, 0xFF, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	0xFF, 0xFF, 0xFD, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xFA, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	/* kf_y_mode_probs */
	0x91, 0x9C, 0xA3, 0x80, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	/* split_mv_probs */
	0x6E, 0x6F, 0x96, 0x00, 0x00, 0x00, 0x00, 0x00,

	/* bmode_prob */
	0x78, 0x5A, 0x4F, 0x85, 0x57, 0x55, 0x50, 0x6F,
	0x97, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	/* sub_mv_ref_prob */
	0x93, 0x88, 0x12, 0x00,
	0x6A, 0x91, 0x01, 0x00,
	0xB3, 0x79, 0x01, 0x00,
	0xDF, 0x01, 0x22, 0x00,
	0xD0, 0x01, 0x01, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	/* mv_counts_to_probs */
	0x07, 0x01, 0x01, 0x8F,
	0x0E, 0x12, 0x0E, 0x6B,
	0x87, 0x40, 0x39, 0x44,
	0x3C, 0x38, 0x80, 0x41,
	0x9F, 0x86, 0x80, 0x22,
	0xEA, 0xBC, 0x80, 0x1C,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	/* kf_y_mode_tree */
	0x84, 0x02, 0x04, 0x06, 0x80, 0x81, 0x82, 0x83,

	/* y_mode_tree */
	0x80, 0x02, 0x04, 0x06, 0x81, 0x82, 0x83, 0x84,

	/* uv_mode_tree */
	0x80, 0x02, 0x81, 0x04, 0x82, 0x83, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00,

	/* small_mv_tree */
	0x02, 0x08, 0x04, 0x06, 0x80, 0x81, 0x82, 0x83,
	0x0A, 0x0C, 0x84, 0x85, 0x86, 0x87, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	/* small_mv_tree again */
	0x02, 0x08, 0x04, 0x06, 0x80, 0x81, 0x82, 0x83,
	0x0A, 0x0C, 0x84, 0x85, 0x86, 0x87, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	/* split_mv_tree */
	0x83, 0x02, 0x82, 0x04, 0x80, 0x81, 0x00, 0x00,

	/* b_mode_tree */
	0x80, 0x02, 0x81, 0x04, 0x82, 0x06, 0x08, 0x0C,
	0x83, 0x0A, 0x85, 0x86, 0x84, 0x0E, 0x87, 0x10,
	0x88, 0x89, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	/* submv_ref_tree */
	0x8A, 0x02, 0x8B, 0x04, 0x8C, 0x8D, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	/* mv_ref_tree */
	0x87, 0x02, 0x85, 0x04, 0x86, 0x06, 0x88, 0x89,
};

/*
 * This table is a copy of k_mv_entropy_update_probs from the VP8
 * specification.
 *
 * FIXME: If any other driver uses it, we can consider moving
 * this table so it can be shared.
 */
static const u8 k_mv_entropy_update_probs[2][V4L2_VP8_MV_PROB_CNT] = {
	{ 237, 246, 253, 253, 254, 254, 254, 254, 254,
	  254, 254, 254, 254, 254, 250, 250, 252, 254, 254 },
	{ 231, 243, 245, 253, 254, 254, 254, 254, 254,
	  254, 254, 254, 254, 254, 251, 251, 254, 254, 254 }
};

static uint8_t read_bits(struct cedrus_dev *dev, unsigned int bits_count,
			 unsigned int probability)
{
	cedrus_write(dev, VE_H264_TRIGGER_TYPE,
		     VE_H264_TRIGGER_TYPE_VP8_GET_BITS |
		     VE_H264_TRIGGER_TYPE_BIN_LENS(bits_count) |
		     VE_H264_TRIGGER_TYPE_PROBABILITY(probability));

	cedrus_wait_for(dev, VE_H264_STATUS, VE_H264_STATUS_VLD_BUSY);

	return cedrus_read(dev, VE_H264_BASIC_BITS);
}

static void get_delta_q(struct cedrus_dev *dev)
{
	if (read_bits(dev, 1, VP8_PROB_HALF)) {
		read_bits(dev, 4, VP8_PROB_HALF);
		read_bits(dev, 1, VP8_PROB_HALF);
	}
}

static void process_segmentation_info(struct cedrus_dev *dev)
{
	int update, i;

	update = read_bits(dev, 1, VP8_PROB_HALF);

	if (read_bits(dev, 1, VP8_PROB_HALF)) {
		read_bits(dev, 1, VP8_PROB_HALF);

		for (i = 0; i < 4; i++)
			if (read_bits(dev, 1, VP8_PROB_HALF)) {
				read_bits(dev, 7, VP8_PROB_HALF);
				read_bits(dev, 1, VP8_PROB_HALF);
			}

		for (i = 0; i < 4; i++)
			if (read_bits(dev, 1, VP8_PROB_HALF)) {
				read_bits(dev, 6, VP8_PROB_HALF);
				read_bits(dev, 1, VP8_PROB_HALF);
			}
	}

	if (update)
		for (i = 0; i < 3; i++)
			if (read_bits(dev, 1, VP8_PROB_HALF))
				read_bits(dev, 8, VP8_PROB_HALF);
}

static void process_ref_lf_delta_info(struct cedrus_dev *dev)
{
	if (read_bits(dev, 1, VP8_PROB_HALF)) {
		int i;

		for (i = 0; i < 4; i++)
			if (read_bits(dev, 1, VP8_PROB_HALF)) {
				read_bits(dev, 6, VP8_PROB_HALF);
				read_bits(dev, 1, VP8_PROB_HALF);
			}

		for (i = 0; i < 4; i++)
			if (read_bits(dev, 1, VP8_PROB_HALF)) {
				read_bits(dev, 6, VP8_PROB_HALF);
				read_bits(dev, 1, VP8_PROB_HALF);
			}
	}
}

static void process_ref_frame_info(struct cedrus_dev *dev)
{
	u8 refresh_golden_frame = read_bits(dev, 1, VP8_PROB_HALF);
	u8 refresh_alt_ref_frame = read_bits(dev, 1, VP8_PROB_HALF);

	if (!refresh_golden_frame)
		read_bits(dev, 2, VP8_PROB_HALF);

	if (!refresh_alt_ref_frame)
		read_bits(dev, 2, VP8_PROB_HALF);

	read_bits(dev, 1, VP8_PROB_HALF);
	read_bits(dev, 1, VP8_PROB_HALF);
}

static void cedrus_irq_clear(struct cedrus_dev *dev)
{
	cedrus_write(dev, VE_H264_STATUS,
		     VE_H264_STATUS_INT_MASK);
}

static void cedrus_read_header(struct cedrus_dev *dev,
			       const struct v4l2_ctrl_vp8_frame *slice)
{
	int i, j;

	if (V4L2_VP8_FRAME_IS_KEY_FRAME(slice)) {
		read_bits(dev, 1, VP8_PROB_HALF);
		read_bits(dev, 1, VP8_PROB_HALF);
	}

	if (read_bits(dev, 1, VP8_PROB_HALF))
		process_segmentation_info(dev);

	read_bits(dev, 1, VP8_PROB_HALF);
	read_bits(dev, 6, VP8_PROB_HALF);
	read_bits(dev, 3, VP8_PROB_HALF);

	if (read_bits(dev, 1, VP8_PROB_HALF))
		process_ref_lf_delta_info(dev);

	read_bits(dev, 2, VP8_PROB_HALF);

	/* y_ac_qi */
	read_bits(dev, 7, VP8_PROB_HALF);

	/* Parses y_dc_delta, y2_dc_delta, etc. */
	for (i = 0; i < QUANT_DELTA_COUNT; i++)
		get_delta_q(dev);

	if (!V4L2_VP8_FRAME_IS_KEY_FRAME(slice))
		process_ref_frame_info(dev);

	read_bits(dev, 1, VP8_PROB_HALF);

	if (!V4L2_VP8_FRAME_IS_KEY_FRAME(slice))
		read_bits(dev, 1, VP8_PROB_HALF);

	cedrus_write(dev, VE_H264_TRIGGER_TYPE, VE_H264_TRIGGER_TYPE_VP8_UPDATE_COEF);
	cedrus_wait_for(dev, VE_H264_STATUS, VE_H264_STATUS_VP8_UPPROB_BUSY);
	cedrus_irq_clear(dev);

	if (read_bits(dev, 1, VP8_PROB_HALF))
		read_bits(dev, 8, VP8_PROB_HALF);

	if (!V4L2_VP8_FRAME_IS_KEY_FRAME(slice)) {
		read_bits(dev, 8, VP8_PROB_HALF);
		read_bits(dev, 8, VP8_PROB_HALF);
		read_bits(dev, 8, VP8_PROB_HALF);

		if (read_bits(dev, 1, VP8_PROB_HALF)) {
			read_bits(dev, 8, VP8_PROB_HALF);
			read_bits(dev, 8, VP8_PROB_HALF);
			read_bits(dev, 8, VP8_PROB_HALF);
			read_bits(dev, 8, VP8_PROB_HALF);
		}

		if (read_bits(dev, 1, VP8_PROB_HALF)) {
			read_bits(dev, 8, VP8_PROB_HALF);
			read_bits(dev, 8, VP8_PROB_HALF);
			read_bits(dev, 8, VP8_PROB_HALF);
		}

		for (i = 0; i < 2; i++)
			for (j = 0; j < V4L2_VP8_MV_PROB_CNT; j++)
				if (read_bits(dev, 1, k_mv_entropy_update_probs[i][j]))
					read_bits(dev, 7, VP8_PROB_HALF);
	}
}

static void cedrus_vp8_update_probs(const struct v4l2_ctrl_vp8_frame *slice,
				    u8 *prob_table)
{
	int i, j, k;

	memcpy(&prob_table[0x1008], slice->entropy.y_mode_probs,
	       sizeof(slice->entropy.y_mode_probs));
	memcpy(&prob_table[0x1010], slice->entropy.uv_mode_probs,
	       sizeof(slice->entropy.uv_mode_probs));

	memcpy(&prob_table[0x1018], slice->segment.segment_probs,
	       sizeof(slice->segment.segment_probs));

	prob_table[0x101c] = slice->prob_skip_false;
	prob_table[0x101d] = slice->prob_intra;
	prob_table[0x101e] = slice->prob_last;
	prob_table[0x101f] = slice->prob_gf;

	memcpy(&prob_table[0x1020], slice->entropy.mv_probs[0],
	       V4L2_VP8_MV_PROB_CNT);
	memcpy(&prob_table[0x1040], slice->entropy.mv_probs[1],
	       V4L2_VP8_MV_PROB_CNT);

	for (i = 0; i < 4; ++i)
		for (j = 0; j < 8; ++j)
			for (k = 0; k < 3; ++k)
				memcpy(&prob_table[i * 512 + j * 64 + k * 16],
				       slice->entropy.coeff_probs[i][j][k], 11);
}

static enum cedrus_irq_status
cedrus_vp8_irq_status(struct cedrus_ctx *ctx)
{
	struct cedrus_dev *dev = ctx->dev;
	u32 reg = cedrus_read(dev, VE_H264_STATUS);

	if (reg & (VE_H264_STATUS_DECODE_ERR_INT |
		   VE_H264_STATUS_VLD_DATA_REQ_INT))
		return CEDRUS_IRQ_ERROR;

	if (reg & VE_H264_CTRL_SLICE_DECODE_INT)
		return CEDRUS_IRQ_OK;

	return CEDRUS_IRQ_NONE;
}

static void cedrus_vp8_irq_clear(struct cedrus_ctx *ctx)
{
	cedrus_irq_clear(ctx->dev);
}

static void cedrus_vp8_irq_disable(struct cedrus_ctx *ctx)
{
	struct cedrus_dev *dev = ctx->dev;
	u32 reg = cedrus_read(dev, VE_H264_CTRL);

	cedrus_write(dev, VE_H264_CTRL,
		     reg & ~VE_H264_CTRL_INT_MASK);
}

static void cedrus_vp8_setup(struct cedrus_ctx *ctx,
			     struct cedrus_run *run)
{
	const struct v4l2_ctrl_vp8_frame *slice = run->vp8.frame_params;
	struct vb2_queue *cap_q = &ctx->fh.m2m_ctx->cap_q_ctx.q;
	struct vb2_buffer *src_buf = &run->src->vb2_buf;
	struct cedrus_dev *dev = ctx->dev;
	dma_addr_t luma_addr, chroma_addr;
	dma_addr_t src_buf_addr;
	int header_size;
	int qindex;
	u32 reg;

	cedrus_engine_enable(ctx, CEDRUS_CODEC_VP8);

	cedrus_write(dev, VE_H264_CTRL, VE_H264_CTRL_VP8);

	cedrus_vp8_update_probs(slice, ctx->codec.vp8.entropy_probs_buf);

	reg = slice->first_part_size * 8;
	cedrus_write(dev, VE_VP8_FIRST_DATA_PART_LEN, reg);

	header_size = V4L2_VP8_FRAME_IS_KEY_FRAME(slice) ? 10 : 3;

	reg = slice->first_part_size + header_size;
	cedrus_write(dev, VE_VP8_PART_SIZE_OFFSET, reg);

	reg = vb2_plane_size(src_buf, 0) * 8;
	cedrus_write(dev, VE_H264_VLD_LEN, reg);

	/*
	 * FIXME: There is a problem if frame header is skipped (adding
	 * first_part_header_bits to offset). It seems that functions
	 * for parsing bitstreams change internal state of VPU in some
	 * way that can't be otherwise set. Maybe this can be bypassed
	 * by somehow fixing probability table buffer?
	 */
	reg = header_size * 8;
	cedrus_write(dev, VE_H264_VLD_OFFSET, reg);

	src_buf_addr = vb2_dma_contig_plane_dma_addr(src_buf, 0);
	cedrus_write(dev, VE_H264_VLD_END,
		     src_buf_addr + vb2_get_plane_payload(src_buf, 0));
	cedrus_write(dev, VE_H264_VLD_ADDR,
		     VE_H264_VLD_ADDR_VAL(src_buf_addr) |
		     VE_H264_VLD_ADDR_FIRST | VE_H264_VLD_ADDR_VALID |
		     VE_H264_VLD_ADDR_LAST);

	cedrus_write(dev, VE_H264_TRIGGER_TYPE,
		     VE_H264_TRIGGER_TYPE_INIT_SWDEC);

	cedrus_write(dev, VE_VP8_ENTROPY_PROBS_ADDR,
		     ctx->codec.vp8.entropy_probs_buf_dma);

	reg = 0;
	switch (slice->version) {
	case 1:
		reg |= VE_VP8_PPS_FILTER_TYPE_SIMPLE;
		reg |= VE_VP8_PPS_BILINEAR_MC_FILTER;
		break;
	case 2:
		reg |= VE_VP8_PPS_LPF_DISABLE;
		reg |= VE_VP8_PPS_BILINEAR_MC_FILTER;
		break;
	case 3:
		reg |= VE_VP8_PPS_LPF_DISABLE;
		reg |= VE_VP8_PPS_FULL_PIXEL;
		break;
	}
	if (slice->segment.flags & V4L2_VP8_SEGMENT_FLAG_UPDATE_MAP)
		reg |= VE_VP8_PPS_UPDATE_MB_SEGMENTATION_MAP;
	if (!(slice->segment.flags & V4L2_VP8_SEGMENT_FLAG_DELTA_VALUE_MODE))
		reg |= VE_VP8_PPS_MB_SEGMENT_ABS_DELTA;
	if (slice->segment.flags & V4L2_VP8_SEGMENT_FLAG_ENABLED)
		reg |= VE_VP8_PPS_SEGMENTATION_ENABLE;
	if (ctx->codec.vp8.last_filter_type)
		reg |= VE_VP8_PPS_LAST_LOOP_FILTER_SIMPLE;
	reg |= VE_VP8_PPS_SHARPNESS_LEVEL(slice->lf.sharpness_level);
	if (slice->lf.flags & V4L2_VP8_LF_FILTER_TYPE_SIMPLE)
		reg |= VE_VP8_PPS_LOOP_FILTER_SIMPLE;
	reg |= VE_VP8_PPS_LOOP_FILTER_LEVEL(slice->lf.level);
	if (slice->lf.flags & V4L2_VP8_LF_ADJ_ENABLE)
		reg |= VE_VP8_PPS_MODE_REF_LF_DELTA_ENABLE;
	if (slice->lf.flags & V4L2_VP8_LF_DELTA_UPDATE)
		reg |= VE_VP8_PPS_MODE_REF_LF_DELTA_UPDATE;
	reg |= VE_VP8_PPS_TOKEN_PARTITION(ilog2(slice->num_dct_parts));
	if (slice->flags & V4L2_VP8_FRAME_FLAG_MB_NO_SKIP_COEFF)
		reg |= VE_VP8_PPS_MB_NO_COEFF_SKIP;
	reg |= VE_VP8_PPS_RELOAD_ENTROPY_PROBS;
	if (slice->flags & V4L2_VP8_FRAME_FLAG_SIGN_BIAS_GOLDEN)
		reg |= VE_VP8_PPS_GOLDEN_SIGN_BIAS;
	if (slice->flags & V4L2_VP8_FRAME_FLAG_SIGN_BIAS_ALT)
		reg |= VE_VP8_PPS_ALTREF_SIGN_BIAS;
	if (ctx->codec.vp8.last_frame_p_type)
		reg |= VE_VP8_PPS_LAST_PIC_TYPE_P_FRAME;
	reg |= VE_VP8_PPS_LAST_SHARPNESS_LEVEL(ctx->codec.vp8.last_sharpness_level);
	if (!(slice->flags & V4L2_VP8_FRAME_FLAG_KEY_FRAME))
		reg |= VE_VP8_PPS_PIC_TYPE_P_FRAME;
	cedrus_write(dev, VE_VP8_PPS, reg);

	cedrus_read_header(dev, slice);

	/* reset registers changed by HW */
	cedrus_write(dev, VE_H264_CUR_MB_NUM, 0);
	cedrus_write(dev, VE_H264_MB_ADDR, 0);
	cedrus_write(dev, VE_H264_ERROR_CASE, 0);

	reg = 0;
	reg |= VE_VP8_QP_INDEX_DELTA_UVAC(slice->quant.uv_ac_delta);
	reg |= VE_VP8_QP_INDEX_DELTA_UVDC(slice->quant.uv_dc_delta);
	reg |= VE_VP8_QP_INDEX_DELTA_Y2AC(slice->quant.y2_ac_delta);
	reg |= VE_VP8_QP_INDEX_DELTA_Y2DC(slice->quant.y2_dc_delta);
	reg |= VE_VP8_QP_INDEX_DELTA_Y1DC(slice->quant.y_dc_delta);
	reg |= VE_VP8_QP_INDEX_DELTA_BASE_QINDEX(slice->quant.y_ac_qi);
	cedrus_write(dev, VE_VP8_QP_INDEX_DELTA, reg);

	reg = 0;
	reg |= VE_VP8_FSIZE_WIDTH(slice->width);
	reg |= VE_VP8_FSIZE_HEIGHT(slice->height);
	cedrus_write(dev, VE_VP8_FSIZE, reg);

	reg = 0;
	reg |= VE_VP8_PICSIZE_WIDTH(slice->width);
	reg |= VE_VP8_PICSIZE_HEIGHT(slice->height);
	cedrus_write(dev, VE_VP8_PICSIZE, reg);

	reg = 0;
	reg |= VE_VP8_SEGMENT3(slice->segment.quant_update[3]);
	reg |= VE_VP8_SEGMENT2(slice->segment.quant_update[2]);
	reg |= VE_VP8_SEGMENT1(slice->segment.quant_update[1]);
	reg |= VE_VP8_SEGMENT0(slice->segment.quant_update[0]);
	cedrus_write(dev, VE_VP8_SEGMENT_FEAT_MB_LV0, reg);

	reg = 0;
	reg |= VE_VP8_SEGMENT3(slice->segment.lf_update[3]);
	reg |= VE_VP8_SEGMENT2(slice->segment.lf_update[2]);
	reg |= VE_VP8_SEGMENT1(slice->segment.lf_update[1]);
	reg |= VE_VP8_SEGMENT0(slice->segment.lf_update[0]);
	cedrus_write(dev, VE_VP8_SEGMENT_FEAT_MB_LV1, reg);

	reg = 0;
	reg |= VE_VP8_LF_DELTA3(slice->lf.ref_frm_delta[3]);
	reg |= VE_VP8_LF_DELTA2(slice->lf.ref_frm_delta[2]);
	reg |= VE_VP8_LF_DELTA1(slice->lf.ref_frm_delta[1]);
	reg |= VE_VP8_LF_DELTA0(slice->lf.ref_frm_delta[0]);
	cedrus_write(dev, VE_VP8_REF_LF_DELTA, reg);

	reg = 0;
	reg |= VE_VP8_LF_DELTA3(slice->lf.mb_mode_delta[3]);
	reg |= VE_VP8_LF_DELTA2(slice->lf.mb_mode_delta[2]);
	reg |= VE_VP8_LF_DELTA1(slice->lf.mb_mode_delta[1]);
	reg |= VE_VP8_LF_DELTA0(slice->lf.mb_mode_delta[0]);
	cedrus_write(dev, VE_VP8_MODE_LF_DELTA, reg);

	luma_addr = cedrus_dst_buf_addr(ctx, run->dst->vb2_buf.index, 0);
	chroma_addr = cedrus_dst_buf_addr(ctx, run->dst->vb2_buf.index, 1);
	cedrus_write(dev, VE_VP8_REC_LUMA, luma_addr);
	cedrus_write(dev, VE_VP8_REC_CHROMA, chroma_addr);

	qindex = vb2_find_timestamp(cap_q, slice->last_frame_ts, 0);
	if (qindex >= 0) {
		luma_addr = cedrus_dst_buf_addr(ctx, qindex, 0);
		chroma_addr = cedrus_dst_buf_addr(ctx, qindex, 1);
		cedrus_write(dev, VE_VP8_FWD_LUMA, luma_addr);
		cedrus_write(dev, VE_VP8_FWD_CHROMA, chroma_addr);
	} else {
		cedrus_write(dev, VE_VP8_FWD_LUMA, 0);
		cedrus_write(dev, VE_VP8_FWD_CHROMA, 0);
	}

	qindex = vb2_find_timestamp(cap_q, slice->golden_frame_ts, 0);
	if (qindex >= 0) {
		luma_addr = cedrus_dst_buf_addr(ctx, qindex, 0);
		chroma_addr = cedrus_dst_buf_addr(ctx, qindex, 1);
		cedrus_write(dev, VE_VP8_BWD_LUMA, luma_addr);
		cedrus_write(dev, VE_VP8_BWD_CHROMA, chroma_addr);
	} else {
		cedrus_write(dev, VE_VP8_BWD_LUMA, 0);
		cedrus_write(dev, VE_VP8_BWD_CHROMA, 0);
	}

	qindex = vb2_find_timestamp(cap_q, slice->alt_frame_ts, 0);
	if (qindex >= 0) {
		luma_addr = cedrus_dst_buf_addr(ctx, qindex, 0);
		chroma_addr = cedrus_dst_buf_addr(ctx, qindex, 1);
		cedrus_write(dev, VE_VP8_ALT_LUMA, luma_addr);
		cedrus_write(dev, VE_VP8_ALT_CHROMA, chroma_addr);
	} else {
		cedrus_write(dev, VE_VP8_ALT_LUMA, 0);
		cedrus_write(dev, VE_VP8_ALT_CHROMA, 0);
	}

	cedrus_write(dev, VE_H264_CTRL, VE_H264_CTRL_VP8 |
		     VE_H264_CTRL_DECODE_ERR_INT |
		     VE_H264_CTRL_SLICE_DECODE_INT);

	if (slice->lf.level) {
		ctx->codec.vp8.last_filter_type =
			!!(slice->lf.flags & V4L2_VP8_LF_FILTER_TYPE_SIMPLE);
		ctx->codec.vp8.last_frame_p_type =
			!V4L2_VP8_FRAME_IS_KEY_FRAME(slice);
		ctx->codec.vp8.last_sharpness_level =
			slice->lf.sharpness_level;
	}
}

static int cedrus_vp8_start(struct cedrus_ctx *ctx)
{
	struct cedrus_dev *dev = ctx->dev;

	ctx->codec.vp8.entropy_probs_buf =
		dma_alloc_coherent(dev->dev, CEDRUS_ENTROPY_PROBS_SIZE,
				   &ctx->codec.vp8.entropy_probs_buf_dma,
				   GFP_KERNEL);
	if (!ctx->codec.vp8.entropy_probs_buf)
		return -ENOMEM;

	/*
	 * This offset has been discovered by reverse engineering, we don’t know
	 * what it actually means.
	 */
	memcpy(&ctx->codec.vp8.entropy_probs_buf[2048],
	       prob_table_init, sizeof(prob_table_init));

	return 0;
}

static void cedrus_vp8_stop(struct cedrus_ctx *ctx)
{
	struct cedrus_dev *dev = ctx->dev;

	cedrus_engine_disable(dev);

	dma_free_coherent(dev->dev, CEDRUS_ENTROPY_PROBS_SIZE,
			  ctx->codec.vp8.entropy_probs_buf,
			  ctx->codec.vp8.entropy_probs_buf_dma);
}

static void cedrus_vp8_trigger(struct cedrus_ctx *ctx)
{
	struct cedrus_dev *dev = ctx->dev;

	cedrus_write(dev, VE_H264_TRIGGER_TYPE,
		     VE_H264_TRIGGER_TYPE_VP8_SLICE_DECODE);
}

struct cedrus_dec_ops cedrus_dec_ops_vp8 = {
	.irq_clear	= cedrus_vp8_irq_clear,
	.irq_disable	= cedrus_vp8_irq_disable,
	.irq_status	= cedrus_vp8_irq_status,
	.setup		= cedrus_vp8_setup,
	.start		= cedrus_vp8_start,
	.stop		= cedrus_vp8_stop,
	.trigger	= cedrus_vp8_trigger,
};
