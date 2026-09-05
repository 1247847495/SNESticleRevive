/* Copyright notice for this file:
 * Copyright (C) 2018
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Mapper implementation originally added to the libretro QuickNES port.
 */

#pragma once

#include "Nes_Mapper.h"

struct namco_88_state_t
{
	uint8_t bank [8];
	uint8_t mirr;
	uint8_t mode;
};

BOOST_STATIC_ASSERT( sizeof (namco_88_state_t) == 10 );

/* AURORA_NAMCO88_154_V2
 *
 * Mapper 88:
 * - registers 0/1 select 2 KiB CHR only from lower 64 KiB
 * - registers 2-5 select 1 KiB CHR only from upper 64 KiB
 *
 * Mapper 154:
 * - same CHR wiring
 * - bit 6 of every mapper write controls one-screen nametable selection
 */
template < bool _is154 >
class Mapper_Namco_34x3 : public Nes_Mapper, namco_88_state_t {
public:
	Mapper_Namco_34x3()
	{
		namco_88_state_t *state = this;
		register_state( state, sizeof *state );
	}

	virtual void reset_state()
	{
		/* No standard PRG-RAM on this Namco 108 family. */
	}

	virtual void apply_mapping()
	{
		set_chr_bank( 0x0000, bank_2k, bank [0] );
		set_chr_bank( 0x0800, bank_2k, bank [1] );

		for ( int i = 0; i < 4; i++ )
			set_chr_bank(
				0x1000 + (i << 10),
				bank_1k, bank [i + 2] );

		set_prg_bank( 0x8000, bank_8k, bank [6] );
		set_prg_bank( 0xA000, bank_8k, bank [7] );
		set_prg_bank( 0xC000, bank_8k, ~1 );
		set_prg_bank( 0xE000, bank_8k, ~0 );

		if ( _is154 )
			mirror_single( mirr );
	}

	virtual void write( nes_time_t, nes_addr_t addr, int data )
	{
		if ( _is154 )
		{
			mirr = (data >> 6) & 1;
			mirror_single( mirr );
		}

		switch ( addr & 0xE001 )
		{
		case 0x8000:
			mode = data & 0x07;
			break;

		case 0x8001:
			switch ( mode )
			{
			case 0:
			case 1:
				/* Lower half only: Mesen masks registers 0/1 to 0x3F. */
				bank [mode] = (data & 0x3E) >> 1;
				set_chr_bank(
					0x0000 + (mode << 11),
					bank_2k, bank [mode] );
				break;

			case 2:
			case 3:
			case 4:
			case 5:
				/* Upper 64 KiB selected by fixed CHR A16=1. */
				bank [mode] = (data & 0x3F) | 0x40;
				set_chr_bank(
					0x1000 + ((mode - 2) << 10),
					bank_1k, bank [mode] );
				break;

			case 6:
			case 7:
				bank [mode] = data & 0x0F;
				set_prg_bank(
					0x8000 + ((mode - 6) << 13),
					bank_8k, bank [mode] );
				break;
			}
			break;
		}
	}
};

typedef Mapper_Namco_34x3<false> Mapper088;
