AC_DEFUN([GP_CHECK_JSONPRETTIFIER],
[
    GP_ARG_DISABLE([JSONPrettifier], [auto])
    GP_CHECK_PLUGIN_DEPS([JSONPrettifier], [YAJL],
                         [yajl >= 2.1.0])
    GP_COMMIT_PLUGIN_STATUS([JSONPrettifier])
    AC_CONFIG_FILES([
        jsonprettifier/Makefile
        jsonprettifier/src/Makefile
    ])
])
