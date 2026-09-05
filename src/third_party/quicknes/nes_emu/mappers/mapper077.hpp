#pragma once
#include "Nes_Mapper.h"
#include <string.h>

class Mapper077 : public Nes_Mapper
{
    uint8_t latch;

public:
    Mapper077()
    {
        register_state(&latch, sizeof latch);
    }

    long aux_chr_ram_size() const
    {
        return 0x1800;
    }

    void reset_state()
    {
        latch = 0;
    }

    void update()
    {
        set_prg_bank(0x8000, bank_32k, latch & 0x0f);

        /* Irem LROG017: first 2 KiB are CHR ROM, upper 6 KiB are
         * three independent 2 KiB CHR-RAM banks. */
        set_chr_bank(0x0000, bank_2k, (latch >> 4) & 0x0f);
        set_chr_ram_bank(0x0800, bank_2k, 0);
        set_chr_ram_bank(0x1000, bank_2k, 1);
        set_chr_ram_bank(0x1800, bank_2k, 2);

        mirror_full();
    }

    void apply_mapping()
    {
        update();
    }

    void write(nes_time_t, nes_addr_t addr, int data)
    {
        latch = (uint8_t)handle_bus_conflict(addr, data);
        update();
    }
};
