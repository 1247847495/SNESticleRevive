#pragma once
#include "Nes_Mapper.h"
#include <string.h>

class Mapper067 : public Nes_Mapper
{
    struct state_t
    {
        uint8_t chr[4];
        uint8_t prg;
        uint8_t mirror;
        uint8_t irq_byte_phase;
        uint8_t irq_enabled;
        uint8_t irq_pending;
        uint16_t irq_counter;
        int32_t last_time;
    } s;

    void update_banks()
    {
        for (int i = 0; i < 4; ++i)
            set_chr_bank(i * 0x800, bank_2k, s.chr[i]);

        set_prg_bank(0x8000, bank_16k, s.prg);
        set_prg_bank(0xc000, bank_16k, last_bank);

        switch (s.mirror & 3)
        {
            case 0: mirror_vert(); break;
            case 1: mirror_horiz(); break;
            case 2: mirror_single(0); break;
            case 3: mirror_single(1); break;
        }
    }

public:
    Mapper067()
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
        update_banks();
    }

    void run_until(nes_time_t end_time)
    {
        if (end_time <= s.last_time)
            return;

        if (s.irq_enabled && !s.irq_pending)
        {
            uint32_t elapsed = (uint32_t)(end_time - s.last_time);
            uint32_t until_irq = (uint32_t)s.irq_counter + 1u;

            if (elapsed >= until_irq)
            {
                s.irq_pending = 1;
                s.irq_enabled = 0;
                s.irq_counter = 0xffff;
            }
            else
            {
                s.irq_counter = (uint16_t)(s.irq_counter - elapsed);
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
        return (nes_time_t)s.last_time + s.irq_counter + 1;
    }

    void write(nes_time_t time, nes_addr_t addr, int data)
    {
        run_until(time);

        switch (addr & 0xf800)
        {
            case 0x8800: s.chr[0] = (uint8_t)data; update_banks(); break;
            case 0x9800: s.chr[1] = (uint8_t)data; update_banks(); break;
            case 0xa800: s.chr[2] = (uint8_t)data; update_banks(); break;
            case 0xb800: s.chr[3] = (uint8_t)data; update_banks(); break;

            case 0xc800:
                if (!s.irq_byte_phase)
                    s.irq_counter =
                        (uint16_t)((s.irq_counter & 0x00ff) | (data << 8));
                else
                    s.irq_counter =
                        (uint16_t)((s.irq_counter & 0xff00) | data);
                s.irq_byte_phase ^= 1;
                break;

            case 0xd800:
                s.irq_enabled = (uint8_t)((data & 0x10) != 0);
                s.irq_pending = 0;
                s.irq_byte_phase = 0;
                irq_changed();
                break;

            case 0xe800:
                s.mirror = (uint8_t)(data & 3);
                update_banks();
                break;

            case 0xf800:
                s.prg = (uint8_t)data;
                update_banks();
                break;
        }
    }
};
