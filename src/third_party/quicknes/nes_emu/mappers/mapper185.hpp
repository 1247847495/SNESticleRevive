#pragma once
#include "Nes_Mapper.h"
#include <string.h>

class Mapper185 : public Nes_Mapper
{
    uint8_t latch;

    bool chr_enabled() const
    {
        int sub = cart().submapper_code();

        if (sub == 0)
            return (latch & 0x0f) != 0 && latch != 0x13;

        if (sub >= 4 && sub <= 7)
            return (latch & 3) == (sub - 4);

        return true;
    }

    void update()
    {
        set_prg_bank(0x8000, bank_32k, 0);
        set_chr_bank(0, bank_8k, 0);
        set_chr_read_or(chr_enabled() ? 0 : 1);
    }

public:
    Mapper185()
    {
        register_state(&latch, sizeof latch);
    }

    void reset_state()
    {
        /* Deterministic power-on value with CHR enabled. */
        latch = cart().submapper_code() >= 4 ?
            (uint8_t)(cart().submapper_code() - 4) : 1;
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
