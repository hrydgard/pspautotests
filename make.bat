@ECHO OFF
CALL %~dp0\prepare.bat
SET BASE_FILE=%1
SET C_FILES=%BASE_FILE%.c
SET ELF_FILE=%BASE_FILE%.elf
SET PRX_INFO=-specs=%PSPSDK%/psp/sdk/lib/prxspecs -Wl,-q,-T%PSPSDK%/psp/sdk/lib/linkfile.prx
psp-gcc -I. -I"%PSPSDK%/psp/sdk/include" -L. -L"%PSPSDK%/psp/sdk/lib" -D_PSP_FW_VERSION=150 -Wall -g -O0 %C_FILES% %PRX_INFO% -lpspumd -lpsppower -lpspdebug -lpspgu -lpspgum -lpspge -lpspdisplay -lpspsdk -lc -lpspuser -lpspkernel -lpsprtc -lpspctrl -o %ELF_FILE%
IF EXIST %ELF_FILE% (
	psp-fixup-imports %ELF_FILE%
)