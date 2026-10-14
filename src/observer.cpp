#include "observer.h"
#include "snes9x.h"
#include "memmap.h"
#include "ppu.h"
#include "gfx.h"
#include <cstring>
extern SLineData LineData[240];
void mcuObserve(McuVideo &s, bool rendered) {
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
    // The final PPU scroll registers can describe the status bar or a dialog.
    // Read the offsets that the emulator actually used for each emitted row.
    s.rasterValid = rendered;
    if (rendered)
        for (size_t row = 0; row < s.raster.size(); ++row)
            for (int bg = 0; bg < 4; ++bg)
                s.raster[row][bg] = {LineData[row].BG[bg].HOffset, LineData[row].BG[bg].VOffset};
}
void mcuObserveMemory(McuVideo &s) {
    std::memcpy(s.ram.data(), Memory.RAM, 131072);
    std::memcpy(s.sram.data(), Memory.SRAM, 131072);
}
