#pragma once
#include "Nes_Mapper.h"
#include <string.h>

template <bool Jf19>
class Mapper_Jaleco72_92 : public Nes_Mapper
{
    struct state_t
    {
        uint8_t prg_bank;
        uint8_t chr_bank;
        uint8_t prg_flag;
        uint8_t chr_flag;
    } s;

    void update()
    {
        if (Jf19)
        {
            set_prg_bank(0x8000, bank_16k, 0);
            set_prg_bank(0xc000, bank_16k, s.prg_bank);
        }
        else
        {
            set_prg_bank(0x8000, bank_16k, s.prg_bank);
            set_prg_bank(0xc000, bank_16k, last_bank);
        }

        set_chr_bank(0, bank_8k, s.chr_bank);
    }

public:
    Mapper_Jaleco72_92()
    {
        register_state(&s, sizeof s);
    }

    void reset_state()
    {
        memset(&s, 0, sizeof s);
    }

    void apply_mapping()
    {
        update();
    }

    void write(nes_time_t, nes_addr_t, int data)
    {
        if (!s.prg_flag && (data & 0x80))
            s.prg_bank = (uint8_t)(data & (Jf19 ? 0x0f : 0x07));

        if (!s.chr_flag && (data & 0x40))
            s.chr_bank = (uint8_t)(data & 0x0f);

        s.prg_flag = (uint8_t)((data & 0x80) != 0);
        s.chr_flag = (uint8_t)((data & 0x40) != 0);

        update();
    }
};

typedef Mapper_Jaleco72_92<false> Mapper072;
typedef Mapper_Jaleco72_92<true>  Mapper092;
