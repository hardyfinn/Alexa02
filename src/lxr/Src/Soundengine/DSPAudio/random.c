/*
 * rnd.c
 *
 *  Created on: 07.04.2012
 *
 * Copyright (C) 2012-2026 Erica Synths
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is part of the LXR firmware. It is licensed under the GNU General
 * Public License version 3, with the additional permission and terms in
 * NOTICE.md. See LICENSE and NOTICE.md in the root of the repository.
 */


#include "Soundengine/DSPAudio/random.h"
#include "stm32f7xx_hal.h"
//-------------------------------------------------------------
void initRng()
{
	/* Enable RNG clock source */
	//RCC_AHB2PeriphClockCmd(RCC_AHB2Periph_RNG, ENABLE);

	/* RNG Peripheral enable */
	//RNG_Cmd(ENABLE);
}
//-------------------------------------------------------------
/* Alexa02 port: the STM32 has a hardware random number generator.
 * On a computer a fast xorshift generator does the same job. */
uint32_t lxr_rngState = 0x9E3779B9u; /* part of the engine state each module keeps */
uint32_t GetRngValue()
{
	uint32_t x = lxr_rngState;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	lxr_rngState = x;
	return x;
}
//-------------------------------------------------------------

