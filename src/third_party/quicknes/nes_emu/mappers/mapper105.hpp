#pragma once
#include "Nes_Mapper.h"
#include <string.h>

class Mapper105 : public Nes_Mapper
{
    struct state_t
    {
        uint8_t regs[4];
        uint8_t bit;
        uint8_t buf;
        uint8_t init_state;
        uint8_t irq_enabled;
        uint8_t irq_pending;
        uint32_t irq_counter;
        int32_t last_time;
    } s;

    uint32_t max_counter() const
    {
        /* No frontend DIP selector yet: DIP 0 is the deterministic default. */
        return 0x20000000u;
    }

    void update_mirror()
    {
        int mode = s.regs[0] & 3;
        if (mode < 2)
            mirror_single(mode & 1);
        else if (mode == 2)
            mirror_vert();
        else
            mirror_horiz();
    }

    void update_state()
    {
        uint8_t chr0 = s.regs[1];
        uint8_t prg = s.regs[3];

        if (s.init_state == 0 && !(chr0 & 0x10))
            s.init_state = 1;
        else if (s.init_state == 1 && (chr0 & 0x10))
            s.init_state = 2;

        if (chr0 & 0x10)
        {
            s.irq_enabled = 0;
            s.irq_pending = 0;
            s.irq_counter = 0;
        }
        else
        {
            s.irq_enabled = 1;
        }

        enable_sram((prg & 0x10) == 0);
        update_mirror();

        if (s.init_state != 2)
        {
            set_prg_bank(0x8000, bank_32k, 0);
            return;
        }

        if (chr0 & 0x08)
        {
            int prg_reg = (prg & 0x07) | 0x08;
            bool prg_mode = (s.regs[0] & 0x08) != 0;
            bool slot_select = (s.regs[0] & 0x04) != 0;

            if (prg_mode)
            {
                if (slot_select)
                {
                    set_prg_bank(0x8000, bank_16k, prg_reg);
                    set_prg_bank(0xc000, bank_16k, 0x0f);
                }
                else
                {
                    set_prg_bank(0x8000, bank_16k, 0x08);
                    set_prg_bank(0xc000, bank_16k, prg_reg);
                }
            }
            else
            {
                set_prg_bank(0x8000, bank_32k, (prg_reg & 0x0e) >> 1);
            }
        }
        else
        {
            set_prg_bank(0x8000, bank_32k, (chr0 & 0x06) >> 1);
        }
    }

public:
    Mapper105()
    {
        register_state(&s, sizeof s);
    }

    void reset_state()
    {
        memset(&s, 0, sizeof s);
        s.regs[0] = 0x0f;
        s.regs[1] = 0x10;
    }

    void apply_mapping()
    {
        s.last_time = 0;
        update_state();
    }

    void run_until(nes_time_t end_time)
    {
        if (end_time <= s.last_time)
            return;

        if (s.irq_enabled && !s.irq_pending)
        {
            uint32_t elapsed = (uint32_t)(end_time - s.last_time);
            uint32_t remain = max_counter() - s.irq_counter;

            if (elapsed >= remain)
            {
                s.irq_counter = max_counter();
                s.irq_enabled = 0;
                s.irq_pending = 1;
            }
            else
            {
                s.irq_counter += elapsed;
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

        return (nes_time_t)s.last_time +
               (nes_time_t)(max_counter() - s.irq_counter);
    }

    void write(nes_time_t time, nes_addr_t addr, int data)
    {
        run_until(time);

        if (data & 0x80)
        {
            s.bit = 0;
            s.buf = 0;
            s.regs[0] |= 0x0c;
            update_state();
            irq_changed();
            return;
        }

        s.buf |= (uint8_t)((data & 1) << s.bit);
        s.bit++;

        if (s.bit >= 5)
        {
            int reg = (addr >> 13) & 3;
            s.regs[reg] = s.buf & 0x1f;
            s.bit = 0;
            s.buf = 0;
            update_state();
            irq_changed();
        }
    }
};
