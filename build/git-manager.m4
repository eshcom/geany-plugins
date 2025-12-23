AC_DEFUN([GP_CHECK_GITMANAGER],
[
    GP_ARG_DISABLE([GitManager], [auto])

    GP_CHECK_PLUGIN_DEPS([GitManager], [GITMANAGER],
                         [$GP_GTK_PACKAGE >= 2.18
                          glib-2.0
                          libgit2 >= 0.21])

    GP_COMMIT_PLUGIN_STATUS([GitManager])

    AC_CONFIG_FILES([
        git-manager/Makefile
        git-manager/src/Makefile
    ])
])
