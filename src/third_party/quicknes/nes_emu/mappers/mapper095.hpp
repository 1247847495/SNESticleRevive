/*
 * AURORA_NAMCO95_V2
 *
 * Mapper 95 / Namcot 3425, used by Dragon Buster.
 *
 * CHR A15 directly controls CIRAM A10:
 *   nametable 0/1 use CHR register 0 bit 5
 *   nametable 2/3 use CHR register 1 bit 5
 *
 * This matches Mesen2's Namco108_95 behavior.
 */

#pragma once

#include "Nes_Mapper.h"

struct mapper095_state_t
{
	uint8_t bank [8];
	uint8_t mode;
};

BOOST_STATIC_ASSERT( sizeof (mapper095_state_t) == 9 );

class Mapper095 : public Nes_Mapper, mapper095_state_t {
public:
	Mapper095()
	{
		mapper095_state_t *state = this;
		register_state( state, sizeof *state );
	}

	virtual void reset_state()
	{
		/* No standard PRG-RAM. */
	}

	void update_nametables()
	{
		int nt0 = (bank [0] >> 5) & 1;
		int nt1 = (bank [1] >> 5) & 1;
		mirror_manual( nt0, nt0, nt1, nt1 );
	}

	virtual void apply_mapping()
	{
		set_chr_bank( 0x0000, bank_2k, bank [0] >> 1 );
		set_chr_bank( 0x0800, bank_2k, bank [1] >> 1 );

		for ( int i = 0; i < 4; i++ )
			set_chr_bank(
				0x1000 + (i << 10),
				bank_1k, bank [i + 2] );

		set_prg_bank( 0x8000, bank_8k, bank [6] );
		set_prg_bank( 0xA000, bank_8k, bank [7] );
		set_prg_bank( 0xC000, bank_8k, ~1 );
		set_prg_bank( 0xE000, bank_8k, ~0 );

		update_nametables();
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
			case 0:
			case 1:
				/* Keep bit 5 for CIRAM selection; bit 0 is not connected
				 * to the 2 KiB CHR bank address. */
				bank [mode] = data & 0x3E;
				set_chr_bank(
					0x0000 + (mode << 11),
					bank_2k, bank [mode] >> 1 );
				update_nametables();
				break;

			case 2:
			case 3:
			case 4:
			case 5:
				bank [mode] = data & 0x3F;
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
