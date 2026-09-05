#pragma once
#include "Nes_Mapper.h"
#include <string.h>

class Mapper065 : public Nes_Mapper
{
    struct state_t
    {
        uint8_t prg[3];
        uint8_t chr[8];
        uint8_t mirror;
        uint8_t irq_enabled;
        uint8_t irq_pending;
        uint8_t reserved;
        uint16_t irq_counter;
        uint16_t irq_reload;
        int32_t last_time;
    } s;

    void update_banks()
    {
        set_prg_bank(0x8000, bank_8k, s.prg[0]);
        set_prg_bank(0xa000, bank_8k, s.prg[1]);
        set_prg_bank(0xc000, bank_8k, s.prg[2]);
        set_prg_bank(0xe000, bank_8k, last_bank);

        for (int i = 0; i < 8; ++i)
            set_chr_bank(i * 0x400, bank_1k, s.chr[i]);

        if (s.mirror)
            mirror_horiz();
        else
            mirror_vert();
    }

public:
    Mapper065()
    {
        register_state(&s, sizeof s);
    }

    void reset_state()
    {
        memset(&s, 0, sizeof s);
        s.prg[0] = 0;
        s.prg[1] = 1;
        s.prg[2] = 0xfe;
    }

    void apply_mapping()
    {
        s.last_time = 0;
        enable_sram();
        update_banks();
    }

    void run_until(nes_time_t end_time)
    {
        if (end_time <= s.last_time)
            return;

        if (s.irq_enabled && !s.irq_pending)
        {
            uint32_t elapsed = (uint32_t)(end_time - s.last_time);
            if (s.irq_counter && elapsed >= s.irq_counter)
            {
                s.irq_counter = 0;
                s.irq_enabled = 0;
                s.irq_pending = 1;
            }
            else if (s.irq_counter)
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
        if (!s.irq_enabled || !s.irq_counter)
            return no_irq;
        return (nes_time_t)s.last_time + s.irq_counter;
    }

    void write(nes_time_t time, nes_addr_t addr, int data)
    {
        run_until(time);

        switch (addr)
        {
            case 0x8000:
                s.prg[0] = (uint8_t)data; update_banks(); break;
            case 0x9001:
                s.mirror = (uint8_t)((data & 0x80) != 0);
                update_banks();
                break;
            case 0x9003:
                s.irq_enabled = (uint8_t)((data & 0x80) != 0);
                s.irq_pending = 0;
                irq_changed();
                break;
            case 0x9004:
                s.irq_counter = s.irq_reload;
                s.irq_pending = 0;
                irq_changed();
                break;
            case 0x9005:
                s.irq_reload =
                    (uint16_t)((s.irq_reload & 0x00ff) | (data << 8));
                break;
            case 0x9006:
                s.irq_reload =
                    (uint16_t)((s.irq_reload & 0xff00) | data);
                break;
            case 0xa000:
                s.prg[1] = (uint8_t)data; update_banks(); break;
            case 0xb000:
            case 0xb001:
            case 0xb002:
            case 0xb003:
            case 0xb004:
            case 0xb005:
            case 0xb006:
            case 0xb007:
                s.chr[addr & 7] = (uint8_t)data;
                update_banks();
                break;
            case 0xc000:
                s.prg[2] = (uint8_t)data; update_banks(); break;
        }
    }
};
