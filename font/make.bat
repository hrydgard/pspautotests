@ECHO OFF
CALL ..\prepare.bat
SET BASE_FILE=fonttest
SET C_FILES=%BASE_FILE%.c libfont-imports.S
SET ELF_FILE=%BASE_FILE%.elf
psp-gcc -I. -I"%PSPSDK%/psp/sdk/include" -L. -L"%PSPSDK%/psp/sdk/lib" -D_PSP_FW_VERSION=150 -Wall -g -O0 %C_FILES% -lpspdebug -lpspge -lpspdisplay -lpspsdk -lc -lpspuser -lpspkernel -o %ELF_FILE%
IF EXIST %ELF_FILE% (
	psp-fixup-imports %ELF_FILE%
)