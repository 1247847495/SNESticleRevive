#pragma once

// Nes_Emu 0.7.0. http://www.slack.net/~ant/

#include "Nes_Mapper.h"

#include <string.h>

/* Copyright (C) 2004-2006 Shay Green. This module is free software; you
can redistribute it and/or modify it under the terms of the GNU Lesser
General Public License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version. This
module is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for
more details. You should have received a copy of the GNU Lesser General
Public License along with this module; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA */

#include "blargg_source.h"

// MMC1

class Mapper001 : public Nes_Mapper, mmc1_state_t {
public:
	Mapper001()
	{
		mmc1_state_t* state = this;
		register_state( state, sizeof *state );
		last_write_time = 0;
		last_write_valid = false;
	}

private:
	nes_time_t last_write_time;
	bool last_write_valid;

public:
	virtual void reset_state()
	{
		regs [0] = 0x0f;
		regs [1] = 0x00;
		regs [2] = 0x01;
		regs [3] = 0x00;
		/* AURORA_V3_QN_MMC_ACCURACY_KRAZY_CLEANUP_20260831: runtime-only MMC1 M2 write filter. */
		last_write_time = 0;
		last_write_valid = false;
	}
	
	int mmc1_sram_bank() const
	{
		/* SOROM/SXROM repurpose CHR-bank-0 output lines because these
		 * boards use CHR-RAM. 8 KiB carts remain bank 0. */
		if ( cart().chr_size() != 0 )
			return 0;
		const long bytes = cart().battery_ram_size();
		if ( bytes >= 0x8000 )
			return (regs [1] >> 2) & 3; /* SXROM: A13=bit2, A14=bit3 */
		if ( bytes >= 0x4000 )
			return (regs [1] >> 3) & 1; /* SOROM */
		return 0;
	}

	virtual void apply_mapping()
	{
		enable_sram(); // early MMC1 always had SRAM enabled
		set_sram_bank( mmc1_sram_bank() );
		register_changed( 0 );

		/* AURORA_V3_QN_MMC_ACCURACY_KRAZY_CLEANUP_20260831
		 * last_write_* is host/runtime timing state, not cartridge state.
		 * Reset it after state-load so the first real post-load write can
		 * never be discarded because of an old host timestamp. */
		last_write_valid = false;
	}

	/* AURORA_V3_1_QN_MMC1_FRAME_LATCH_HARDEN_20260831
	 * QuickNES mapper timestamps are relative to the current emulated frame.
	 * Do not carry the RMW write-suppression timestamp into the next frame:
	 * an unrelated later write could otherwise collide numerically with it.
	 * emulate_frame() calls end_frame() only after the current CPU instruction
	 * has completed, so the two write phases of one RMW cannot be split here. */
	virtual void end_frame( nes_time_t )
	{
		last_write_time = 0;
		last_write_valid = false;
	}
	
	virtual void write( nes_time_t time, nes_addr_t addr, int data )
	{
		/* AURORA_V3_QN_MMC_ACCURACY_KRAZY_CLEANUP_20260831
		 * Real MMC1 accepts the first serial-port write of a consecutive
		 * CPU-write sequence and ignores later D0 writes until a read cycle.
		 * QuickNES accounts an instruction's cycles up front, so the two
		 * write phases of an absolute RMW instruction reach the mapper with
		 * the SAME timestamp. Equal timestamps are therefore the precise
		 * representation available in this CPU core for that RMW pair.
		 *
		 * Bit 7 reset is intentionally handled FIRST: real MMC1 never filters
		 * reset writes, including a reset occurring on the second RMW write. */
		if ( data & 0x80 )
		{
			last_write_time = time;
			last_write_valid = true;
			bit = 0;
			buf = 0;
			regs [0] |= 0x0c;
			register_changed( 0 );
			return;
		}

		if ( last_write_valid && time == last_write_time )
			return;

		last_write_time = time;
		last_write_valid = true;

		buf |= (data & 1) << bit;
		bit++;

		if ( bit >= 5 )
		{
			int reg = addr >> 13 & 3;
			regs [reg] = buf & 0x1f;
			bit = 0;
			buf = 0;
			register_changed( reg );
		}
	}

	
	void register_changed( int reg )
	{
		// Mirroring
		if ( reg == 0 )
		{
			int mode = regs [0] & 3;
			if ( mode < 2 )
				mirror_single( mode & 1 );
			else if ( mode == 2 )
				mirror_vert();
			else
				mirror_horiz();
		}
		
		/* In normal 8 KiB CHR mode, CHR bank register 0 drives the
		 * repurposed SOROM/SXROM PRG-RAM address lines. */
		if ( reg == 1 && cart().chr_size() == 0 &&
		     cart().battery_ram_size() > 0x2000 )
			set_sram_bank( mmc1_sram_bank() );

		// CHR
		if ( reg < 3 && cart().chr_size() > 0 )
		{
			if ( regs [0] & 0x10 )
			{
				set_chr_bank( 0x0000, bank_4k, regs [1] );
				set_chr_bank( 0x1000, bank_4k, regs [2] );
			}
			else
			{
				set_chr_bank( 0, bank_8k, regs [1] >> 1 );
			}
		}
		
		// PRG
		int bank = (regs [1] & 0x10) | (regs [3] & 0x0f);
		if ( !(regs [0] & 0x08) )
		{
			set_prg_bank( 0x8000, bank_32k, bank >> 1 );
		}
		else if ( regs [0] & 0x04 )
		{
			set_prg_bank( 0x8000, bank_16k, bank );
			set_prg_bank( 0xC000, bank_16k, bank | 0x0f );
		}
		else
		{
			set_prg_bank( 0x8000, bank_16k, bank & ~0x0f );
			set_prg_bank( 0xC000, bank_16k, bank );
		}
	}
};


