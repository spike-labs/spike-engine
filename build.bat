@echo off

if "%1" == "editor" goto editor
if "%1" == "windows" goto windows
if "%1" == "android" goto android
if "%1" == "web" goto web
if "%1" == "vsproj" goto vsproj
if "%1" == "doc" goto doc

goto help

:editor
scons -C godot tools=yes target=editor custom_modules=..\modules
::strip .\bin\godot.windows.opt.tools.64.exe
exit

:windows
scons -C godot platform=windows target=template_release bits=64 custom_modules=..\modules
::strip .\bin\godot.windows.opt.64.exe
move .\godot\bin\godot.windows.template_release.x86_64.exe .\godot\bin\windows_release_x86_64.exe

scons -C godot platform=windows target=template_debug bits=64 custom_modules=..\modules
::strip .\bin\godot.windows.opt.debug.64.exe
move .\godot\bin\godot.windows.template_debug.x86_64.exe .\godot\bin\windows_debug_x86_64.exe
exit

:android
scons -C godot platform=android target=template_release arch=arm64 custom_modules=..\modules
scons -C godot platform=android target=template_debug arch=arm64 custom_modules=..\modules
scons -C godot platform=android target=template_release arch=arm32 custom_modules=..\modules
scons -C godot platform=android target=template_debug arch=arm32 custom_modules=..\modules

cd .\godot\platform\android\java
.\gradlew generateGodotTemplates
exit

:web
scons  -C godot platform=web target=template_release custom_modules=../modules
move .\godot\bin\godot.web.template_release.wasm32.zip .\godot\bin\web_release.zip

scons -C godot platform=web target=template_debug custom_modules=../modules
move .\godot\bin\godot.web.template_debug.wasm32.zip .\godot\bin\web_debug.zip
exit

:vsproj
scons -C godot target=editor custom_modules=..\modules debug_symbols=yes vsproj=yes
exit

:doc
cd modules
@setlocal enabledelayedexpansion
@set dir_args=
@for /f %%i in ('dir doc_classes /S/B') do @(
    @set dpath=%%i
    @set dir_args=!dir_args%! !dpath!
)
python spike/doc/translations/extract.py --path !dir_args! --output spike/doc/translations/classes.pot

cd spike
python editor/translations/extract.py --path ..

cd %~dp0
exit

:help
echo ".\build.bat <editor|windows|android|vsproj|doc>"
