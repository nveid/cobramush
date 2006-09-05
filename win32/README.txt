How to compile PennMUSH 1.7.x under Windows (MSVC++/MS VS.NET)
----------------------------------------------
by Nick Gammon <nick@gammon.com.au> and Javelin and Luuk de Waard

Last update: Monday, 1 November 2002

1. From the top-level pennmush directory,
   Copy the following files     to:

  win32/config.h      config.h
  win32/confmagic.h   confmagic.h
  win32/options.h     options.h
  win32/cmds.h        hdrs/cmds.h
  win32/funs.h        hdrs/funs.h
  win32/patches.h     hdrs/patches.h
  src/local.dst       src/local.c
  src/funlocal.dst    src/funlocal.c
  src/cmdlocal.dst    src/cmdlocal.c
  src/flaglocal.dst   src/flaglocal.c
  game/mushcnf.dst    game/mush.cnf

  Project files for MSVC++:
  win32/pennmush.dsw  pennmush.dsw
  win32/pennmush.dsp  pennmush.dsp

  Project files for MS VS.NET:
  win32/pennmush.vcproj  pennmush.vcproj
  win32/pennmush.sln  pennmush.sln

   (If you've already got src/*local.c files that you've modified,
    you'll just have to make sure that there are no new functions
    in src/*local.dst that're missing in your src/*local.c files)

2. If you're running under Windows NT, you may wish to edit options.h
and uncomment the #define NT_TCP option. If you can build
with this, you'll get greatly enhanced network i/o performance. This
does not work on Windows 95/98. This option is also incompatible
with @shutdown/reboot.

3. Use supplied project files in the top-level pennmush directory.

4. Compile

5. From the top-level pennmush directory, the binary is: game/pennmush.exe

