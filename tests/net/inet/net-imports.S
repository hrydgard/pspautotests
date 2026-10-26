.set noreorder

#include "pspimport.s"

# The SDK's net stubs, linked with these tests, came out with wrong function counts
# ("stubs out of order"), so sceNet never got linked on hardware and sceNetInit returned 8002013A.
IMPORT_START "sceNet",0x00090000
IMPORT_FUNC	"sceNet",0x39AF39A6,sceNetInit
IMPORT_FUNC	"sceNet",0x281928A9,sceNetTerm

IMPORT_START "sceNetInet",0x00090000
IMPORT_FUNC	"sceNetInet",0x17943399,sceNetInetInit
IMPORT_FUNC	"sceNetInet",0xA9ED66B9,sceNetInetTerm
IMPORT_FUNC	"sceNetInet",0xDB094E1B,sceNetInetAccept
IMPORT_FUNC	"sceNetInet",0x1A33F9AE,sceNetInetBind
IMPORT_FUNC	"sceNetInet",0x8D7284EA,sceNetInetClose
IMPORT_FUNC	"sceNetInet",0x410B34AA,sceNetInetConnect
IMPORT_FUNC	"sceNetInet",0x162E6FD5,sceNetInetGetsockname
IMPORT_FUNC	"sceNetInet",0xD10A1A7A,sceNetInetListen
IMPORT_FUNC	"sceNetInet",0xFAABB1DD,sceNetInetPoll
IMPORT_FUNC	"sceNetInet",0xCDA85C99,sceNetInetRecv
IMPORT_FUNC	"sceNetInet",0xC91142E4,sceNetInetRecvfrom
IMPORT_FUNC	"sceNetInet",0x5BE8D595,sceNetInetSelect
IMPORT_FUNC	"sceNetInet",0x7AA671BC,sceNetInetSend
IMPORT_FUNC	"sceNetInet",0x05038FC7,sceNetInetSendto
IMPORT_FUNC	"sceNetInet",0x2FE71FE7,sceNetInetSetsockopt
IMPORT_FUNC	"sceNetInet",0x8B7B220F,sceNetInetSocket
IMPORT_FUNC	"sceNetInet",0xFBABE411,sceNetInetGetErrno
