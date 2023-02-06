@setlocal enabledelayedexpansion
@cd ../../..
@set dir_args=
@for /f %%i in ('dir doc_classes /b /s') do @(
    @set dpath=%%i
    @set dir_args=!dir_args%! !dpath!
)
python3 spike/doc/translations/extract.py --path !dir_args! --output spike/doc/translations/classes.pot
pause