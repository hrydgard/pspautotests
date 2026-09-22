.set noreorder

#include "pspimport.s"

# NIDs that no firmware exports, from a library that exists on every firmware. It has to be one
# nothing else in the binary imports: two import blocks for the same library confuse the stub
# fixup ("stubs out of order") and the module then doesn't start. sceRtc is safe as long as the
# test doesn't use any rtc function itself.
IMPORT_START "sceRtc",0x40010000
IMPORT_FUNC  "sceRtc",0xDEADBEEF,bogusRtcCall
IMPORT_FUNC  "sceRtc",0x00000001,bogusRtcCall2
