#pragma once
#include "Nes_Mapper.h"
#include <string.h>

template <bool Is158>
class Mapper_Rambo1 : public Nes_Mapper
{
    struct state_t
    {
        uint8_t regs[16];
        uint8_t reg8000;
        uint8_t rega000;
        uint8_t current;
        uint8_t chr_mode;
        uint8_t prg_mode;
        uint8_t irq_enabled;
        uint8_t irq_cycle_mode;
        uint8_t need_reload;
        uint8_t irq_counter;
        uint8_t irq_reload;
        uint8_t irq_pending;
        uint8_t force_clock;
        uint8_t nt[4];
        int32_t next_scanline;
        int32_t next_cpu_clock;
        int32_t irq_due;
    } s;

    enum
    {
        irq_fine_tune = 268,
        first_scanline = 20 * Nes_Ppu::scanline_len + irq_fine_tune,
        last_scanline = first_scanline + 240 * Nes_Ppu::scanline_len
    };

    void update_prg()
    {
        if (!s.prg_mode)
        {
            set_prg_bank(0x8000, bank_8k, s.regs[6]);
            set_prg_bank(0xa000, bank_8k, s.regs[7]);
            set_prg_bank(0xc000, bank_8k, s.regs[15]);
            set_prg_bank(0xe000, bank_8k, last_bank);
        }
        else
        {
            set_prg_bank(0x8000, bank_8k, s.regs[15]);
            set_prg_bank(0xa000, bank_8k, s.regs[7]);
            set_prg_bank(0xc000, bank_8k, s.regs[6]);
            set_prg_bank(0xe000, bank_8k, last_bank);
        }
    }

    void update_chr()
    {
        int x = s.chr_mode ? 0x1000 : 0;

        set_chr_bank(0x0000 ^ x, bank_1k, s.regs[0]);
        set_chr_bank(0x0800 ^ x, bank_1k, s.regs[1]);
        set_chr_bank(0x1000 ^ x, bank_1k, s.regs[2]);
        set_chr_bank(0x1400 ^ x, bank_1k, s.regs[3]);
        set_chr_bank(0x1800 ^ x, bank_1k, s.regs[4]);
        set_chr_bank(0x1c00 ^ x, bank_1k, s.regs[5]);

        if (s.reg8000 & 0x20)
        {
            set_chr_bank(0x0400 ^ x, bank_1k, s.regs[8]);
            set_chr_bank(0x0c00 ^ x, bank_1k, s.regs[9]);
        }
        else
        {
            set_chr_bank(0x0400 ^ x, bank_1k, s.regs[0] | 1);
            set_chr_bank(0x0c00 ^ x, bank_1k, s.regs[1] | 1);
        }
    }

    void update_mirror()
    {
        if (Is158)
        {
            mirror_manual(s.nt[0] & 1, s.nt[1] & 1,
                          s.nt[2] & 1, s.nt[3] & 1);
        }
        else
        {
            if (s.rega000 & 1)
                mirror_horiz();
            else
                mirror_vert();
        }
    }

    void update_state()
    {
        s.current = s.reg8000 & 0x0f;
        s.chr_mode = (s.reg8000 >> 7) & 1;
        s.prg_mode = (s.reg8000 >> 6) & 1;
        update_prg();
        update_chr();
        update_mirror();
    }

    void clock_irq(nes_time_t event_time, int delay)
    {
        if (s.need_reload)
        {
            if (s.irq_reload <= 1)
                s.irq_counter = (uint8_t)(s.irq_reload + 1);
            else
                s.irq_counter = (uint8_t)(s.irq_reload + 2);

            s.need_reload = 0;
        }
        else if (s.irq_counter == 0)
        {
            s.irq_counter = (uint8_t)(s.irq_reload + 1);
        }

        s.irq_counter--;

        if (s.irq_counter == 0 && s.irq_enabled)
            s.irq_due = (int32_t)(event_time + delay);
    }

    bool scanline_clock_enabled() const
    {
        /* AURORA_V3_3_A12_FAMILY_NOTIFY_FIX_20260831
         * RAMBO-1 scanline mode follows PPU A12. Either BG or sprite
         * rendering is enough to keep the fetch pipeline active.
         * CPU-cycle IRQ mode bypasses this helper entirely. */
        if (!(emu().ppu.w2001 & 0x18))
            return false;

        int ctrl = emu().ppu.w2000;
        bool sprite_8x16 = (ctrl & 0x20) != 0;
        bool same_table =
            ((ctrl & 0x18) == 0x00) || ((ctrl & 0x18) == 0x18);

        return !same_table || sprite_8x16;
    }

    int predicted_clocks_to_irq() const
    {
        if (!s.irq_enabled)
            return -1;

        uint8_t c = s.irq_counter;
        bool reload = s.need_reload != 0;

        for (int n = 1; n <= 260; ++n)
        {
            if (reload)
            {
                c = (uint8_t)(s.irq_reload <= 1 ?
                              s.irq_reload + 1 :
                              s.irq_reload + 2);
                reload = false;
            }
            else if (c == 0)
            {
                c = (uint8_t)(s.irq_reload + 1);
            }

            c--;

            if (c == 0)
                return n;
        }

        return -1;
    }

    void update_158_nt(int data)
    {
        if (!Is158)
            return;

        int nt = (data >> 7) & 1;

        if (s.chr_mode)
        {
            switch (s.current & 7)
            {
                case 2: s.nt[0] = (uint8_t)nt; break;
                case 3: s.nt[1] = (uint8_t)nt; break;
                case 4: s.nt[2] = (uint8_t)nt; break;
                case 5: s.nt[3] = (uint8_t)nt; break;
            }
        }
        else
        {
            switch (s.current & 7)
            {
                case 0:
                    s.nt[0] = s.nt[1] = (uint8_t)nt;
                    break;
                case 1:
                    s.nt[2] = s.nt[3] = (uint8_t)nt;
                    break;
            }
        }

        update_mirror();
    }

public:
    Mapper_Rambo1()
    {
        register_state(&s, sizeof s);
    }

    /* AURORA_V3_3_A12_FAMILY_NOTIFY_FIX_20260831
     * Applies to both Mapper064 and Mapper158 (same RAMBO-1 template).
     * Only scanline mode depends on these PPU inputs; cycle mode remains
     * governed solely by its 4-CPU-cycle prescaler. */
    virtual bool ppu_ctrl_mask_irq_sensitive() const { return true; }

    void reset_state()
    {
        memset(&s, 0, sizeof s);

        s.regs[0] = 0;
        s.regs[1] = 2;
        s.regs[2] = 4;
        s.regs[3] = 5;
        s.regs[4] = 6;
        s.regs[5] = 7;
        s.regs[6] = 0;
        s.regs[7] = 1;
        s.regs[8] = 8;
        s.regs[9] = 9;
        s.regs[15] = 0xfe;

        if (cart().mirroring() & 1)
        {
            s.nt[0] = 0; s.nt[1] = 1;
            s.nt[2] = 0; s.nt[3] = 1;
        }
        else
        {
            s.nt[0] = 0; s.nt[1] = 0;
            s.nt[2] = 1; s.nt[3] = 1;
        }

        s.next_scanline = first_scanline;
        s.next_cpu_clock = 4;
        s.irq_due = -1;
    }

    void apply_mapping()
    {
        enable_sram();
        s.next_scanline = first_scanline;
        if (s.next_cpu_clock <= 0)
            s.next_cpu_clock = 4;
        update_state();
    }

    void run_until(nes_time_t end_time)
    {
        if (s.irq_due >= 0 && end_time >= s.irq_due)
        {
            s.irq_pending = 1;
            s.irq_due = -1;
        }

        /* AURORA_V4_A12_CURSOR_FORCECLOCK_20260831
         * RAMBO-1 has two IRQ clock sources. In CPU-cycle mode, A12 is
         * ignored but the synthetic scanline cursor must still advance so
         * switching back cannot replay old scanlines. When leaving CPU
         * mode, force_clock remains a real one-shot CPU-clock event while
         * A12 scanline events are active again, so merge the two event
         * streams chronologically instead of making force_clock suppress
         * the scanline path for the whole run_until() interval. */
        const int32_t end_ppu = (int32_t)(end_time * ppu_overclock);

        if (s.irq_cycle_mode)
        {
            while (s.next_cpu_clock <= end_time)
            {
                clock_irq(s.next_cpu_clock, 1);
                s.next_cpu_clock += 4;
            }

            while (s.next_scanline < end_ppu &&
                   s.next_scanline <= last_scanline)
                s.next_scanline += Nes_Ppu::scanline_len;
        }
        else
        {
            const bool clock_scanlines = scanline_clock_enabled();

            while (true)
            {
                const bool scan_due =
                    s.next_scanline < end_ppu &&
                    s.next_scanline <= last_scanline;
                const bool force_due =
                    s.force_clock && s.next_cpu_clock <= end_time;

                if (!scan_due && !force_due)
                    break;

                const int32_t force_ppu =
                    (int32_t)(s.next_cpu_clock * ppu_overclock);

                if (force_due &&
                    (!scan_due || force_ppu <= s.next_scanline))
                {
                    clock_irq(s.next_cpu_clock, 1);
                    s.next_cpu_clock += 4;
                    s.force_clock = 0;
                }
                else
                {
                    if (clock_scanlines)
                        clock_irq(s.next_scanline / ppu_overclock, 2);
                    s.next_scanline += Nes_Ppu::scanline_len;
                }
            }
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
        s.next_cpu_clock -= (int32_t)end_time;
        while (s.next_cpu_clock <= 0)
            s.next_cpu_clock += 4;

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

        int clocks = predicted_clocks_to_irq();
        if (clocks < 0)
            return no_irq;

        if (s.irq_cycle_mode)
            return s.next_cpu_clock + (clocks - 1) * 4 + 1;

        /* A pending force_clock is a mapper event even after CPU-cycle
         * mode has been cleared. Stop the CPU no later than that event
         * (+ RAMBO-1's one-cycle IRQ delay), then run_until() will consume
         * it and next_irq() can recalculate from the resulting state.
         * Returning an early event is safe even when this particular
         * force clock does not itself assert IRQ. */
        nes_time_t force_time = no_irq;
        if (s.force_clock && s.irq_enabled)
            force_time = s.next_cpu_clock + 1;

        nes_time_t scanline_time = no_irq;
        if (scanline_clock_enabled())
        {
            int32_t ppu_t =
                s.next_scanline +
                (clocks - 1) * Nes_Ppu::scanline_len;

            if (ppu_t <= last_scanline)
                scanline_time = ppu_t / ppu_overclock + 2;
        }

        if (force_time != no_irq &&
            (scanline_time == no_irq || force_time < scanline_time))
            return force_time;

        return scanline_time;
    }

    void a12_clocked()
    {
        if (!s.irq_cycle_mode)
        {
            clock_irq(mapper_clock(), 2);
            irq_changed();
        }
    }

    void write(nes_time_t time, nes_addr_t addr, int data)
    {
        run_until(time);

        switch (addr & 0xe001)
        {
            case 0x8000:
                s.reg8000 = (uint8_t)data;
                update_state();
                break;

            case 0x8001:
                update_158_nt(data);
                s.regs[s.current & 0x0f] = (uint8_t)data;
                update_state();
                break;

            case 0xa000:
                if (!Is158)
                {
                    s.rega000 = (uint8_t)data;
                    update_mirror();
                }
                break;

            case 0xc000:
                s.irq_reload = (uint8_t)data;
                break;

            case 0xc001:
                if (s.irq_cycle_mode && !(data & 1))
                    s.force_clock = 1;

                s.irq_cycle_mode = (uint8_t)(data & 1);
                if (s.irq_cycle_mode)
                    s.next_cpu_clock = (int32_t)time + 4;

                s.need_reload = 1;
                irq_changed();
                break;

            case 0xe000:
                s.irq_enabled = 0;
                s.irq_pending = 0;
                s.irq_due = -1;
                irq_changed();
                break;

            case 0xe001:
                s.irq_enabled = 1;
                irq_changed();
                break;
        }
    }
};

typedef Mapper_Rambo1<false> Mapper064;
typedef Mapper_Rambo1<true>  Mapper158;
