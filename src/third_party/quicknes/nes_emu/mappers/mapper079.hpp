/* Copyright notice for this file:
 * Copyright (C) 2018
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 * This mapper was added by retrowertz for Libretro port of QuickNES.
 *
 * Mapper 079
 * Mapper 113
 * Nina-03 / Nina-06
 */

#include "Nes_Mapper.h"

#pragma once

/* AURORA_QN_KRAZY_MIRRORING_V2_20260830
 * Bootgod/NesCartDB physical boards for both Krazy Kreatures revisions are
 * vertically mirrored, while historical ROM/header databases disagree.
 * MAME explicitly fixed NINA-06 mirroring to make Krazy Kreatures work.
 *
 * Identify only the two known clean PRG+CHR payloads here, so other mapper-79
 * boards keep their own header/PCB mirroring (several are genuinely horizontal).
 * Cost is load/reset/state-load only; no gameplay hot-path checks. */
static inline uint32_t aurora_nina_crc32_update(
    uint32_t crc, const uint8_t *p, long n)
{
    static const uint32_t table[16] =
    {
        0x00000000U, 0x1DB71064U, 0x3B6E20C8U, 0x26D930ACU,
        0x76DC4190U, 0x6B6B51F4U, 0x4DB26158U, 0x5005713CU,
        0xEDB88320U, 0xF00F9344U, 0xD6D6A3E8U, 0xCB61B38CU,
        0x9B64C2B0U, 0x86D3D2D4U, 0xA00AE278U, 0xBDBDF21CU
    };

    while (n-- > 0)
    {
        crc ^= *p++;
        crc = (crc >> 4) ^ table[crc & 0x0FU];
        crc = (crc >> 4) ^ table[crc & 0x0FU];
    }
    return crc;
}

static inline bool aurora_nina_krazy_vertical(const Nes_Cart &cart)
{
    if (cart.prg_size() != 0x8000 || cart.chr_size() != 0x8000)
        return false;

    uint32_t crc = 0xFFFFFFFFU;
    crc = aurora_nina_crc32_update(crc, cart.prg(), cart.prg_size());
    crc = aurora_nina_crc32_update(crc, cart.chr(), cart.chr_size());
    crc ^= 0xFFFFFFFFU;

    /* v1.0 NINA-03, v1.1 NINA-06 */
    return crc == 0x85323FD6U || crc == 0x637FE65CU;
}

template < bool multicart >
class Mapper_AveNina : public Nes_Mapper {
public:
	Mapper_AveNina()
	{
		register_state( &regs, 1 );
	}

	void write_regs();

	virtual void reset_state()
	{
		intercept_writes( 0x4000, 0x1000 );
		intercept_writes( 0x5000, 0x1000 );
	}

	virtual void apply_mapping()
	{
		/* AURORA_QN_KRAZY_MIRRORING_V2_20260830
		 * Force the verified PCB wiring only for the two Krazy Kreatures
		 * payloads. Do it in apply_mapping(), not just reset_state(), so a
		 * save-state load cannot restore the bad historical header mirroring. */
		if ( !multicart && aurora_nina_krazy_vertical( cart() ) )
			mirror_vert();

		write_intercepted( 0, 0x4100, regs );
	}

	virtual bool write_intercepted( nes_time_t, nes_addr_t addr , int data )
	{
		/* AURORA_QN_MAPPER79_NINA_EXACT_V1_20260830
		 * Real AVE NINA-03/NINA-06 decode:
		 *   010x xxx1 xxxx xxxx
		 * i.e. $4100-$41FF, $4300-$43FF ... $5F00-$5FFF.
		 * Do not claim the A8=0 expansion addresses. */
		if ( (addr & 0x4100) != 0x4100 )
			return false;

		regs = data;
		write_regs();
		return true;
	}

	virtual void write( nes_time_t, nes_addr_t, int )
	{
		/* AURORA_QN_MAPPER79_NINA_EXACT_V1_20260830
		 * Mapper 79 hardware has no register in $8000-$FFFF.
		 * QuickNES inherited a historical CNROM-compatibility alias used only
		 * for old mapper-3 dumps mislabelled as mapper 79. On a real NINA cart,
		 * writes here are ordinary ROM-space writes and must not switch CHR.
		 *
		 * Mapper 113 already ignored this path (multicart==true), so making the
		 * common high-ROM write a no-op is hardware-accurate for both variants. */
	}

	uint8_t regs;
};

template < bool multicart >
void Mapper_AveNina< multicart >::write_regs()
{
	if ( multicart == 0 )
	{
		set_prg_bank ( 0x8000, bank_32k, ( regs >> 3 ) & 0x01 );
		set_chr_bank ( 0, bank_8k, regs & 0x07 );
	}
	else
	{
		set_prg_bank ( 0x8000, bank_32k, ( regs >> 3 ) & 0x07 );
		set_chr_bank ( 0x0000, bank_8k, ( ( regs >> 3 ) & 0x08 ) | ( regs & 0x07 ) );
		if ( regs & 0x80 ) mirror_vert();
		else mirror_horiz();
	}
}

typedef Mapper_AveNina<false> Mapper079;
