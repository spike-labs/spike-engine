@echo off

@set FOLDER=engine

if "%1" == "editor" goto editor
if "%1" == "web_editor" goto web_editor
if "%1" == "windows" goto windows
if "%1" == "android" goto android
if "%1" == "web" goto web
if "%1" == "vsproj" goto vsproj
if "%1" == "doc" goto doc

goto help

:editor
scons -C %FOLDER% target=editor custom_modules=..\modules
::strip .\bin\godot.windows.opt.tools.64.exe
exit

:web_editor
scons -C %FOLDER% platform=web target=editor custom_modules=..\modules
exit

:windows
scons -C %FOLDER% platform=windows target=template_release bits=64 custom_modules=..\modules
::strip .\bin\godot.windows.opt.64.exe
move .\godot\bin\godot.windows.template_release.x86_64.spike.exe .\godot\bin\windows_release_x86_64.exe

scons -C %FOLDER% platform=windows target=template_debug bits=64 custom_modules=..\modules
::strip .\bin\godot.windows.opt.debug.64.exe
move .\godot\bin\godot.windows.template_debug.x86_64.spike.exe .\godot\bin\windows_debug_x86_64.exe
exit

:android
scons -C %FOLDER% platform=android target=template_release arch=arm64 custom_modules=..\modules
scons -C %FOLDER% platform=android target=template_debug arch=arm64 custom_modules=..\modules
scons -C %FOLDER% platform=android target=template_release arch=arm32 custom_modules=..\modules
scons -C %FOLDER% platform=android target=template_debug arch=arm32 custom_modules=..\modules

cd .\%FOLDER%\platform\android\java
.\gradlew generateGodotTemplates
exit

:web
scons  -C %FOLDER% platform=web target=template_release custom_modules=../modules
move .\godot\bin\godot.web.template_release.wasm32.spike.zip .\godot\bin\web_release.zip

scons -C %FOLDER% platform=web target=template_debug custom_modules=../modules
move .\godot\bin\godot.web.template_debug.wasm32.spike.zip .\godot\bin\web_debug.zip
exit

:vsproj
scons -C %FOLDER% target=editor custom_modules=..\modules debug_symbols=yes vsproj=yes
exit

:doc
cd modules
@setlocal enabledelayedexpansion
@set dir_args=
@for /f %%i in ('dir doc_classes /S/B') do @(
    @set dpath=%%i
    @set dir_args=!dir_args%! !dpath!
)
python translation/doc/translations/extract.py --path !dir_args! --output translation/doc/translations/classes.pot

cd translation
python editor/translations/extract.py --path ..

cd %~dp0
exit

:help
echo ".\build.bat <editor|windows|android|vsproj|doc>"
