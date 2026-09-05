#pragma once
#include "Nes_Mapper.h"
#include <string.h>

class Mapper210 : public Nes_Mapper
{
    struct state_t
    {
        uint8_t chr[8];
        uint8_t prg[3];
        uint8_t ram_enabled;
        uint8_t mirror_mode;
    } s;

    bool is_175() const
    {
        int sub = cart().submapper_code();
        if (sub == 1) return true;
        if (sub == 2) return false;

        /* NES 2.0 submapper 0 / legacy iNES fallback:
         * battery-backed mapper-210 commercial carts are N175;
         * otherwise N340 is the compatible default. */
        return cart().has_battery_ram();
    }

    void update_mirroring()
    {
        if (is_175())
        {
            if (cart().mirroring() & 1)
                mirror_vert();
            else
                mirror_horiz();
            return;
        }

        switch (s.mirror_mode & 3)
        {
            case 0: mirror_single(0); break;
            case 1: mirror_vert(); break;
            case 2: mirror_single(1); break;
            case 3: mirror_horiz(); break;
        }
    }

    void update()
    {
        for (int i = 0; i < 8; ++i)
            set_chr_bank(i * 0x400, bank_1k, s.chr[i]);

        set_prg_bank(0x8000, bank_8k, s.prg[0] & 0x3f);
        set_prg_bank(0xa000, bank_8k, s.prg[1] & 0x3f);
        set_prg_bank(0xc000, bank_8k, s.prg[2] & 0x3f);
        set_prg_bank(0xe000, bank_8k, last_bank);

        if (is_175())
            enable_sram(s.ram_enabled != 0);
        else
            enable_sram(false);

        update_mirroring();
    }

public:
    Mapper210()
    {
        register_state(&s, sizeof s);
    }

    void reset_state()
    {
        memset(&s, 0, sizeof s);
    }

    void apply_mapping()
    {
        update();
    }

    void write(nes_time_t, nes_addr_t addr, int data)
    {
        int reg = addr & 0xf800;

        if (reg >= 0x8000 && reg <= 0xb800)
        {
            int bank = (reg - 0x8000) >> 11;
            s.chr[bank] = (uint8_t)data;
        }
        else switch (reg)
        {
            case 0xc000:
                if (is_175())
                    s.ram_enabled = (uint8_t)(data & 1);
                break;

            /* $C800-$DFFF do not exist on N175/N340. Some games write
             * N163-compatible setup values here; intentionally ignore. */
            case 0xc800:
            case 0xd000:
            case 0xd800:
                break;

            case 0xe000:
                s.prg[0] = (uint8_t)(data & 0x3f);
                if (!is_175())
                    s.mirror_mode = (uint8_t)((data >> 6) & 3);
                break;

            case 0xe800:
                s.prg[1] = (uint8_t)(data & 0x3f);
                break;

            case 0xf000:
                s.prg[2] = (uint8_t)(data & 0x3f);
                break;
        }

        update();
    }
};
