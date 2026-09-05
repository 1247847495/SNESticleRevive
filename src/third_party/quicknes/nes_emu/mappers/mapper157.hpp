#pragma once
#include "Nes_Mapper.h"
#include "mapper_bandai_eeprom.hpp"
#include <string.h>

class Mapper157 : public Nes_Mapper
{
    struct state_t
    {
        uint8_t chr_regs[8];
        uint8_t prg;
        uint8_t prg_high;
        uint8_t mirror;
        uint8_t irq_enabled;
        uint8_t irq_pending;
        uint16_t irq_counter;
        uint16_t irq_reload;
        int32_t last_time;
        AuroraEepromState internal_24c02;
        AuroraEepromState external_24c01;
    } s;

    void update_prg_high()
    {
        uint8_t high = 0;

        if (cart().prg_size() >= 0x80000)
        {
            for (int i = 0; i < 8; ++i)
                high |= (s.chr_regs[i] & 1) ? 0x10 : 0;
        }

        s.prg_high = high;
    }

    void update()
    {
        update_prg_high();

        set_prg_bank(0x8000, bank_16k,
                     (s.prg & 0x0f) | s.prg_high);
        set_prg_bank(0xc000, bank_16k,
                     0x0f | s.prg_high);

        switch (s.mirror & 3)
        {
            case 0: mirror_vert(); break;
            case 1: mirror_horiz(); break;
            case 2: mirror_single(0); break;
            case 3: mirror_single(1); break;
        }
    }

public:
    Mapper157()
    {
        register_state(&s, sizeof s);
    }

    void reset_state()
    {
        memset(&s, 0, sizeof s);
        AuroraEepromReset(s.internal_24c02);
        AuroraEepromReset(s.external_24c01);
        intercept_reads(0x6000, 0x2000);
    }

    void apply_mapping()
    {
        s.last_time = 0;
        intercept_reads(0x6000, 0x2000);
        update();
    }

    void run_until(nes_time_t end_time)
    {
        if (end_time <= s.last_time)
            return;

        if (s.irq_enabled && !s.irq_pending)
        {
            uint32_t elapsed =
                (uint32_t)(end_time - s.last_time);
            uint32_t until_irq =
                (uint32_t)s.irq_counter + 1u;

            if (elapsed >= until_irq)
            {
                s.irq_pending = 1;
                s.irq_counter = 0xffff;
            }
            else
            {
                s.irq_counter =
                    (uint16_t)(s.irq_counter - elapsed);
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
               (nes_time_t)s.irq_counter + 1;
    }

    int read(nes_time_t time, nes_addr_t addr)
    {
        if (addr >= 0x6000 && addr <= 0x7fff)
        {
            /* No barcode reader connected: barcode contribution is 0.
             * Serial EEPROM data remains fully emulated. */
            int ee =
                (s.internal_24c02.output &&
                 s.external_24c01.output) ? 0x10 : 0;

            return ((addr >> 8) & 0xe7) | ee;
        }

        return Nes_Mapper::read(time, addr);
    }

    void write(nes_time_t time, nes_addr_t addr, int data)
    {
        run_until(time);

        int reg = addr & 0x0f;
        uint8_t* ram = sram_data_ptr();

        if (reg < 8)
        {
            s.chr_regs[reg] = (uint8_t)data;

            /* Datach external 24C01 gets SCL from regs $8000-$8003,
             * bit 3. */
            if (reg <= 3)
            {
                AuroraEeprom24C01WriteScl(
                    s.external_24c01, ram, 0x100,
                    (uint8_t)((data >> 3) & 1));
            }

            update();
            return;
        }

        switch (reg)
        {
            case 0x08:
                s.prg = (uint8_t)(data & 0x0f);
                update();
                break;

            case 0x09:
                s.mirror = (uint8_t)data;
                update();
                break;

            case 0x0a:
                s.irq_pending = 0;
                s.irq_enabled = (uint8_t)(data & 1);
                s.irq_counter = s.irq_reload;
                irq_changed();
                break;

            case 0x0b:
                s.irq_reload =
                    (uint16_t)((s.irq_reload & 0xff00) | data);
                break;

            case 0x0c:
                s.irq_reload =
                    (uint16_t)((s.irq_reload & 0x00ff) |
                               (data << 8));
                break;

            case 0x0d:
            {
                uint8_t scl = (uint8_t)((data >> 5) & 1);
                uint8_t sda = (uint8_t)((data >> 6) & 1);

                AuroraEeprom24C02Write(
                    s.internal_24c02, ram, 0x000, scl, sda);

                AuroraEeprom24C01WriteSda(
                    s.external_24c01, ram, 0x100, sda);
                break;
            }
        }
    }
};
