#pragma once
#include "Nes_Mapper.h"
#include <string.h>

class Mapper013 : public Nes_Mapper
{
    uint8_t chr_bank;

public:
    Mapper013()
    {
        register_state(&chr_bank, sizeof chr_bank);
    }

    void reset_state()
    {
        chr_bank = 0;
    }

    long chr_ram_size() const
    {
        return 0x4000;
    }

    void apply_mapping()
    {
        set_prg_bank(0x8000, bank_32k, 0);
        set_chr_bank(0x0000, bank_4k, 0);
        set_chr_bank(0x1000, bank_4k, chr_bank & 3);
        mirror_vert();
    }

    void write(nes_time_t, nes_addr_t, int data)
    {
        chr_bank = data & 3;
        set_chr_bank(0x1000, bank_4k, chr_bank);
    }
};
