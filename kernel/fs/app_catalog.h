#ifndef OPENXP_FS_APP_CATALOG_H
#define OPENXP_FS_APP_CATALOG_H
/* One authoritative catalog: namespace aliases never imply Windows PE support.
 * They resolve only to bundled, allowlisted OpenXP ELF64 executables. */
#define OPENXP_APP_CATALOG(X) \
    X("windows/system32/notepad.exe", "bin/notepad") \
    X("windows/system32/calc.exe", "bin/calculator") \
    X("windows/system32/cmd.exe", "bin/terminal") \
    X("windows/system32/explorer.exe", "bin/filemanager") \
    X("windows/system32/mspaint.exe", "bin/paint") \
    X("windows/system32/write.exe", "bin/wordpad") \
    X("windows/system32/control.exe", "bin/settings") \
    X("windows/system32/clock.exe", "bin/clock") \
    X("windows/system32/run.exe", "bin/run") \
    X("program files/openxp/calendar.exe", "bin/calendar") \
    X("program files/openxp/mines.exe", "bin/mines") \
    X("program files/openxp/snake.exe", "bin/snake") \
    X("program files/openxp/monitor.exe", "bin/monitor") \
    X("program files/openxp/about.exe", "bin/about") \
    X("program files/openxp/canvas.exe", "bin/canvas") \
    X("system apps/notepad.exe", "bin/notepad") \
    X("system apps/calculator.exe", "bin/calculator") \
    X("system apps/terminal.exe", "bin/terminal") \
    X("system apps/filemanager.exe", "bin/filemanager") \
    X("system apps/paint.exe", "bin/paint") \
    X("system apps/wordpad.exe", "bin/wordpad") \
    X("system apps/settings.exe", "bin/settings") \
    X("system apps/clock.exe", "bin/clock") \
    X("system apps/run.exe", "bin/run") \
    X("system apps/calendar.exe", "bin/calendar") \
    X("system apps/mines.exe", "bin/mines") \
    X("system apps/snake.exe", "bin/snake") \
    X("system apps/monitor.exe", "bin/monitor") \
    X("system apps/about.exe", "bin/about") \
    X("system apps/canvas.exe", "bin/canvas")
#endif
