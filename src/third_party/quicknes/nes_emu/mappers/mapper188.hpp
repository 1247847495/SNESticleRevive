#pragma once
#include "Nes_Mapper.h"
#include <string.h>

class Mapper188 : public Nes_Mapper
{
    struct state_t
    {
        uint8_t prg;
        uint8_t mirror;
    } s;

    void update()
    {
        int bank;

        if (s.prg & 0x10)
            bank = s.prg & 7;
        else
            bank = (s.prg & 7) | 8;

        set_prg_bank(0x8000, bank_16k, bank);
        set_prg_bank(0xc000, bank_16k, 7);
        set_chr_bank(0, bank_8k, 0);

        if (s.mirror)
            mirror_horiz();
        else
            mirror_vert();
    }

public:
    Mapper188()
    {
        register_state(&s, sizeof s);
    }

    void reset_state()
    {
        memset(&s, 0, sizeof s);
        s.prg = 0x10;
        intercept_reads(0x6000, 0x2000);
    }

    void apply_mapping()
    {
        intercept_reads(0x6000, 0x2000);
        update();
    }

    int read(nes_time_t time, nes_addr_t addr)
    {
        if (addr >= 0x6000 && addr <= 0x7fff)
        {
            /* Microphone input is a frontend peripheral; mapper banking is
             * still correct when no microphone is connected. */
            return addr >> 8;
        }

        return Nes_Mapper::read(time, addr);
    }

    void write(nes_time_t, nes_addr_t, int data)
    {
        s.prg = (uint8_t)data;
        s.mirror = (uint8_t)((data & 0x20) != 0);
        update();
    }
};
