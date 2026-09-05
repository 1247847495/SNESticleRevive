#pragma once
#include "Nes_Mapper.h"
#include <string.h>

class Mapper216 : public Nes_Mapper
{
    struct state_t
    {
        uint8_t prg;
        uint8_t chr;
    } s;

    void select_from_addr(nes_addr_t addr)
    {
        s.prg = (uint8_t)(addr & 1);
        s.chr = (uint8_t)((addr & 0x0e) >> 1);
        set_prg_bank(0x8000, bank_32k, s.prg);
        set_chr_bank(0, bank_8k, s.chr);
    }

public:
    Mapper216()
    {
        register_state(&s, sizeof s);
    }

    void reset_state()
    {
        memset(&s, 0, sizeof s);
        intercept_reads(0x5000, 1);
        intercept_writes(0x5000, 1);
    }

    void apply_mapping()
    {
        set_prg_bank(0x8000, bank_32k, s.prg);
        set_chr_bank(0, bank_8k, s.chr);
        intercept_reads(0x5000, 1);
        intercept_writes(0x5000, 1);
    }

    int read(nes_time_t time, nes_addr_t addr)
    {
        if (addr == 0x5000)
            return 0;
        return Nes_Mapper::read(time, addr);
    }

    bool write_intercepted(nes_time_t, nes_addr_t addr, int)
    {
        if (addr == 0x5000)
        {
            select_from_addr(addr);
            return true;
        }
        return false;
    }

    void write(nes_time_t, nes_addr_t addr, int)
    {
        select_from_addr(addr);
    }
};
