#pragma once
#include "Nes_Mapper.h"
#include <string.h>

class Mapper027 : public Nes_Mapper
{
    struct state_t
    {
        uint8_t prg[2];
        uint8_t chr[8];
        uint8_t mirror;
        uint8_t prg_swap;
        uint8_t irq_latch;
        uint8_t irq_control;
        uint8_t irq_pending;
        uint8_t reserved;
        int32_t next_time;
    } s;

    enum { timer_period = 113 * 4 + 3 };

    void update_prg()
    {
        if (s.prg_swap & 2)
        {
            set_prg_bank(0x8000, bank_8k, last_bank - 1);
            set_prg_bank(0xc000, bank_8k, s.prg[0]);
        }
        else
        {
            set_prg_bank(0x8000, bank_8k, s.prg[0]);
            set_prg_bank(0xc000, bank_8k, last_bank - 1);
        }

        set_prg_bank(0xa000, bank_8k, s.prg[1]);
        set_prg_bank(0xe000, bank_8k, last_bank);
    }

    void update_chr()
    {
        for (int i = 0; i < 8; ++i)
            set_chr_bank(i * 0x400, bank_1k, s.chr[i]);
    }

    void update_mirror()
    {
        switch (s.mirror & 3)
        {
            case 0: mirror_vert(); break;
            case 1: mirror_horiz(); break;
            case 2: mirror_single(0); break;
            case 3: mirror_single(1); break;
        }
    }

    void reset_timer(nes_time_t present)
    {
        s.next_time =
            (int32_t)(present +
                (unsigned)((0x100 - s.irq_latch) * timer_period) / 4);
    }

    void write_irq(nes_time_t time, nes_addr_t addr, int data)
    {
        run_until(time);
        switch (addr & 3)
        {
            case 0:
                s.irq_latch =
                    (uint8_t)((s.irq_latch & 0xf0) | (data & 0x0f));
                break;
            case 1:
                s.irq_latch =
                    (uint8_t)((s.irq_latch & 0x0f) | ((data & 0x0f) << 4));
                break;
            case 2:
                s.irq_pending = 0;
                s.irq_control = (uint8_t)(data & 3);
                if (data & 2)
                    reset_timer(time);
                irq_changed();
                break;
            case 3:
                s.irq_pending = 0;
                s.irq_control =
                    (uint8_t)((s.irq_control & ~2) |
                              ((s.irq_control << 1) & 2));
                irq_changed();
                break;
        }
    }

public:
    Mapper027()
    {
        register_state(&s, sizeof s);
    }

    void reset_state()
    {
        memset(&s, 0, sizeof s);
    }

    void apply_mapping()
    {
        enable_sram();
        update_prg();
        update_chr();
        update_mirror();
    }

    void run_until(nes_time_t end_time)
    {
        if (s.irq_control & 2)
        {
            while (s.next_time < end_time)
            {
                s.irq_pending = 1;
                reset_timer(s.next_time);
            }
        }
    }

    void end_frame(nes_time_t end_time)
    {
        run_until(end_time);
        s.next_time -= (int32_t)end_time;
    }

    nes_time_t next_irq(nes_time_t present)
    {
        if (s.irq_pending)
            return present;
        if (s.irq_control & 2)
            return (nes_time_t)s.next_time + 1;
        return no_irq;
    }

    void write(nes_time_t time, nes_addr_t addr, int data)
    {
        addr = (addr & 0xf000) | (addr & 3);

        if (addr >= 0xb000 && addr <= 0xe003)
        {
            unsigned bank =
                ((addr >> 1) & 1) | ((addr - 0xb000) >> 11);
            unsigned shift = (addr & 1) << 2;
            s.chr[bank] &= (uint8_t)(0xf0 >> shift);
            s.chr[bank] |= (uint8_t)((data & 0x0f) << shift);
            update_chr();
            return;
        }

        switch (addr & 0xf003)
        {
            case 0x8000:
            case 0x8001:
            case 0x8002:
            case 0x8003:
                s.prg[0] = (uint8_t)(data & 0x1f);
                update_prg();
                break;

            case 0xa000:
            case 0xa001:
            case 0xa002:
            case 0xa003:
                s.prg[1] = (uint8_t)(data & 0x1f);
                update_prg();
                break;

            case 0x9000:
            case 0x9001:
                s.mirror = (uint8_t)data;
                update_mirror();
                break;

            case 0x9002:
            case 0x9003:
                s.prg_swap = (uint8_t)data;
                update_prg();
                break;

            case 0xf000:
            case 0xf001:
            case 0xf002:
            case 0xf003:
                write_irq(time, addr, data);
                break;
        }
    }
};
