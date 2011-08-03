@ECHO OFF
del EBOOT.PBP
del %1.elf
CALL %~dp0\make.bat %*
IF NOT EXIST %1.elf GOTO end
mksfo TESTMODULE param.sfo
pack-pbp EBOOT.PBP param.sfo NUL NUL NUL NUL NUL %1.elf NUL > NUL
REM del %1.elf param.sfo
del param.sfo
IF EXIST EBOOT.PBP (
	copy EBOOT.PBP K:\PSP\GAME\test\EBOOT.PBP /Y
	EBOOT.PBP
)
:end