#pragma once
#include "Nes_Mapper.h"
#include <string.h>

class Mapper096 : public Nes_Mapper
{
    struct state_t
    {
        uint8_t prg;
        uint8_t outer_chr;
        uint8_t inner_chr;
        uint16_t last_vram_addr;
    } s;

    void update_chr_immediate()
    {
        int base = s.outer_chr | s.inner_chr;

        /* Called from the renderer's VRAM hook, so do not recurse through
         * Nes_Mapper::set_chr_bank(), which would render_until() again. */
        emu().ppu.set_chr_bank(0x0000, 0x1000, base << 12);
        emu().ppu.set_chr_bank(0x1000, 0x1000,
                              (s.outer_chr | 0x03) << 12);
    }

public:
    Mapper096()
    {
        register_state(&s, sizeof s);
    }

    long chr_ram_size() const
    {
        return 0x8000;
    }

    bool needs_vram_address_hook() const
    {
        return true;
    }

    void reset_state()
    {
        memset(&s, 0, sizeof s);
    }

    void apply_mapping()
    {
        set_prg_bank(0x8000, bank_32k, s.prg);
        update_chr_immediate();
    }

    void vram_address_changed(nes_addr_t addr)
    {
        if ((s.last_vram_addr & 0x3000) != 0x2000 &&
            (addr & 0x3000) == 0x2000)
        {
            s.inner_chr = (uint8_t)((addr >> 8) & 3);
            update_chr_immediate();
        }

        s.last_vram_addr = (uint16_t)addr;
    }

    void write(nes_time_t, nes_addr_t addr, int data)
    {
        data = handle_bus_conflict(addr, data);
        s.prg = (uint8_t)(data & 3);
        s.outer_chr = (uint8_t)(data & 4);
        set_prg_bank(0x8000, bank_32k, s.prg);
        update_chr_immediate();
    }
};
