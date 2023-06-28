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

make aarch CONFIG=release

SET USBDRIVE=F:

echo Copying to %USBDRIVE%\

pushd .\bin\release
for /r %%a in (*.img) do (
    echo "%%a" "%USBDRIVE%\%%~nxa"
    copy "%%a" "%USBDRIVE%\%%~nxa"
)
popd

removedrive %USBDRIVE%\ -L -na

:EXIT