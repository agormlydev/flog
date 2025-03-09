REM build lib
cl /c flog.c
lib /out:flog.lib flog.obj

REM build consumer of lib
cl consumer.c flog.lib
