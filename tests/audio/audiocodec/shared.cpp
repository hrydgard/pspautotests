#include "shared.h"

const char *AudioCodecToString(int codecType) {
    switch (codecType) {
    case AUDIOCODEC_AT3PLUS:
        return "AUDIOCODEC_AT3PLUS";
    case AUDIOCODEC_AT3:
        return "AUDIOCODEC_AT3";
    case AUDIOCODEC_MP3:
        return "AUDIOCODEC_MP3";
    case AUDIOCODEC_AAC:
        return "AUDIOCODEC_AAC";
    default:
        return "Unknown codec type";
    }
}

void LogCodec(SceAudiocodecCodec &codec) {
    schedf("Codec Dump:\n");
    schedf("  err: %08x\n", codec.err);
    schedf("  edramAddr: %08x\n", codec.edramAddr);
    schedf("  neededMem: %08x\n", codec.neededMem);
    schedf("  inited: %08x\n", codec.inited);
    schedf("  inBuf: %p\n", codec.inBuf);
    schedf("  inBytes: %04x\n", codec.inBytes);
    schedf("  outBuf: %p\n", codec.outBuf);
    schedf("  outBytes: %04x\n", codec.outBytes);
    schedf("  allocMem: %p\n", codec.allocMem);
    schedf("  tailRelated: %d\n", codec.tailRelated);
    schedf("  tailFlag: %d\n", codec.tailFlag);
}
