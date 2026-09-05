#pragma once
#include "Nes_Mapper.h"
#include <string.h>

template <bool Modern552>
class Mapper_TaitoX1017_AddOnly : public Nes_Mapper
{
    struct state_t
    {
        uint8_t chr_mode;
        uint8_t chr[6];
        uint8_t ram_permission[3];
        uint8_t prg[3];
        uint8_t mirror;
    } s;

    int prg_page(int value) const
    {
        value &= 0xff;

        if (!Modern552)
            return value >> 2;

        return
            ((value & 0x20) >> 5) |
            ((value & 0x10) >> 3) |
            ((value & 0x08) >> 1) |
            ((value & 0x04) << 1) |
            ((value & 0x02) << 3) |
            ((value & 0x01) << 5);
    }

    void update_chr()
    {
        int b0 = s.chr[0] & 0xfe;
        int b1 = s.chr[1] & 0xfe;

        if (!s.chr_mode)
        {
            set_chr_bank(0x0000, bank_1k, b0 + 0);
            set_chr_bank(0x0400, bank_1k, b0 + 1);
            set_chr_bank(0x0800, bank_1k, b1 + 0);
            set_chr_bank(0x0c00, bank_1k, b1 + 1);
            set_chr_bank(0x1000, bank_1k, s.chr[2]);
            set_chr_bank(0x1400, bank_1k, s.chr[3]);
            set_chr_bank(0x1800, bank_1k, s.chr[4]);
            set_chr_bank(0x1c00, bank_1k, s.chr[5]);
        }
        else
        {
            set_chr_bank(0x0000, bank_1k, s.chr[2]);
            set_chr_bank(0x0400, bank_1k, s.chr[3]);
            set_chr_bank(0x0800, bank_1k, s.chr[4]);
            set_chr_bank(0x0c00, bank_1k, s.chr[5]);
            set_chr_bank(0x1000, bank_1k, b0 + 0);
            set_chr_bank(0x1400, bank_1k, b0 + 1);
            set_chr_bank(0x1800, bank_1k, b1 + 0);
            set_chr_bank(0x1c00, bank_1k, b1 + 1);
        }
    }

    void update()
    {
        update_chr();

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
            case 0x7ef1:
            case 0x7ef2:
            case 0x7ef3:
            case 0x7ef4:
            case 0x7ef5:
                s.chr[addr - 0x7ef0] = (uint8_t)data;
                break;

            case 0x7ef6:
                s.mirror = (uint8_t)(data & 1);
                s.chr_mode = (uint8_t)((data >> 1) & 1);
                break;

            case 0x7ef7:
            case 0x7ef8:
            case 0x7ef9:
                s.ram_permission[addr - 0x7ef7] = (uint8_t)data;
                break;

            case 0x7efa:
                s.prg[0] = (uint8_t)prg_page(data);
                break;
            case 0x7efb:
                s.prg[1] = (uint8_t)prg_page(data);
                break;
            case 0x7efc:
                s.prg[2] = (uint8_t)prg_page(data);
                break;

            /* $7EFD-$7EFF are the recently documented IRQ block.
             * No commercial X1-017 game uses it; swallow writes here so
             * they never become accidental SRAM writes. */
            case 0x7efd:
            case 0x7efe:
            case 0x7eff:
                break;
        }

        update();
    }

public:
    Mapper_TaitoX1017_AddOnly()
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
        /* Add-only compromise: ordinary QuickNES 8 KiB SRAM mapping.
         * Banking/mirroring are accurate; fine-grained X1-017 RAM
         * permissions can be added later without touching the loader. */
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

typedef Mapper_TaitoX1017_AddOnly<false> Mapper082;
typedef Mapper_TaitoX1017_AddOnly<true>  Mapper552;
