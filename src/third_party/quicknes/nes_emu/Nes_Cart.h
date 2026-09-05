
// NES cartridge data (PRG, CHR, mapper)

// Nes_Emu 0.7.0

#ifndef NES_CART_H
#define NES_CART_H

#include <stdint.h>
#include "blargg_common.h"
#include "abstract_file.h"

class Nes_Cart {
public:
	Nes_Cart();
	~Nes_Cart();
	
	// Load iNES file
	const char * load_ines( Auto_File_Reader );
	static const char not_ines_file [];
	
	// to do: support UNIF?
	
	// True if data is currently loaded
	bool loaded() const { return prg_ != NULL; }
	
	// Free data
	void clear();
	
	// True if cartridge claims to have battery-backed memory
	bool has_battery_ram() const;
	long battery_ram_size() const;
	
	// Size of PRG data
	long prg_size() const { return prg_size_; }
	
	// Size of CHR data
	long chr_size() const { return chr_size_; }
	
	// Change size of PRG (code) data
	const char * resize_prg( long );
	
	// Change size of CHR (graphics) data 
	const char * resize_chr( long );
	
	// Set mapper and information bytes. LSB and MSB are the standard iNES header
	// bytes at offsets 6 and 7.
	void set_mapper( int mapper_lsb, int mapper_msb );
	void set_mapper_nes2( int mapper_lsb, int mapper_msb, int mapper_ext );
	
	unsigned mapper_data() const { return mapper; }
	
	// Initial mirroring setup
	int mirroring() const { return mapper & 0x09; }
	
	// iNES mapper code
	int mapper_code() const;
	int submapper_code() const;
	
	// Pointer to beginning of PRG data
	uint8_t      * prg()       { return prg_; }
	uint8_t const* prg() const { return prg_; }
	
	// Pointer to beginning of CHR data
	uint8_t      * chr()       { return chr_; }
	uint8_t const* chr() const { return chr_; }
	
	// End of public interface
private:
	enum { bank_size = 8 * 1024L }; // bank sizes must be a multiple of this
	uint8_t *prg_;
	uint8_t *chr_;
	long prg_size_;
	long chr_size_;
	long battery_ram_size_;
	unsigned mapper;
	unsigned mapper_code_value;
	unsigned submapper;
	long round_to_bank_size( long n );
};

/* AURORA_FINAL10_EEPROM_BATTERY_V1 */
inline long Nes_Cart::battery_ram_size() const
{
	if ( battery_ram_size_ > 0 )
		return battery_ram_size_;
	/* Existing Bandai EEPROM front-end compatibility: these mappers
	 * historically exposed one 8 KiB persistence window in QuickNES. */
	if ( mapper_code_value == 157 || mapper_code_value == 159 )
		return 0x2000;
	return 0;
}

inline bool Nes_Cart::has_battery_ram() const
{
	return battery_ram_size() > 0;
}

/* AURORA_NES2_HEADER_V1
 * Keep legacy byte-6 flags in mapper for battery/mirroring, while exposing
 * the decoded mapper ID independently. This lets NES 2.0 use mapper bits
 * 8-11 and a submapper without changing the hot emulation path.
 */
/* AURORA_NES2_HEADER_V2
 * Keep raw iNES flags in mapper (battery/mirroring), while storing the
 * decoded mapper number and NES 2.0 submapper separately.
 */
inline void Nes_Cart::set_mapper( int mapper_lsb, int mapper_msb )
{
	mapper = mapper_msb * 0x100 + mapper_lsb;
	mapper_code_value =
		(mapper_msb & 0xF0) | ((mapper_lsb >> 4) & 0x0F);
	submapper = 0;
}

inline void Nes_Cart::set_mapper_nes2(
	int mapper_lsb, int mapper_msb, int mapper_ext )
{
	mapper = mapper_msb * 0x100 + mapper_lsb;
	mapper_code_value =
		((mapper_ext & 0x0F) << 8) |
		(mapper_msb & 0xF0) |
		((mapper_lsb >> 4) & 0x0F);
	submapper = (mapper_ext >> 4) & 0x0F;
}

inline int Nes_Cart::mapper_code() const
{
	return (int) mapper_code_value;
}

inline int Nes_Cart::submapper_code() const
{
	return (int) submapper;
}

#endif
