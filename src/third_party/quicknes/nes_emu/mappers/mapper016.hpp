#pragma once
#include "Nes_Mapper.h"
#include <string.h>

/*
 * Aurora add-only Bandai FCG/LZ93D50.
 *
 * Mapper 16:
 *   - CHR/PRG banking, mirroring and cycle IRQ
 *   - submapper 4/5 register-window distinction
 *   - serial EEPROM intentionally deferred in this add-only batch
 *
 * Mapper 153:
 *   - Famicom Jump II outer/inner PRG banking
 *   - unbanked 8 KiB CHR-RAM
 *   - 8 KiB WRAM enable
 *   - LZ93D50 latched cycle IRQ
 */
template <bool Is153>
class Mapper_Bandai_AddOnly : public Nes_Mapper
{
    struct state_t
    {
        uint8_t chr[8];
        uint8_t prg;
        uint8_t outer_prg;
        uint8_t mirror;
        uint8_t irq_enabled;
        uint8_t irq_pending;
        uint8_t ram_enabled;
        uint8_t reserved;
        uint16_t irq_counter;
        uint16_t irq_latch;
        int32_t last_time;
    } s;

    bool fcg12() const
    {
        return !Is153 && cart().submapper_code() == 4;
    }

    bool accepts(nes_addr_t addr) const
    {
        if (Is153)
            return addr >= 0x8000;

        int sub = cart().submapper_code();
        if (sub == 4)
            return addr >= 0x6000 && addr < 0x8000;
        if (sub == 5)
            return addr >= 0x8000;

        /* Legacy/unspecified mapper 16: accept both historical windows. */
        return addr >= 0x6000;
    }

    void update_prg()
    {
        int outer = Is153 ? (s.outer_prg ? 0x10 : 0) : 0;
        set_prg_bank(0x8000, bank_16k, (s.prg & 0x0f) | outer);
        set_prg_bank(0xc000, bank_16k, 0x0f | outer);
    }

    void update_chr()
    {
        if (Is153)
        {
            /* Mapper 153 has unbanked CHR-RAM. */
            set_chr_bank(0, bank_8k, 0);
            return;
        }

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

    void write_reg(nes_time_t time, nes_addr_t addr, int data)
    {
        if (!accepts(addr))
            return;

        run_until(time);

        int reg = addr & 0x0f;
        data &= 0xff;

        if (reg < 8)
        {
            if (Is153)
            {
                /* PA12/PA13 are grounded: only $8000-$8003 matter. */
                if (reg < 4)
                {
                    s.outer_prg = (uint8_t)(data & 1);
                    update_prg();
                }
            }
            else
            {
                s.chr[reg] = (uint8_t)data;
                set_chr_bank(reg * 0x400, bank_1k, s.chr[reg]);
            }
            return;
        }

        switch (reg)
        {
            case 0x08:
                s.prg = (uint8_t)(data & 0x0f);
                update_prg();
                break;

            case 0x09:
                s.mirror = (uint8_t)data;
                update_mirror();
                break;

            case 0x0a:
                s.irq_pending = 0;
                s.irq_enabled = (uint8_t)(data & 1);

                /* LZ93D50 reloads from latch; FCG-1/2 does not. */
                if (!fcg12())
                    s.irq_counter = s.irq_latch;

                irq_changed();
                break;

            case 0x0b:
                if (fcg12())
                    s.irq_counter =
                        (uint16_t)((s.irq_counter & 0xff00) | data);
                else
                    s.irq_latch =
                        (uint16_t)((s.irq_latch & 0xff00) | data);
                break;

            case 0x0c:
                if (fcg12())
                    s.irq_counter =
                        (uint16_t)((s.irq_counter & 0x00ff) | (data << 8));
                else
                    s.irq_latch =
                        (uint16_t)((s.irq_latch & 0x00ff) | (data << 8));
                break;

            case 0x0d:
                if (Is153)
                {
                    s.ram_enabled = (uint8_t)((data & 0x20) != 0);
                    enable_sram(s.ram_enabled != 0);
                }
                /* Mapper 16 serial EEPROM is intentionally deferred. */
                break;
        }
    }

public:
    Mapper_Bandai_AddOnly()
    {
        register_state(&s, sizeof s);
    }

    void reset_state()
    {
        memset(&s, 0, sizeof s);
        s.last_time = 0;

        if (!Is153 && cart().submapper_code() != 5)
            intercept_writes(0x6000, 0x2000);
    }

    void apply_mapping()
    {
        s.last_time = 0;

        if (!Is153 && cart().submapper_code() != 5)
            intercept_writes(0x6000, 0x2000);

        update_prg();
        update_chr();
        update_mirror();

        if (Is153)
            enable_sram(s.ram_enabled != 0);
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
                s.irq_counter = 0xffff;
                s.irq_pending = 1;
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

    bool write_intercepted(
        nes_time_t time, nes_addr_t addr, int data)
    {
        if (!accepts(addr))
            return false;

        write_reg(time, addr, data);
        return true;
    }

    void write(nes_time_t time, nes_addr_t addr, int data)
    {
        write_reg(time, addr, data);
    }
};

typedef Mapper_Bandai_AddOnly<false> Mapper016;
typedef Mapper_Bandai_AddOnly<true>  Mapper153;
