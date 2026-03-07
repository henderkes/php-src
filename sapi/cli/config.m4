PHP_ARG_ENABLE([cli],
  [for CLI build],
  [AS_HELP_STRING([--disable-cli],
    [Disable building CLI version of PHP (this forces --without-pear)])],
  [yes],
  [no])

PHP_ARG_ENABLE([cli-lib],,
  [AS_HELP_STRING([[--enable-cli-lib[=TYPE]]],
    [Enable building of CLI SAPI library. TYPE is either
    'shared' or 'static'. [TYPE=shared]])],
  [no],
  [no])

if test "$PHP_CLI" != "no"; then
  AC_CHECK_FUNCS([setproctitle])

  AC_CHECK_HEADERS([sys/pstat.h])

  AC_CACHE_CHECK([for PS_STRINGS], [php_cv_var_PS_STRINGS],
    [AC_LINK_IFELSE([AC_LANG_PROGRAM([
      #include <machine/vmparam.h>
      #include <sys/exec.h>
    ],
    [
      PS_STRINGS->ps_nargvstr = 1;
      PS_STRINGS->ps_argvstr = "foo";
    ])],
    [php_cv_var_PS_STRINGS=yes],
    [php_cv_var_PS_STRINGS=no])])
  AS_VAR_IF([php_cv_var_PS_STRINGS], [yes],
    [AC_DEFINE([HAVE_PS_STRINGS], [], [Define if the PS_STRINGS exists.])])

  PHP_ADD_MAKEFILE_FRAGMENT([$abs_srcdir/sapi/cli/Makefile.frag])

  dnl Set filename.
  SAPI_CLI_PATH=sapi/cli/php

  dnl When building a shared CLI library, ensure CLI objects are compiled with PIC.
  CLI_EXTRA_CFLAGS="-DZEND_ENABLE_STATIC_TSRMLS_CACHE=1"
  AS_CASE([$PHP_CLI_LIB],
    [yes|shared], [CLI_EXTRA_CFLAGS="$CLI_EXTRA_CFLAGS -fPIC"])

  dnl Select SAPI.
  PHP_SELECT_SAPI([cli],
    [program],
    [php_cli.c php_cli_main.c php_http_parser.c php_cli_server.c ps_title.c php_cli_process_title.c],
    [$CLI_EXTRA_CFLAGS])

  AS_CASE([$host_alias],
    [*aix*], [
      AS_VAR_IF([php_sapi_module], [shared], [
        BUILD_CLI="echo '\#! .' > php.sym && echo >>php.sym && nm -BCpg \`echo \$(PHP_GLOBAL_OBJS) \$(PHP_BINARY_OBJS) \$(PHP_CLI_OBJS) | sed 's/\([A-Za-z0-9_]*\)\.lo/.libs\/\1.o/g'\` | \$(AWK) '{ if (((\$\$2 == \"T\") || (\$\$2 == \"D\") || (\$\$2 == \"B\")) && (substr(\$\$3,1,1) != \".\")) { print \$\$3 } }' | sort -u >> php.sym && \$(LIBTOOL) --tag=CC --mode=link \$(CC) -export-dynamic \$(CFLAGS_CLEAN) \$(EXTRA_CFLAGS) \$(EXTRA_LDFLAGS_PROGRAM) \$(LDFLAGS) -Wl,-brtl -Wl,-bE:php.sym \$(PHP_RPATHS) \$(PHP_GLOBAL_OBJS) \$(PHP_BINARY_OBJS) \$(PHP_CLI_OBJS) \$(EXTRA_LIBS) \$(ZEND_EXTRA_LIBS) -o \$(SAPI_CLI_PATH)"
      ], [
        BUILD_CLI="echo '\#! .' > php.sym && echo >>php.sym && nm -BCpg \`echo \$(PHP_GLOBAL_OBJS) \$(PHP_BINARY_OBJS) \$(PHP_CLI_OBJS) | sed 's/\([A-Za-z0-9_]*\)\.lo/\1.o/g'\` | \$(AWK) '{ if (((\$\$2 == \"T\") || (\$\$2 == \"D\") || (\$\$2 == \"B\")) && (substr(\$\$3,1,1) != \".\")) { print \$\$3 } }' | sort -u >> php.sym && \$(LIBTOOL) --tag=CC --mode=link \$(CC) -export-dynamic \$(CFLAGS_CLEAN) \$(EXTRA_CFLAGS) \$(EXTRA_LDFLAGS_PROGRAM) \$(LDFLAGS) -Wl,-brtl -Wl,-bE:php.sym \$(PHP_RPATHS) \$(PHP_GLOBAL_OBJS) \$(PHP_BINARY_OBJS) \$(PHP_CLI_OBJS) \$(EXTRA_LIBS) \$(ZEND_EXTRA_LIBS) -o \$(SAPI_CLI_PATH)"
      ])
    ],
    [*darwin*], [
      BUILD_CLI="\$(CC) \$(CFLAGS_CLEAN) \$(EXTRA_CFLAGS) \$(EXTRA_LDFLAGS_PROGRAM) \$(LDFLAGS) \$(NATIVE_RPATHS) \$(PHP_GLOBAL_OBJS:.lo=.o) \$(PHP_BINARY_OBJS:.lo=.o) \$(PHP_CLI_OBJS:.lo=.o) \$(PHP_FRAMEWORKS) \$(EXTRA_LIBS) \$(ZEND_EXTRA_LIBS) -o \$(SAPI_CLI_PATH)"
    ], [
      BUILD_CLI="\$(LIBTOOL) --tag=CC --mode=link \$(CC) -export-dynamic \$(CFLAGS_CLEAN) \$(EXTRA_CFLAGS) \$(EXTRA_LDFLAGS_PROGRAM) \$(LDFLAGS) \$(PHP_RPATHS) \$(PHP_GLOBAL_OBJS:.lo=.o) \$(PHP_BINARY_OBJS:.lo=.o) \$(PHP_CLI_OBJS:.lo=.o) \$(EXTRA_LIBS) \$(ZEND_EXTRA_LIBS) -o \$(SAPI_CLI_PATH)"
    ])

  dnl Set executable for tests.
  PHP_EXECUTABLE="\$(top_builddir)/\$(SAPI_CLI_PATH)"

  PHP_SUBST([PHP_EXECUTABLE])
  PHP_SUBST([SAPI_CLI_PATH])
  PHP_SUBST([BUILD_CLI])

  AC_CONFIG_FILES([sapi/cli/php.1])

  PHP_INSTALL_HEADERS([sapi/cli], [cli.h])
fi

if test "$PHP_CLI_LIB" != "no"; then
  if test "$PHP_CLI" = "no"; then
    AC_MSG_ERROR([--enable-cli-lib requires --enable-cli (the default)])
  fi

  AC_MSG_CHECKING([for CLI SAPI library support])

  dnl CLI library objects: same as CLI but without php_cli_main (which has main()).
  PHP_CLI_LIB_OBJS="sapi/cli/php_cli.lo sapi/cli/php_http_parser.lo sapi/cli/php_cli_server.lo sapi/cli/ps_title.lo sapi/cli/php_cli_process_title.lo"

  AS_CASE([$PHP_CLI_LIB],
    [yes|shared], [
      AC_MSG_RESULT([shared])
      AS_CASE([$host_alias],
        [*darwin*], [
          SAPI_CLI_LIB_PATH=sapi/cli/libphpcli.dylib
          BUILD_CLI_LIB="\$(CC) -dynamiclib \$(CFLAGS_CLEAN) \$(EXTRA_CFLAGS) \$(LDFLAGS) -o \$(SAPI_CLI_LIB_PATH) \$(PHP_CLI_LIB_OBJS:.lo=.o) -Llibs -lphp"
          INSTALL_CLI_LIB="\$(INSTALL) -m 0755 \$(SAPI_CLI_LIB_PATH) \$(INSTALL_ROOT)\$(orig_libdir)/libphpcli.dylib"
        ], [
          SAPI_CLI_LIB_PATH=sapi/cli/libphpcli.so
          BUILD_CLI_LIB="\$(CC) -shared \$(CFLAGS_CLEAN) \$(EXTRA_CFLAGS) \$(LDFLAGS) -o \$(SAPI_CLI_LIB_PATH) \$(PHP_CLI_LIB_OBJS:.lo=.o) -Llibs -lphp"
          INSTALL_CLI_LIB="\$(INSTALL) -m 0755 \$(SAPI_CLI_LIB_PATH) \$(INSTALL_ROOT)\$(orig_libdir)/libphpcli.so"
        ])
    ],
    [static], [
      AC_MSG_RESULT([static])
      SAPI_CLI_LIB_PATH=sapi/cli/libphpcli.a
      BUILD_CLI_LIB="\$(AR) rcs \$(SAPI_CLI_LIB_PATH) \$(PHP_CLI_LIB_OBJS:.lo=.o)"
      INSTALL_CLI_LIB="\$(INSTALL) -m 0644 \$(SAPI_CLI_LIB_PATH) \$(INSTALL_ROOT)\$(orig_libdir)/libphpcli.a"
    ],
    [AC_MSG_ERROR([Invalid value for --enable-cli-lib: $PHP_CLI_LIB])])

  install_binary_targets="$install_binary_targets install-cli-lib"

  PHP_SUBST([PHP_CLI_LIB_OBJS])
  PHP_SUBST([SAPI_CLI_LIB_PATH])
  PHP_SUBST([BUILD_CLI_LIB])
  PHP_SUBST([INSTALL_CLI_LIB])
fi
