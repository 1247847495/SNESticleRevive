#pragma once
#include "Nes_Mapper.h"
#include <string.h>

class Mapper018 : public Nes_Mapper
{
    struct state_t
    {
        uint8_t prg[3];
        uint8_t chr[8];
        uint8_t irq_reload_nibble[4];
        uint8_t irq_size;
        uint8_t irq_enabled;
        uint8_t irq_pending;
        uint8_t mirror;
        uint16_t irq_counter;
        int32_t last_time;
    } s;

    uint16_t irq_mask() const
    {
        static const uint16_t masks[4] =
            { 0xffff, 0x0fff, 0x00ff, 0x000f };
        return masks[s.irq_size & 3];
    }

    void update_prg()
    {
        set_prg_bank(0x8000, bank_8k, s.prg[0]);
        set_prg_bank(0xa000, bank_8k, s.prg[1]);
        set_prg_bank(0xc000, bank_8k, s.prg[2]);
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
            case 0: mirror_horiz(); break;
            case 1: mirror_vert(); break;
            case 2: mirror_single(0); break;
            case 3: mirror_single(1); break;
        }
    }

    void update_nibble(uint8_t &v, int data, bool high)
    {
        if (high)
            v = (uint8_t)((v & 0x0f) | ((data & 0x0f) << 4));
        else
            v = (uint8_t)((v & 0xf0) | (data & 0x0f));
    }

    uint32_t cycles_to_irq() const
    {
        uint16_t mask = irq_mask();
        uint16_t c = s.irq_counter & mask;
        return c ? c : (uint32_t)mask + 1u;
    }

public:
    Mapper018()
    {
        register_state(&s, sizeof s);
    }

    void reset_state()
    {
        memset(&s, 0, sizeof s);
    }

    void apply_mapping()
    {
        s.last_time = 0;
        enable_sram();
        update_prg();
        update_chr();
        update_mirror();
    }

    void run_until(nes_time_t end_time)
    {
        if (end_time <= s.last_time)
            return;

        if (s.irq_enabled && !s.irq_pending)
        {
            uint32_t elapsed = (uint32_t)(end_time - s.last_time);
            uint32_t hit = cycles_to_irq();
            uint16_t mask = irq_mask();

            if (elapsed >= hit)
            {
                s.irq_counter &= (uint16_t)~mask;
                s.irq_pending = 1;
            }
            else
            {
                uint16_t c = s.irq_counter & mask;
                c = (uint16_t)((c - elapsed) & mask);
                s.irq_counter =
                    (uint16_t)((s.irq_counter & ~mask) | c);
            }
        }

        s.last_time = (int32_t)end_time;
    }

    void end_frame(nes_time_t end_time)
    {
        run_until(end_time);
        s.last_time -= (int32_t)end_time;
    }

    nes_time_t next_irq(nes_time_t present)
    {
        run_until(present);
        if (s.irq_pending)
            return present;
        if (!s.irq_enabled)
            return no_irq;
        return (nes_time_t)s.last_time + cycles_to_irq();
    }

    void write(nes_time_t time, nes_addr_t addr, int data)
    {
        run_until(time);
        data &= 0x0f;
        bool high = (addr & 1) != 0;

        switch (addr & 0xf003)
        {
            case 0x8000:
            case 0x8001:
                update_nibble(s.prg[0], data, high);
                update_prg();
                break;

            case 0x8002:
            case 0x8003:
                update_nibble(s.prg[1], data, high);
                update_prg();
                break;

            case 0x9000:
            case 0x9001:
                update_nibble(s.prg[2], data, high);
                update_prg();
                break;

            case 0xa000:
            case 0xa001:
                update_nibble(s.chr[0], data, high); update_chr(); break;
            case 0xa002:
            case 0xa003:
                update_nibble(s.chr[1], data, high); update_chr(); break;
            case 0xb000:
            case 0xb001:
                update_nibble(s.chr[2], data, high); update_chr(); break;
            case 0xb002:
            case 0xb003:
                update_nibble(s.chr[3], data, high); update_chr(); break;
            case 0xc000:
            case 0xc001:
                update_nibble(s.chr[4], data, high); update_chr(); break;
            case 0xc002:
            case 0xc003:
                update_nibble(s.chr[5], data, high); update_chr(); break;
            case 0xd000:
            case 0xd001:
                update_nibble(s.chr[6], data, high); update_chr(); break;
            case 0xd002:
            case 0xd003:
                update_nibble(s.chr[7], data, high); update_chr(); break;

            case 0xe000:
            case 0xe001:
            case 0xe002:
            case 0xe003:
                s.irq_reload_nibble[addr & 3] = (uint8_t)data;
                break;

            case 0xf000:
                s.irq_pending = 0;
                s.irq_counter =
                    (uint16_t)(
                        s.irq_reload_nibble[0] |
                        (s.irq_reload_nibble[1] << 4) |
                        (s.irq_reload_nibble[2] << 8) |
                        (s.irq_reload_nibble[3] << 12));
                irq_changed();
                break;

            case 0xf001:
                s.irq_pending = 0;
                s.irq_enabled = (uint8_t)(data & 1);
                if (data & 8) s.irq_size = 3;
                else if (data & 4) s.irq_size = 2;
                else if (data & 2) s.irq_size = 1;
                else s.irq_size = 0;
                irq_changed();
                break;

            case 0xf002:
                s.mirror = (uint8_t)(data & 3);
                update_mirror();
                break;

            case 0xf003:
                break;
        }
    }
};
