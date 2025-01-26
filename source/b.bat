:: This configuration makes the compiler treat the source code as
:: C89-conformant with C++ comments. This configuration does not 
:: disable all compiler extensions. Note that the size of any integer 
:: of type `long` is four bytes.
@echo off
@setlocal
setlocal enabledelayedexpansion
cls

set translationunits=main.c init.c gfx.c
set optimizationlevel=%1
if [%optimizationlevel%]==[] (
    set /a optimizationlevel=0
)

set start=%time%

set startcsec=%start:~9,2%
set startsec=%start:~6,2%
set startmin=%start:~3,2%
set starthour=%start:~0,2%

:: Remove whitespace
set starthour=%starthour: =%

for %%G in (startcsec startsec startmin starthour) do call :OctalToDecimal %%G
goto Compile
:OctalToDecimal
set var=%1
set val=!%1!
if %val:~0,1%==0 (
    set %var%=%val:~1,1%
)
goto :Eof


:Compile
gcc %translationunits% -o a.exe -luser32 -lgdi32 -lwinmm -lntdll -Wall -Wextra -fpermissive -Werror=attributes -Werror=pointer-arith -Werror=pointer-sign -Werror=missing-parameter-type -Werror=vla -Werror=declaration-after-statement -Werror=multichar -Werror=old-style-declaration -Werror=cast-align -Werror=cast-qual -Werror=cast-function-type -Werror=disabled-optimization -Werror=format=2 -Werror=init-self -Werror=logical-op -Werror=missing-include-dirs -Werror=redundant-decls -Werror=shadow -Werror=undef -Werror=alloca -Werror=strict-aliasing=1 -Werror=arith-conversion -Werror=flex-array-member-not-at-end -Werror=missing-prototypes -Werror=missing-variable-declarations -Werror=inline -Werror=strict-prototypes -Werror=main -Werror=return-mismatch -Werror=enum-conversion -Werror=enum-int-mismatch -Werror=conversion -Werror=int-conversion -Werror=jump-misses-init -Werror=incompatible-pointer-types -Werror=implicit-function-declaration -std=gnu89 -fstrict-flex-arrays=2 -fdiagnostics-show-option -fno-builtin -fno-asm -O%optimizationlevel% -fmax-errors=5

if %errorlevel% NEQ 0 (
    echo Build failed. 
    EXIT /B
)

set end=%time%

set endcsec=%end:~9,2%
set endsec=%end:~6,2%
set endmin=%end:~3,2%
set endhour=%end:~0,2%

:: Remove whitespace
set endhour=%endhour: =%

for %%G in (endcsec endsec endmin endhour) do call :OctalToDecimal %%G

set /a cseccomponent=(%endcsec%-%startcsec%)
set /a seccomponent=(%endsec%-%startsec%)
set /a mincomponent=(%endmin%-%startmin%)
set /a hourcomponent=(%endhour%-%starthour%)
set /a periodc=%cseccomponent%+%seccomponent%*100+%mincomponent%*100*60+%hourcomponent%*100*3600
set /a period=(%periodc%/100)
set displayTime=%period%.%periodc:~-2%

echo Build completed in %displayTime%s. 
echo Applying manifest...

mt.exe -manifest main.manifest -outputresource:a.exe
if %errorlevel%==0 (
    echo Manifest integration successful.
    
) else (
    echo Failed to integrate the manifest.
    
)

exit /B