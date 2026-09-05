#pragma once
#include "Nes_Mapper.h"
#include <string.h>

class Mapper101 : public Nes_Mapper
{
    uint8_t chr_bank;

public:
    Mapper101()
    {
        register_state(&chr_bank, sizeof chr_bank);
    }

    void reset_state()
    {
        chr_bank = 0;
        intercept_writes(0x6000, 0x2000);
    }

    void apply_mapping()
    {
        set_prg_bank(0x8000, bank_32k, 0);
        set_chr_bank(0, bank_8k, chr_bank);
        intercept_writes(0x6000, 0x2000);
    }

    bool write_intercepted(nes_time_t, nes_addr_t addr, int data)
    {
        if (addr >= 0x6000 && addr <= 0x7fff)
        {
            chr_bank = (uint8_t)data;
            set_chr_bank(0, bank_8k, chr_bank);
            return true;
        }
        return false;
    }

    void write(nes_time_t, nes_addr_t, int)
    {
    }
};
