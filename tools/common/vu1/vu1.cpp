#include "vu1.h"

#include <string.h>
#include <xmmintrin.h>
#include <vector>

#include "vu1_ops.h"

namespace vu1 {

bool RunGenerated(State& s, uint32_t entry, GifSink& sink);   // vu1_microcode.gen.cpp

void Reset(State& s) {
    memset(&s, 0, sizeof(s));
    s.vf[0][3] = 0x3F800000u;   // 1.0f
}

bool Run(State& s, uint32_t entry, uint16_t top, GifSink& sink) {
    s.top = top;
    // The VU rounds every FMAC result toward zero; do the same for the duration of the program.
    unsigned int csr = _mm_getcsr();
    _mm_setcsr((csr & ~0x6000u) | 0x6000u);
    bool ok = RunGenerated(s, entry, sink);
    _mm_setcsr(csr);
    return ok;
}

namespace ops {

// Copy one GIF packet (tags up to and including the EOP one) out of data memory, like vusim.gif_packet.
void Kick(State& s, uint16_t addr, GifSink& sink) {
    std::vector<uint32_t> buf;
    uint32_t a = addr;
    for (int tags = 0; tags < 64; ++tags) {
        const uint32_t* tag = s.mem[a & 0x3FF];
        buf.insert(buf.end(), tag, tag + 4);
        uint64_t lo = tag[0] | ((uint64_t)tag[1] << 32);
        uint32_t nloop = lo & 0x7FFF, eop = (lo >> 15) & 1;
        uint32_t flg = (lo >> 58) & 3, nreg = (lo >> 60) & 15;
        if (nreg == 0) nreg = 16;
        uint32_t n = flg == 0 ? nloop * nreg : flg == 1 ? (nloop * nreg + 1) / 2 : nloop;
        for (uint32_t k = 0; k < n; ++k) {
            const uint32_t* q = s.mem[(a + 1 + k) & 0x3FF];
            buf.insert(buf.end(), q, q + 4);
        }
        a += 1 + n;
        if (eop) break;
    }
    sink.Kick(addr, reinterpret_cast<const uint32_t(*)[4]>(buf.data()), (int)(buf.size() / 4));
}

}  // namespace ops
}  // namespace vu1
