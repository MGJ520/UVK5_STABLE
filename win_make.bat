@echo off

@REM 环境配置-----------------------------------------------------------------------------------------------
@REM ::python  -m pip install --upgrade pip crcmod
@REM ::python3 -m pip install --upgrade pip crcmod
@REM ::arm-none-eabi-size firmware

@set PATH=D:\GNU Arm Embedded Toolchain\10 2021.10\bin;%PATH%
@set PATH=D:\GNU Arm Embedded Toolchain\10 2021.10\arm-none-eabi\bin;%PATH%
@set PATH=D:\GnuWin32\bin;%PATH%

@REM ::编译任务-----------------------------------------------------------------------------------------------
make clean

@REM make all
@REM make out

@REM pause
@REM @echo on
