#pragma once
#include "mapper004.hpp"

class Mapper118 : public Mapper004
{
    void update_tx_mirroring()
    {
        if (!(mode & 0x80))
        {
            int a = (banks[0] >> 7) & 1;
            int b = (banks[1] >> 7) & 1;
            mirror_manual(a, a, b, b);
        }
        else
        {
            mirror_manual(
                (banks[2] >> 7) & 1,
                (banks[3] >> 7) & 1,
                (banks[4] >> 7) & 1,
                (banks[5] >> 7) & 1);
        }
    }

public:
    void apply_mapping()
    {
        Mapper004::apply_mapping();
        update_tx_mirroring();
    }

    void write(nes_time_t time, nes_addr_t addr, int data)
    {
        int reg = addr & 0xe001;

        if (reg == 0xa000)
        {
            mirror = (uint8_t)data;
            return;
        }

        Mapper004::write(time, addr, data);

        if (reg == 0x8000 || reg == 0x8001)
            update_tx_mirroring();
    }
};
