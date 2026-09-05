#pragma once
#include "Nes_Mapper.h"
#include <string.h>

class Mapper048 : public Nes_Mapper
{
    struct state_t
    {
        uint8_t prg[2];
        uint8_t chr[6];
        uint8_t mirror;
        uint8_t irq_counter;
        uint8_t irq_reload;
        uint8_t irq_reload_pending;
        uint8_t irq_enabled;
        uint8_t irq_pending;
        int32_t next_scanline;
        int32_t irq_due;
    } s;

    enum
    {
        irq_fine_tune = 268,
        first_scanline = 20 * Nes_Ppu::scanline_len + irq_fine_tune,
        last_scanline = first_scanline + 240 * Nes_Ppu::scanline_len
    };

    int irq_delay() const
    {
        return cart().submapper_code() == 1 ? 6 : 22;
    }

    bool scanline_clock_enabled() const
    {
        /* AURORA_V3_3_A12_FAMILY_NOTIFY_FIX_20260831
         * Mapper 48's IRQ is MMC3-like and driven by filtered PPU A12.
         * Either BG or sprite rendering keeps the rendering fetch pipeline
         * active; do not use Nes_Mapper::ppu_enabled(), which is BG-only. */
        if (!(emu().ppu.w2001 & 0x18))
            return false;

        int ctrl = emu().ppu.w2000;
        bool sprite_8x16 = (ctrl & 0x20) != 0;
        bool same_table =
            ((ctrl & 0x18) == 0x00) || ((ctrl & 0x18) == 0x18);

        return !same_table || sprite_8x16;
    }

    void update_banks()
    {
        set_prg_bank(0x8000, bank_8k, s.prg[0]);
        set_prg_bank(0xa000, bank_8k, s.prg[1]);
        set_prg_bank(0xc000, bank_8k, last_bank - 1);
        set_prg_bank(0xe000, bank_8k, last_bank);

        set_chr_bank(0x0000, bank_2k, s.chr[0]);
        set_chr_bank(0x0800, bank_2k, s.chr[1]);
        set_chr_bank(0x1000, bank_1k, s.chr[2]);
        set_chr_bank(0x1400, bank_1k, s.chr[3]);
        set_chr_bank(0x1800, bank_1k, s.chr[4]);
        set_chr_bank(0x1c00, bank_1k, s.chr[5]);

        if (s.mirror)
            mirror_horiz();
        else
            mirror_vert();
    }

    bool clock_irq_counter(nes_time_t event_time)
    {
        if (s.irq_counter == 0 || s.irq_reload_pending)
        {
            s.irq_counter = s.irq_reload;
            s.irq_reload_pending = 0;
        }
        else
        {
            s.irq_counter--;
        }

        if (s.irq_counter == 0 && s.irq_enabled)
        {
            s.irq_due = (int32_t)(event_time + irq_delay());
            return true;
        }

        return false;
    }

    int clocks_until_trigger() const
    {
        if (!s.irq_enabled)
            return -1;

        uint8_t counter = s.irq_counter;
        bool reload_pending = s.irq_reload_pending != 0;

        for (int clocks = 1; clocks <= 258; ++clocks)
        {
            if (counter == 0 || reload_pending)
            {
                counter = s.irq_reload;
                reload_pending = false;
            }
            else
            {
                counter--;
            }

            if (counter == 0)
                return clocks;
        }

        return -1;
    }

public:
    Mapper048()
    {
        register_state(&s, sizeof s);
    }

    /* AURORA_V3_3_A12_FAMILY_NOTIFY_FIX_20260831
     * PPUCTRL pattern/8x16 changes and PPUMASK sprite changes can alter
     * the A12 scanline source used by this mapper's IRQ predictor. */
    virtual bool ppu_ctrl_mask_irq_sensitive() const { return true; }

    void reset_state()
    {
        memset(&s, 0, sizeof s);
        s.next_scanline = first_scanline;
        s.irq_due = -1;
    }

    void apply_mapping()
    {
        s.next_scanline = first_scanline;
        s.irq_due = -1;
        update_banks();
    }

    void run_until(nes_time_t end_time)
    {
        if (s.irq_due >= 0 && end_time >= s.irq_due)
        {
            s.irq_pending = 1;
            s.irq_due = -1;
        }

        /* AURORA_V4_A12_CURSOR_FORCECLOCK_20260831
         * Keep the synthetic A12 scanline cursor monotonic even while
         * rendering/A12 clocking is inactive. The old code advanced
         * next_scanline only when scanline_clock_enabled() was true, so
         * re-enabling rendering or changing pattern-table eligibility
         * could retroactively clock scanlines that had already elapsed. */
        const bool clock_scanlines = scanline_clock_enabled();
        int32_t end_ppu = (int32_t)(end_time * ppu_overclock);

        while (s.next_scanline < end_ppu &&
               s.next_scanline <= last_scanline)
        {
            if (clock_scanlines)
            {
                nes_time_t t = s.next_scanline / ppu_overclock;
                clock_irq_counter(t);
            }
            s.next_scanline += Nes_Ppu::scanline_len;
        }

        if (s.irq_due >= 0 && end_time >= s.irq_due)
        {
            s.irq_pending = 1;
            s.irq_due = -1;
        }
    }

    void end_frame(nes_time_t end_time)
    {
        run_until(end_time);
        s.next_scanline = first_scanline;

        if (s.irq_due >= 0)
            s.irq_due -= (int32_t)end_time;
    }

    nes_time_t next_irq(nes_time_t present)
    {
        run_until(present);

        if (s.irq_pending)
            return present;

        if (s.irq_due >= 0)
            return s.irq_due;

        if (!scanline_clock_enabled())
            return no_irq;

        int clocks = clocks_until_trigger();
        if (clocks < 0)
            return no_irq;

        int32_t ppu_t =
            s.next_scanline +
            (clocks - 1) * Nes_Ppu::scanline_len;

        if (ppu_t > last_scanline)
            return no_irq;

        return ppu_t / ppu_overclock + irq_delay();
    }

    void a12_clocked()
    {
        if (clock_irq_counter(mapper_clock()))
            irq_changed();
    }

    void write(nes_time_t time, nes_addr_t addr, int data)
    {
        run_until(time);

        switch (addr & 0xe003)
        {
            case 0x8000:
                s.prg[0] = (uint8_t)(data & 0x3f);
                update_banks();
                break;
            case 0x8001:
                s.prg[1] = (uint8_t)(data & 0x3f);
                update_banks();
                break;
            case 0x8002:
                s.chr[0] = (uint8_t)data;
                update_banks();
                break;
            case 0x8003:
                s.chr[1] = (uint8_t)data;
                update_banks();
                break;
            case 0xa000:
            case 0xa001:
            case 0xa002:
            case 0xa003:
                s.chr[2 + (addr & 3)] = (uint8_t)data;
                update_banks();
                break;

            case 0xc000:
                s.irq_pending = 0;
                s.irq_due = -1;
                s.irq_reload =
                    (uint8_t)((data ^ 0xff) +
                              (cart().submapper_code() == 1 ? 1 : 0));
                irq_changed();
                break;

            case 0xc001:
                s.irq_pending = 0;
                s.irq_due = -1;
                s.irq_counter = 0;
                s.irq_reload_pending = 1;
                irq_changed();
                break;

            case 0xc002:
                s.irq_enabled = 1;
                irq_changed();
                break;

            case 0xc003:
                s.irq_enabled = 0;
                s.irq_pending = 0;
                s.irq_due = -1;
                irq_changed();
                break;

            case 0xe000:
                s.mirror = (uint8_t)((data & 0x40) != 0);
                update_banks();
                break;
        }
    }
};
