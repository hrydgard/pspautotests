@echo off
REM http://www.microsoft.com/resources/documentation/windows/xp/all/proddocs/en-us/for.mspx?mfr=true
REM dir *.expected /S /B

REM %PATH:~0,-2%
REM DIR > tempfile.txt
REM FOR %A IN (TYPE DEL) DO %A tempfile.txt

REM FOR /F %variable IN (`DIR`) DO
SET VVAR=

DIR *.expected /S /B > EXPECTED_FILES
for /F %%i IN (EXPECTED_FILES) DO (
	PUSHD %%~dpi
		CALL %~dp0\make.bat %%~dpi%%~ni
	POPD
)

DEL EXPECTED_FILES


REM for /F "usebackq delims==" %%i IN (`dir *.expected /S /B`) DO (
REM )
REM CD %~dp0
