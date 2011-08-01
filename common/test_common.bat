@echo off
del EBOOT.PBP
cls && psp-gcc -I. -I"%PSPSDK%/psp/sdk/include" -L. -L"%PSPSDK%/psp/sdk/lib" -D_PSP_FW_VERSION=150 -Wall -g -O0 common.c test_common.c -lpspumd -lpsppower -lpspdebug -lpspgu -lpspgum -lpspge -lpspdisplay -lpspsdk -lc -lpspnet -lpspnet_inet -lpspuser -lpsprtc -lpspctrl -o test_common.elf
psp-fixup-imports test_common.elf
mksfo TESTMODULE param.sfo
pack-pbp EBOOT.PBP param.sfo NUL NUL NUL NUL NUL test_common.elf NUL > NUL
del test_common.elf param.sfo
IF EXIST EBOOT.PBP (
	EBOOT.PBP
)