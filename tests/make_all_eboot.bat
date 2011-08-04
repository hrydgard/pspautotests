@echo off
REM http://www.microsoft.com/resources/documentation/windows/xp/all/proddocs/en-us/for.mspx?mfr=true

DIR *.expected /S /B > EXPECTED_FILES
for /F %%i IN (EXPECTED_FILES) DO (
	PUSHD %%~dpi
		CALL %~dp0\make_eboot.bat %%~dpi%%~ni
	POPD
)

DEL EXPECTED_FILES
