#pragma once
#include "Nes_Mapper.h"
#include <string.h>

/* Taito X1-005 add-only implementation.
 * Banking is native; SRAM remains permissively mapped by QuickNES. */
class Mapper080 : public Nes_Mapper
{
    struct state_t
    {
        uint8_t chr[8];
        uint8_t prg[3];
        uint8_t mirror;
        uint8_t ram_permission;
    } s;

    void update()
    {
        for (int i = 0; i < 8; ++i)
            set_chr_bank(i * 0x400, bank_1k, s.chr[i]);

        set_prg_bank(0x8000, bank_8k, s.prg[0]);
        set_prg_bank(0xa000, bank_8k, s.prg[1]);
        set_prg_bank(0xc000, bank_8k, s.prg[2]);
        set_prg_bank(0xe000, bank_8k, last_bank);

        if (s.mirror)
            mirror_vert();
        else
            mirror_horiz();
    }

    void reg_write(nes_addr_t addr, int data)
    {
        switch (addr)
        {
            case 0x7ef0:
                s.chr[0] = (uint8_t)data;
                s.chr[1] = (uint8_t)(data + 1);
                break;
            case 0x7ef1:
                s.chr[2] = (uint8_t)data;
                s.chr[3] = (uint8_t)(data + 1);
                break;
            case 0x7ef2: s.chr[4] = (uint8_t)data; break;
            case 0x7ef3: s.chr[5] = (uint8_t)data; break;
            case 0x7ef4: s.chr[6] = (uint8_t)data; break;
            case 0x7ef5: s.chr[7] = (uint8_t)data; break;
            case 0x7ef6:
            case 0x7ef7:
                s.mirror = (uint8_t)(data & 1);
                break;
            case 0x7ef8:
            case 0x7ef9:
                s.ram_permission = (uint8_t)data;
                break;
            case 0x7efa:
            case 0x7efb:
                s.prg[0] = (uint8_t)data;
                break;
            case 0x7efc:
            case 0x7efd:
                s.prg[1] = (uint8_t)data;
                break;
            case 0x7efe:
            case 0x7eff:
                s.prg[2] = (uint8_t)data;
                break;
        }

        update();
    }

public:
    Mapper080()
    {
        register_state(&s, sizeof s);
    }

    void reset_state()
    {
        memset(&s, 0, sizeof s);
        intercept_writes(0x7e00, 0x0200);
    }

    void apply_mapping()
    {
        enable_sram();
        intercept_writes(0x7e00, 0x0200);
        update();
    }

    bool write_intercepted(
        nes_time_t, nes_addr_t addr, int data)
    {
        if (addr >= 0x7ef0 && addr <= 0x7eff)
        {
            reg_write(addr, data);
            return true;
        }

        return false;
    }

    void write(nes_time_t, nes_addr_t, int)
    {
    }
};
