/*
 * AURORA_NAMCO76_V2
 *
 * Mapper 76 / Namcot 3446.
 * Namco 108 variant where CHR registers 2-5 become four 2 KiB banks and
 * CHR registers 0/1 are inaccessible.
 */

#pragma once

#include "Nes_Mapper.h"

struct mapper076_state_t
{
	uint8_t bank [8];
	uint8_t mode;
};

BOOST_STATIC_ASSERT( sizeof (mapper076_state_t) == 9 );

class Mapper076 : public Nes_Mapper, mapper076_state_t {
public:
	Mapper076()
	{
		mapper076_state_t *state = this;
		register_state( state, sizeof *state );
	}

	virtual void reset_state()
	{
		/* No standard PRG-RAM. */
	}

	virtual void apply_mapping()
	{
		for ( int i = 0; i < 4; i++ )
			set_chr_bank(
				i << 11,
				bank_2k, bank [i + 2] );

		set_prg_bank( 0x8000, bank_8k, bank [6] );
		set_prg_bank( 0xA000, bank_8k, bank [7] );
		set_prg_bank( 0xC000, bank_8k, ~1 );
		set_prg_bank( 0xE000, bank_8k, ~0 );
	}

	virtual void write( nes_time_t, nes_addr_t addr, int data )
	{
		switch ( addr & 0xE001 )
		{
		case 0x8000:
			mode = data & 0x07;
			break;

		case 0x8001:
			switch ( mode )
			{
			case 2:
			case 3:
			case 4:
			case 5:
				bank [mode] = data & 0x3F;
				set_chr_bank(
					(mode - 2) << 11,
					bank_2k, bank [mode] );
				break;

			case 6:
			case 7:
				bank [mode] = data & 0x0F;
				set_prg_bank(
					0x8000 + ((mode - 6) << 13),
					bank_8k, bank [mode] );
				break;

			default:
				/* Registers 0 and 1 are not connected on mapper 76. */
				break;
			}
			break;
		}
	}
};
