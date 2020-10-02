AC_DEFUN([GP_CHECK_SETFILETYPE],
[
    GP_ARG_DISABLE([SetFileType], [auto])
    GP_CHECK_PLUGIN_DEPS([SetFileType], [SETFILETYPE],
                         [$GP_GTK_PACKAGE >= 2.8])
    GP_COMMIT_PLUGIN_STATUS([SetFileType])
    AC_CONFIG_FILES([
        setfiletype/Makefile
        setfiletype/src/Makefile
    ])
])
