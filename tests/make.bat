@ECHO OFF
CALL %~dp0\prepare.bat
SET BASE_FILE=%1
SET C_FILES=
SET C_FILES=%C_FILES% "%~dp0/../common/common.c" 
SET C_FILES=%C_FILES% %BASE_FILE%.c
SET ELF_FILE=%BASE_FILE%.elf
SET PRX_INFO=
REM SET PRX_INFO=%PRX_INFO% -specs=%PSPSDK%/psp/sdk/lib/prxspecs -Wl,-q,-T%PSPSDK%/psp/sdk/lib/linkfile.prx
SET PSP_LIBS=
SET PSP_LIBS=%PSP_LIBS% -lpspumd
SET PSP_LIBS=%PSP_LIBS% -lpsppower
SET PSP_LIBS=%PSP_LIBS% -lpspdebug
SET PSP_LIBS=%PSP_LIBS% -lpspgu
SET PSP_LIBS=%PSP_LIBS% -lpspgum
SET PSP_LIBS=%PSP_LIBS% -lpspge
SET PSP_LIBS=%PSP_LIBS% -lpspdisplay
SET PSP_LIBS=%PSP_LIBS% -lpspsdk
SET PSP_LIBS=%PSP_LIBS% -lc
SET PSP_LIBS=%PSP_LIBS% -lpspnet
SET PSP_LIBS=%PSP_LIBS% -lpspnet_inet
SET PSP_LIBS=%PSP_LIBS% -lpspuser
SET PSP_LIBS=%PSP_LIBS% -lpsprtc
SET PSP_LIBS=%PSP_LIBS% -lpspctrl

psp-gcc -I. -I"%PSPSDK%/psp/sdk/include" -I"%~dp0/../common" -L. -L"%PSPSDK%/psp/sdk/lib" -D_PSP_FW_VERSION=150 -Wall -g -O0 %C_FILES% %PRX_INFO% %PSP_LIBS% -o %ELF_FILE%

IF EXIST %ELF_FILE% (
	psp-fixup-imports %ELF_FILE%
)