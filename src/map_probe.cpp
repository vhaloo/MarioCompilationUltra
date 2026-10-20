#include "observer.h"
#include "snes9x.h"
#include "memmap.h"
#include "cpuexec.h"
#include "getset.h"
#include <libretro.h>
#include <limits>
#include <stdexcept>

namespace {
// Execute a RAM-only map decoder without advancing a video/audio frame. Its
// nested calls use the cartridge's own instructions through the SNES CPU.
bool decodeColumn(unsigned &budget, bool lostLevels) {
    unsigned bank = lostLevels ? 0x0d : 3;
    Registers.DB = uint8_t(bank);
    ICPU.ShiftedDB = bank << 16;
    Registers.D.W = 0;
    Registers.P.W = 0x34;
    Registers.A.W = Registers.X.W = Registers.Y.W = 0;
    Registers.S.W = 0x1fd;
    Memory.RAM[0x1ff] = 0xff;
    Memory.RAM[0x1fe] = 0xfe; // RTS returns to a sentinel, never executed.
    S9xUnpackStatus();
    S9xFixCycles();
    S9xSetPCBase(lostLevels ? 0x0da165 : 0x03a436);
    CPU.NextEvent = std::numeric_limits<int>::max();
    CPU.Cycles = 0;
    CPU.WaitingForInterrupt = false;
    for (unsigned count = 0; count < 100000 && budget; ++count, --budget) {
        if (Registers.PB == bank && Registers.PCw == 0xffff)
            return true;
        uint8_t op = S9xGetByte(Registers.PBPC);
        // A malformed cartridge must not leave this preparation waiting forever.
        if (op == 0xcb || op == 0xdb)
            return false;
        ++Registers.PCw;
        S9xOpcodesSlow[op].S9xOpcode();
    }
    return false;
}
} // namespace

bool mcuClassicColumns(std::vector<uint8_t> &out, bool lostLevels) {
    out.clear();
    size_t size = retro_serialize_size();
    if (!size || size > 4 * 1024 * 1024)
        return false;
    std::vector<uint8_t> checkpoint(size);
    if (!retro_serialize(checkpoint.data(), size))
        return false;
    bool complete = false;
    try {
        auto *r = Memory.RAM;
        unsigned ptr = r[0xfa] | r[0xfb] << 8 | r[0xfc] << 16;
        if ((ptr & 0xffff) < 0x8002)
            throw std::runtime_error("Invalid column stream");
        int first = S9xGetByte(ptr - 2), second = S9xGetByte(ptr - 1);
        // Initialize the independent parser cursor from the area's header. The
        // live parser can already be several screens ahead when play begins.
        r[0x741] = (first & 7) < 4 ? first & 7 : 0;
        r[0x744] = (first & 7) >= 4 ? first & (lostLevels ? 7 : 4) : 0;
        r[0x727] = second & 15;
        int scenery = (second >> 4) & 3;
        r[0x742] = !lostLevels && scenery == 1 ? 0 : scenery;
        r[0x715] = first >> 6;
        r[0x733] = (second >> 6) == 3 ? 0 : second >> 6;
        r[0x743] = (second >> 6) == 3 ? 3 : 0;
        for (int p :
             {0x725, 0x726, 0x728, 0x729, 0x72a, 0x72b, 0x72c, 0x72d, 0x6a0, 0xec9, 0x724, 0x734})
            r[p] = 0;
        for (int i = 0; i < 5; ++i)
            r[0x1300 + i] = 0xff;
        unsigned budget = 4000000;
        int end = -1;
        for (int column = 0; column < 512; ++column) {
            r[0x725] = uint8_t(column / 16);
            r[0x726] = uint8_t(column % 16);
            r[0x6a0] = uint8_t(column % 32);
            if (!decodeColumn(budget, lostLevels))
                break;
            out.insert(out.end(), r + 0x6a1, r + 0x6ae);
            unsigned index = r[0x72c] | r[0x72d] << 8;
            bool active = false;
            for (int i = 0; i < 5; ++i)
                active |= r[0x1300 + i] != 0xff;
            if (S9xGetByte(ptr + index) == 0xfd && !active) {
                if (end < 0)
                    end = column;
                // Keep the final exit and its trailing screen in the framing.
                if (column >= end + 16) {
                    complete = true;
                    break;
                }
            }
        }
    } catch (...) {
        complete = false;
    }
    if (!retro_unserialize(checkpoint.data(), checkpoint.size()))
        throw std::runtime_error("Cannot restore the map preparation checkpoint");
    if (!complete)
        out.clear();
    return complete;
}
