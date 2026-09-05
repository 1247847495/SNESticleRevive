#pragma once
#include <stdint.h>

enum AuroraEepromMode
{
    AEE_IDLE = 0,
    AEE_ADDRESS,
    AEE_READ,
    AEE_WRITE,
    AEE_SEND_ACK,
    AEE_WAIT_ACK,
    AEE_CHIP_ADDRESS
};

struct AuroraEepromState
{
    uint8_t mode;
    uint8_t next_mode;
    uint8_t chip_address;
    uint8_t address;
    uint8_t data;
    uint8_t counter;
    uint8_t output;
    uint8_t prev_scl;
    uint8_t prev_sda;
};

inline void AuroraEepromReset(AuroraEepromState &s)
{
    s.mode = AEE_IDLE;
    s.next_mode = AEE_IDLE;
    s.chip_address = 0;
    s.address = 0;
    s.data = 0;
    s.counter = 0;
    s.output = 1;
    s.prev_scl = 0;
    s.prev_sda = 0;
}

inline void AuroraEeprom24C01WriteBit(
    AuroraEepromState &s, uint8_t &dest, uint8_t value)
{
    if (s.counter < 8)
    {
        uint8_t mask = (uint8_t)~(1u << s.counter);
        dest = (uint8_t)((dest & mask) |
                         ((value & 1) << s.counter));
        s.counter++;
    }
}

inline void AuroraEeprom24C01ReadBit(
    AuroraEepromState &s, uint8_t const* mem, int base)
{
    if (s.counter < 8)
    {
        uint8_t value = mem[base + (s.address & 0x7f)];
        s.output = (value & (1u << s.counter)) ? 1 : 0;
        s.counter++;
    }
}

inline void AuroraEeprom24C01Write(
    AuroraEepromState &s, uint8_t* mem, int base,
    uint8_t scl, uint8_t sda)
{
    if (s.prev_scl && scl && sda < s.prev_sda)
    {
        s.mode = AEE_ADDRESS;
        s.address = 0;
        s.counter = 0;
        s.output = 1;
    }
    else if (s.prev_scl && scl && sda > s.prev_sda)
    {
        s.mode = AEE_IDLE;
        s.output = 1;
    }
    else if (scl > s.prev_scl)
    {
        switch (s.mode)
        {
            case AEE_ADDRESS:
                if (s.counter < 7)
                    AuroraEeprom24C01WriteBit(
                        s, s.address, sda);
                else if (s.counter == 7)
                {
                    s.counter = 8;
                    if (sda)
                    {
                        s.next_mode = AEE_READ;
                        s.data = mem[base + (s.address & 0x7f)];
                    }
                    else
                    {
                        s.next_mode = AEE_WRITE;
                    }
                }
                break;

            case AEE_SEND_ACK:
                s.output = 0;
                break;

            case AEE_READ:
                AuroraEeprom24C01ReadBit(s, mem, base);
                break;

            case AEE_WRITE:
                AuroraEeprom24C01WriteBit(s, s.data, sda);
                break;

            case AEE_WAIT_ACK:
                if (!sda)
                    s.next_mode = AEE_IDLE;
                break;

            default:
                break;
        }
    }
    else if (scl < s.prev_scl)
    {
        switch (s.mode)
        {
            case AEE_ADDRESS:
                if (s.counter == 8)
                {
                    s.mode = AEE_SEND_ACK;
                    s.output = 1;
                }
                break;

            case AEE_SEND_ACK:
                s.mode = s.next_mode;
                s.counter = 0;
                s.output = 1;
                break;

            case AEE_READ:
                if (s.counter == 8)
                {
                    s.mode = AEE_WAIT_ACK;
                    s.address = (uint8_t)((s.address + 1) & 0x7f);
                }
                break;

            case AEE_WRITE:
                if (s.counter == 8)
                {
                    s.mode = AEE_SEND_ACK;
                    s.next_mode = AEE_IDLE;
                    mem[base + (s.address & 0x7f)] = s.data;
                    s.address = (uint8_t)((s.address + 1) & 0x7f);
                }
                break;

            default:
                break;
        }
    }

    s.prev_scl = scl;
    s.prev_sda = sda;
}

inline void AuroraEeprom24C01WriteScl(
    AuroraEepromState &s, uint8_t* mem, int base, uint8_t scl)
{
    AuroraEeprom24C01Write(s, mem, base, scl, s.prev_sda);
}

inline void AuroraEeprom24C01WriteSda(
    AuroraEepromState &s, uint8_t* mem, int base, uint8_t sda)
{
    AuroraEeprom24C01Write(s, mem, base, s.prev_scl, sda);
}

inline void AuroraEeprom24C02WriteBit(
    AuroraEepromState &s, uint8_t &dest, uint8_t value)
{
    if (s.counter < 8)
    {
        uint8_t mask =
            (uint8_t)~(1u << (7 - s.counter));
        dest = (uint8_t)((dest & mask) |
                         ((value & 1) << (7 - s.counter)));
        s.counter++;
    }
}

inline void AuroraEeprom24C02ReadBit(
    AuroraEepromState &s, uint8_t const* mem, int base)
{
    if (s.counter < 8)
    {
        uint8_t value = mem[base + s.address];
        s.output =
            (value & (1u << (7 - s.counter))) ? 1 : 0;
        s.counter++;
    }
}

inline void AuroraEeprom24C02Write(
    AuroraEepromState &s, uint8_t* mem, int base,
    uint8_t scl, uint8_t sda)
{
    if (s.prev_scl && scl && sda < s.prev_sda)
    {
        s.mode = AEE_CHIP_ADDRESS;
        s.counter = 0;
        s.output = 1;
    }
    else if (s.prev_scl && scl && sda > s.prev_sda)
    {
        s.mode = AEE_IDLE;
        s.output = 1;
    }
    else if (scl > s.prev_scl)
    {
        switch (s.mode)
        {
            case AEE_CHIP_ADDRESS:
                AuroraEeprom24C02WriteBit(
                    s, s.chip_address, sda);
                break;

            case AEE_ADDRESS:
                AuroraEeprom24C02WriteBit(
                    s, s.address, sda);
                break;

            case AEE_READ:
                AuroraEeprom24C02ReadBit(s, mem, base);
                break;

            case AEE_WRITE:
                AuroraEeprom24C02WriteBit(
                    s, s.data, sda);
                break;

            case AEE_SEND_ACK:
                s.output = 0;
                break;

            case AEE_WAIT_ACK:
                if (!sda)
                {
                    s.next_mode = AEE_READ;
                    s.data = mem[base + s.address];
                }
                break;

            default:
                break;
        }
    }
    else if (scl < s.prev_scl)
    {
        switch (s.mode)
        {
            case AEE_CHIP_ADDRESS:
                if (s.counter == 8)
                {
                    if ((s.chip_address & 0xa0) == 0xa0)
                    {
                        s.mode = AEE_SEND_ACK;
                        s.counter = 0;
                        s.output = 1;

                        if (s.chip_address & 1)
                        {
                            s.next_mode = AEE_READ;
                            s.data = mem[base + s.address];
                        }
                        else
                        {
                            s.next_mode = AEE_ADDRESS;
                        }
                    }
                    else
                    {
                        s.mode = AEE_IDLE;
                        s.counter = 0;
                        s.output = 1;
                    }
                }
                break;

            case AEE_ADDRESS:
                if (s.counter == 8)
                {
                    s.counter = 0;
                    s.mode = AEE_SEND_ACK;
                    s.next_mode = AEE_WRITE;
                    s.output = 1;
                }
                break;

            case AEE_READ:
                if (s.counter == 8)
                {
                    s.mode = AEE_WAIT_ACK;
                    s.address++;
                }
                break;

            case AEE_WRITE:
                if (s.counter == 8)
                {
                    s.counter = 0;
                    s.mode = AEE_SEND_ACK;
                    s.next_mode = AEE_WRITE;
                    mem[base + s.address] = s.data;
                    s.address++;
                }
                break;

            case AEE_SEND_ACK:
            case AEE_WAIT_ACK:
                s.mode = s.next_mode;
                s.counter = 0;
                s.output = 1;
                break;

            default:
                break;
        }
    }

    s.prev_scl = scl;
    s.prev_sda = sda;
}

inline void AuroraEeprom24C02WriteScl(
    AuroraEepromState &s, uint8_t* mem, int base, uint8_t scl)
{
    AuroraEeprom24C02Write(s, mem, base, scl, s.prev_sda);
}

inline void AuroraEeprom24C02WriteSda(
    AuroraEepromState &s, uint8_t* mem, int base, uint8_t sda)
{
    AuroraEeprom24C02Write(s, mem, base, s.prev_scl, sda);
}
