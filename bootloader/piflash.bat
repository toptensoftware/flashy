@echo off
setlocal


REM for /F "skip=1 tokens=1-10" %%A IN ('wmic logicaldisk get description^, deviceid')DO (
REM    if "%%A %%B" == "Removable Disk" (
REM         SET USBDRIVE=%%C
REM    )
REM )
REM 
REM if "%USBDRIVE%"=="" (
REM     echo No removeable drive found, aborting!
REM     goto EXIT
REM )

SET USBDRIVE=F:

echo Copying to %USBDRIVE%\

xcopy kernel.img %USBDRIVE%\ /Q /Y > nul
xcopy kernel7.img %USBDRIVE%\ /Q /Y > nul
REM xcopy config.txt %USBDRIVE%\ /Q /Y > nul
REM xcopy cmdline.txt %USBDRIVE%\ /Q /Y > nul

removedrive %USBDRIVE%\ -L -na

:EXIT