#include "observer.h"
#include "snes9x.h"
#include "memmap.h"
#include "ppu.h"
#include "snes9x.h"
#include <cstring>
void mcuObserve(McuVideo &s) {
    std::memcpy(s.vram.data(), Memory.VRAM, 65536);
    std::memcpy(s.ram.data(), Memory.RAM, 131072);
    std::memcpy(s.sram.data(), Memory.SRAM, 131072);
    std::memcpy(s.palette.data(), PPU.CGDATA, 512);
    for (int i = 0; i < 4; i++) {
        auto &b = PPU.BG[i];
        s.layers[i] = {b.SCBase * 2, b.NameBase * 2, b.HOffset,
                       b.VOffset,    (int)b.SCSize,  (int)b.BGSize};
    }
    s.mode = PPU.BGMode;
    s.brightness = PPU.Brightness;
    s.blank = PPU.ForcedBlanking;
    s.mainLayers = Memory.FillRAM[0x212c] | Memory.FillRAM[0x212d];
}
void mcuObserveMemory(McuVideo &s) {
    std::memcpy(s.ram.data(), Memory.RAM, 131072);
    std::memcpy(s.sram.data(), Memory.SRAM, 131072);
}
