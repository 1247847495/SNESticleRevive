#pragma once
#include "Nes_Mapper.h"
#include <string.h>

class Mapper068 : public Nes_Mapper
{
    struct state_t
    {
        uint8_t chr[4];
        uint8_t nt[2];
        uint8_t prg;
        uint8_t mirror;
        uint8_t use_chr_nt;
        uint8_t ram_enabled;
    } s;

    void update_nametables()
    {
        if (!s.use_chr_nt)
        {
            switch (s.mirror & 3)
            {
                case 0: mirror_vert(); break;
                case 1: mirror_horiz(); break;
                case 2: mirror_single(0); break;
                case 3: mirror_single(1); break;
            }
            return;
        }

        for (int i = 0; i < 4; ++i)
        {
            int reg = 0;

            switch (s.mirror & 3)
            {
                case 0: reg = i & 1; break;
                case 1: reg = (i >> 1) & 1; break;
                case 2: reg = 0; break;
                case 3: reg = 1; break;
            }

            mirror_chr(i, s.nt[reg]);
        }
    }

    void update()
    {
        for (int i = 0; i < 4; ++i)
            set_chr_bank(i * 0x800, bank_2k, s.chr[i]);

        set_prg_bank(0x8000, bank_16k, s.prg & 7);
        set_prg_bank(0xc000, bank_16k, 7);

        enable_sram(s.ram_enabled != 0);
        update_nametables();
    }

public:
    Mapper068()
    {
        register_state(&s, sizeof s);
    }

    void reset_state()
    {
        memset(&s, 0, sizeof s);
        s.nt[0] = 0x80;
        s.nt[1] = 0x81;
    }

    void apply_mapping()
    {
        update();
    }

    void write(nes_time_t, nes_addr_t addr, int data)
    {
        switch (addr & 0xf000)
        {
            case 0x8000: s.chr[0] = (uint8_t)data; break;
            case 0x9000: s.chr[1] = (uint8_t)data; break;
            case 0xa000: s.chr[2] = (uint8_t)data; break;
            case 0xb000: s.chr[3] = (uint8_t)data; break;

            case 0xc000:
                s.nt[0] = (uint8_t)(data | 0x80);
                break;

            case 0xd000:
                s.nt[1] = (uint8_t)(data | 0x80);
                break;

            case 0xe000:
                s.mirror = (uint8_t)(data & 3);
                s.use_chr_nt = (uint8_t)((data & 0x10) != 0);
                break;

            case 0xf000:
                s.prg = (uint8_t)(data & 7);
                s.ram_enabled = (uint8_t)((data & 0x10) != 0);
                break;
        }

        update();
    }
};
