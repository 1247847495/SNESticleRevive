
// Nes_Emu 0.7.0. http://www.slack.net/~ant/

#include "Nes_Cart.h"

#include <stdlib.h>
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

char const Nes_Cart::not_ines_file [] = "Not an iNES file";

Nes_Cart::Nes_Cart()
{
	prg_ = NULL;
	chr_ = NULL;
	clear();
}

Nes_Cart::~Nes_Cart()
{
	clear();
}

void Nes_Cart::clear()
{
	if ( prg_ )
		free( prg_ );
	prg_ = NULL;
	
	if ( chr_ )
		free( chr_ );
	chr_ = NULL;
	
	prg_size_ = 0;
	chr_size_ = 0;
	battery_ram_size_ = 0;
	mapper = 0;
	mapper_code_value = 0;
	submapper = 0;
}

long Nes_Cart::round_to_bank_size( long n )
{
	n += bank_size - 1;
	return n - n % bank_size;
}

const char * Nes_Cart::resize_prg( long size )
{
	if ( size != prg_size_ )
	{
		// padding allows CPU to always read operands of instruction, which
		// might go past end of data
		void* p = realloc( prg_, round_to_bank_size( size ) + 2 );
		CHECK_ALLOC( p || !size );
		prg_ = (uint8_t*) p;
		prg_size_ = size;
	}
	return 0;
}

const char * Nes_Cart::resize_chr( long size )
{
	if ( size != chr_size_ )
	{
		void* p = realloc( chr_, round_to_bank_size( size ) );
		CHECK_ALLOC( p || !size );
		chr_ = (uint8_t*) p;
		chr_size_ = size;
	}
	return 0;
}

// iNES reading

struct ines_header_t {
	uint8_t signature [4];
	uint8_t prg_count; // number of 16K PRG banks
	uint8_t chr_count; // number of 8K CHR banks
	uint8_t flags;     // MMMM FTBV Mapper low, Four-screen, Trainer, Battery, V mirror
	uint8_t flags2;    // MMMM --XX Mapper high 4 bits
	uint8_t zero [8];  // if zero [7] is non-zero, treat flags2 as zero
};
BOOST_STATIC_ASSERT( sizeof (ines_header_t) == 16 );

/* AURORA_NES2_HEADER_V2
 *
 * NES 2.0 supports mapper bits 8-11, submappers, and exponent/multiplier
 * ROM sizes. Galaxian is a real-world reason this matters: its physical
 * PRG-ROM is only 8 KiB.
 */
static long AuroraNes2RomSize(
	uint8_t lsb, uint8_t msb_nibble, long linear_unit )
{
	if ( msb_nibble != 0x0F )
	{
		unsigned long count =
			((unsigned long) msb_nibble << 8) | (unsigned long) lsb;
		unsigned long long bytes =
			(unsigned long long) count *
			(unsigned long long) linear_unit;

		if ( bytes > 0x7FFFFFFFULL )
			return -1;
		return (long) bytes;
	}

	/* exponent/multiplier: 2^E * (2*M + 1) */
	unsigned exponent = lsb >> 2;
	unsigned multiplier = ((unsigned) lsb & 3u) * 2u + 1u;

	if ( exponent >= 31 )
		return -1;

	unsigned long long bytes =
		((unsigned long long) 1 << exponent) * multiplier;

	if ( bytes > 0x7FFFFFFFULL )
		return -1;

	return (long) bytes;
}

static void AuroraMirrorRom(
	uint8_t *data, long raw_size, long mapped_size )
{
	if ( !data || raw_size <= 0 || mapped_size <= raw_size )
		return;

	for ( long i = raw_size; i < mapped_size; i++ )
		data [i] = data [i % raw_size];
}

/* AURORA_QUICKNES_DEZAEMON_SXROM_V1
 * Old iNES cannot describe SXROM's 32 KiB PRG-RAM. Keep the
 * compatibility exception exact (the canonical Dezaemon dump),
 * while also honoring explicit iNES/NES2 RAM-size metadata when
 * present. CRC is calculated once at load; zero runtime cost. */
static uint32_t AuroraCrc32Update(
	uint32_t crc, const void *data, long bytes )
{
	const uint8_t *p = (const uint8_t *) data;
	while ( bytes-- > 0 )
	{
		crc ^= *p++;
		for ( int bit = 0; bit < 8; ++bit )
			crc = (crc >> 1) ^
				(0xEDB88320u & (0u - (crc & 1u)));
	}
	return crc;
}

static long AuroraNes2RamSize( unsigned shift )
{
	if ( shift == 0 )
		return 0;
	/* NES 2.0 RAM size = 64 << shift bytes. */
	return 64L << shift;
}

const char * Nes_Cart::load_ines( Auto_File_Reader in )
{
	RETURN_ERR( in.open() );

	ines_header_t h;
	RETURN_ERR( in->read( &h, sizeof h ) );

	if ( 0 != memcmp( h.signature, "NES\x1A", 4 ) )
		return not_ines_file;

	const bool nes2 = (h.flags2 & 0x0C) == 0x08;

	/* Bytes 12-15 being non-zero is the classic sign of an archaic/dirty
	 * iNES header. In that case, the upper mapper nibble is unreliable. */
	const bool archaic_ines =
		!nes2 && (h.zero [4] || h.zero [5] || h.zero [6] || h.zero [7]);

	long raw_prg_bytes;
	long raw_chr_bytes;

	if ( nes2 )
	{
		set_mapper_nes2( h.flags, h.flags2, h.zero [0] );

		const uint8_t size_msb = h.zero [1];

		raw_prg_bytes = AuroraNes2RomSize(
			h.prg_count, size_msb & 0x0F, 16 * 1024L );

		raw_chr_bytes = AuroraNes2RomSize(
			h.chr_count, (size_msb >> 4) & 0x0F, 8 * 1024L );
	}
	else
	{
		set_mapper( h.flags, archaic_ines ? 0 : h.flags2 );

		/* Historical iNES convention: PRG count 0 means 256 banks. */
		long prg_banks = h.prg_count ? (long) h.prg_count : 256L;
		raw_prg_bytes = prg_banks * 16 * 1024L;
		raw_chr_bytes = (long) h.chr_count * 8 * 1024L;
	}

	/* Determine battery-backed PRG-RAM without changing legacy carts.
	 * Ordinary old iNES battery carts keep QuickNES' historical 8 KiB.
	 * Mapper 1 may explicitly request 16/32 KiB when the header can say so. */
	battery_ram_size_ = (h.flags & 0x02) ? 0x2000L : 0;
	if ( (h.flags & 0x02) && mapper_code() == 1 )
	{
		if ( nes2 )
		{
			const unsigned nv_shift = (h.zero [2] >> 4) & 0x0F;
			const long nv_bytes = AuroraNes2RamSize( nv_shift );
			if ( nv_bytes == 0x2000L || nv_bytes == 0x4000L ||
			     nv_bytes == 0x8000L )
				battery_ram_size_ = nv_bytes;
		}
		else if ( !archaic_ines && h.zero [0] >= 1 && h.zero [0] <= 4 )
		{
			/* iNES 1.0 byte 8: count of 8 KiB PRG-RAM units. */
			battery_ram_size_ = (long) h.zero [0] * 0x2000L;
		}
	}

	if ( raw_prg_bytes <= 0 || raw_chr_bytes < 0 )
		return "Invalid NES ROM size";

	if ( h.flags & 0x04 )
		RETURN_ERR( in->skip( 512 ) );

	long mapped_prg_bytes = raw_prg_bytes;
	long mapped_chr_bytes = raw_chr_bytes;

	/* AURORA_SMALL_PRG_MIRROR_V1
	 *
	 * Galaxian's Namcot 3301 PCB has an 8 KiB PRG chip. The absent address
	 * line mirrors that chip through the NROM CPU window. QuickNES normally
	 * maps PRG in 16 KiB units during reset, so materialize the physical
	 * mirror once at load time. There is zero per-frame cost.
	 */
	if ( mapper_code() == 0 && raw_prg_bytes == 8 * 1024L )
		mapped_prg_bytes = 16 * 1024L;

	/* AURORA_SAFE_CHR_MIRROR_V1
	 *
	 * NES 2.0 can describe CHR ROMs smaller than the normal 8 KiB PPU
	 * window. If a small ROM divides 8 KiB evenly, mirror it once at load
	 * time so normal QuickNES 1 KiB CHR page mapping remains valid.
	 */
	if ( raw_chr_bytes > 0 &&
	     raw_chr_bytes < 8 * 1024L &&
	     (8 * 1024L) % raw_chr_bytes == 0 )
	{
		mapped_chr_bytes = 8 * 1024L;
	}

	RETURN_ERR( resize_prg( mapped_prg_bytes ) );
	RETURN_ERR( resize_chr( mapped_chr_bytes ) );

	RETURN_ERR( in->read( prg(), raw_prg_bytes ) );
	AuroraMirrorRom( prg(), raw_prg_bytes, mapped_prg_bytes );

	if ( raw_chr_bytes > 0 )
	{
		RETURN_ERR( in->read( chr(), raw_chr_bytes ) );
		AuroraMirrorRom( chr(), raw_chr_bytes, mapped_chr_bytes );
	}

	/* Canonical unpatched Dezaemon (Japan).nes / No-Intro:
	 * CRC32 032594CD, mapper 1, 128 KiB PRG, CHR-RAM.
	 * Its old iNES header cannot express the SXROM 32 KiB PRG-RAM. */
	if ( !nes2 && mapper_code() == 1 && !(h.flags & 0x04) &&
	     raw_prg_bytes == 128 * 1024L && raw_chr_bytes == 0 )
	{
		uint32_t crc = 0xFFFFFFFFu;
		crc = AuroraCrc32Update( crc, &h, sizeof h );
		crc = AuroraCrc32Update( crc, prg(), raw_prg_bytes );
		crc ^= 0xFFFFFFFFu;
		if ( crc == 0x032594CDu )
			battery_ram_size_ = 0x8000L;
	}

	return 0;
}
