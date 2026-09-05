#pragma once
#include "Nes_Mapper.h"
#include <string.h>

class Mapper159 : public Nes_Mapper
{
    enum EepromMode
    {
        EE_IDLE = 0,
        EE_ADDRESS,
        EE_SEND_ACK,
        EE_READ,
        EE_WRITE,
        EE_WAIT_ACK
    };

    struct state_t
    {
        uint8_t chr[8];
        uint8_t prg;
        uint8_t prg_high;
        uint8_t mirror;
        uint8_t irq_enabled;
        uint8_t irq_pending;
        uint16_t irq_counter;
        uint16_t irq_reload;
        int32_t last_time;

        uint8_t ee_mode;
        uint8_t ee_next_mode;
        uint8_t ee_counter;
        uint8_t ee_output;
        uint8_t ee_prev_scl;
        uint8_t ee_prev_sda;
        uint8_t ee_address;
        uint8_t ee_data;
    } s;

    void update_banks()
    {
        uint8_t high = 0;
        if (cart().prg_size() >= 0x80000)
        {
            for (int i = 0; i < 8; ++i)
                high |= (s.chr[i] & 1) ? 0x10 : 0;
        }
        s.prg_high = high;

        set_prg_bank(0x8000, bank_16k, (s.prg & 0x0f) | high);
        set_prg_bank(0xc000, bank_16k, 0x0f | high);

        if (cart().chr_size() > 0)
            for (int i = 0; i < 8; ++i)
                set_chr_bank(i * 0x400, bank_1k, s.chr[i]);

        switch (s.mirror & 3)
        {
            case 0: mirror_vert(); break;
            case 1: mirror_horiz(); break;
            case 2: mirror_single(0); break;
            case 3: mirror_single(1); break;
        }
    }

    void ee_write_bit(uint8_t &dest, uint8_t value)
    {
        if (s.ee_counter < 8)
        {
            uint8_t mask = (uint8_t)~(1u << s.ee_counter);
            dest = (uint8_t)((dest & mask) |
                             ((value & 1) << s.ee_counter));
            s.ee_counter++;
        }
    }

    void ee_read_bit()
    {
        if (s.ee_counter < 8)
        {
            s.ee_output =
                (sram_read_byte((nes_addr_t)(0x6000 +
                    (s.ee_address & 0x7f))) &
                 (1u << s.ee_counter)) ? 1 : 0;
            s.ee_counter++;
        }
    }

    void eeprom_write(uint8_t scl, uint8_t sda)
    {
        if (s.ee_prev_scl && scl && sda < s.ee_prev_sda)
        {
            s.ee_mode = EE_ADDRESS;
            s.ee_address = 0;
            s.ee_counter = 0;
            s.ee_output = 1;
        }
        else if (s.ee_prev_scl && scl && sda > s.ee_prev_sda)
        {
            s.ee_mode = EE_IDLE;
            s.ee_output = 1;
        }
        else if (scl > s.ee_prev_scl)
        {
            switch (s.ee_mode)
            {
                case EE_ADDRESS:
                    if (s.ee_counter < 7)
                    {
                        ee_write_bit(s.ee_address, sda);
                    }
                    else if (s.ee_counter == 7)
                    {
                        s.ee_counter = 8;
                        if (sda)
                        {
                            s.ee_next_mode = EE_READ;
                            s.ee_data =
                                (uint8_t)sram_read_byte(
                                    (nes_addr_t)(0x6000 +
                                        (s.ee_address & 0x7f)));
                        }
                        else
                        {
                            s.ee_next_mode = EE_WRITE;
                        }
                    }
                    break;

                case EE_SEND_ACK:
                    s.ee_output = 0;
                    break;

                case EE_READ:
                    ee_read_bit();
                    break;

                case EE_WRITE:
                    ee_write_bit(s.ee_data, sda);
                    break;

                case EE_WAIT_ACK:
                    if (!sda)
                        s.ee_next_mode = EE_IDLE;
                    break;

                default:
                    break;
            }
        }
        else if (scl < s.ee_prev_scl)
        {
            switch (s.ee_mode)
            {
                case EE_ADDRESS:
                    if (s.ee_counter == 8)
                    {
                        s.ee_mode = EE_SEND_ACK;
                        s.ee_output = 1;
                    }
                    break;

                case EE_SEND_ACK:
                    s.ee_mode = s.ee_next_mode;
                    s.ee_counter = 0;
                    s.ee_output = 1;
                    break;

                case EE_READ:
                    if (s.ee_counter == 8)
                    {
                        s.ee_mode = EE_WAIT_ACK;
                        s.ee_address =
                            (uint8_t)((s.ee_address + 1) & 0x7f);
                    }
                    break;

                case EE_WRITE:
                    if (s.ee_counter == 8)
                    {
                        s.ee_mode = EE_SEND_ACK;
                        s.ee_next_mode = EE_IDLE;
                        sram_write_byte(
                            (nes_addr_t)(0x6000 +
                                (s.ee_address & 0x7f)),
                            s.ee_data);
                        s.ee_address =
                            (uint8_t)((s.ee_address + 1) & 0x7f);
                    }
                    break;

                default:
                    break;
            }
        }

        s.ee_prev_scl = scl;
        s.ee_prev_sda = sda;
    }

public:
    Mapper159()
    {
        register_state(&s, sizeof s);
    }

    void reset_state()
    {
        memset(&s, 0, sizeof s);
        s.ee_output = 1;
        s.last_time = 0;
        intercept_reads(0x6000, 0x2000);
    }

    void apply_mapping()
    {
        s.last_time = 0;
        intercept_reads(0x6000, 0x2000);
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

    int read(nes_time_t time, nes_addr_t addr)
    {
        if (addr >= 0x6000 && addr <= 0x7fff)
            return ((addr >> 8) & 0xe7) | (s.ee_output << 4);

        return Nes_Mapper::read(time, addr);
    }

    void write(nes_time_t time, nes_addr_t addr, int data)
    {
        run_until(time);

        int reg = addr & 0x0f;

        if (reg < 8)
        {
            s.chr[reg] = (uint8_t)data;
            update_banks();
            return;
        }

        switch (reg)
        {
            case 0x08:
                s.prg = (uint8_t)(data & 0x0f);
                update_banks();
                break;

            case 0x09:
                s.mirror = (uint8_t)data;
                update_banks();
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
                    (uint16_t)((s.irq_reload & 0x00ff) | (data << 8));
                break;

            case 0x0d:
                eeprom_write((uint8_t)((data >> 5) & 1),
                             (uint8_t)((data >> 6) & 1));
                break;
        }
    }
};
