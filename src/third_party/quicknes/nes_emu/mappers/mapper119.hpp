#pragma once
#include "mapper004.hpp"

class Mapper119 : public Mapper004
{
    void map_tqrom_1k(nes_addr_t addr, int bank)
    {
        bank &= 0xff;

        if (bank >= 0x40 && bank <= 0x7f)
            set_chr_ram_bank(addr, bank_1k, bank & 7);
        else
            set_chr_bank(addr, bank_1k, bank);
    }

public:
    long aux_chr_ram_size() const
    {
        return 0x2000;
    }

    void update_chr_banks()
    {
        int x = (mode >> 7 & 1) * 0x1000;

        int b0 = banks[0] & 0xfe;
        int b1 = banks[1] & 0xfe;

        map_tqrom_1k(0x0000 ^ x, b0 + 0);
        map_tqrom_1k(0x0400 ^ x, b0 + 1);
        map_tqrom_1k(0x0800 ^ x, b1 + 0);
        map_tqrom_1k(0x0c00 ^ x, b1 + 1);

        map_tqrom_1k(0x1000 ^ x, banks[2]);
        map_tqrom_1k(0x1400 ^ x, banks[3]);
        map_tqrom_1k(0x1800 ^ x, banks[4]);
        map_tqrom_1k(0x1c00 ^ x, banks[5]);
    }
};
