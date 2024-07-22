AC_DEFUN([GP_CHECK_RUNSCRIPT],
[
    GP_ARG_DISABLE([RunScript], [auto])
    GP_CHECK_PLUGIN_DEPS([RunScript], [RUNSCRIPT],
                         [$GP_GTK_PACKAGE >= 2.8])
    GP_COMMIT_PLUGIN_STATUS([RunScript])
    AC_CONFIG_FILES([
        runscript/Makefile
        runscript/src/Makefile
    ])
])
