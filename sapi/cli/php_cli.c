/*
   +----------------------------------------------------------------------+
   | Copyright (c) The PHP Group                                          |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | https://www.php.net/license/3_01.txt                                 |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Author: Edin Kadribasic <edink@php.net>                              |
   |         Marcus Boerger <helly@php.net>                               |
   |         Johannes Schlueter <johannes@php.net>                        |
   |         Parts based on CGI SAPI Module by                            |
   |         Rasmus Lerdorf, Stig Bakken and Zeev Suraski                 |
   +----------------------------------------------------------------------+
*/

#include "php.h"
#include "php_globals.h"
#include "php_variables.h"
#include "php_ini_builder.h"
#include "zend_hash.h"
#include "zend_modules.h"
#include "zend_interfaces.h"

#include "ext/reflection/php_reflection.h"

#include "SAPI.h"

#include <stdio.h>
#ifdef PHP_WIN32
#include "win32/time.h"
#include "win32/signal.h"
#include "win32/console.h"
#include <process.h>
#include <shellapi.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <signal.h>
#include <locale.h>
#include "zend.h"
#include "zend_extensions.h"
#include "php_ini.h"
#include "php_main.h"
#include "fopen_wrappers.h"
#include "ext/standard/php_standard.h"
#include "ext/standard/dl_arginfo.h"
#include "cli.h"
#ifdef PHP_WIN32
#include <io.h>
#include <fcntl.h>
#include "win32/php_registry.h"
#endif

#ifdef __riscos__
#include <unixlib/local.h>
#endif

#include "zend_compile.h"
#include "zend_execute.h"
#include "zend_highlight.h"
#include "zend_exceptions.h"

#include "php_getopt.h"

#ifndef PHP_CLI_WIN32_NO_CONSOLE
#include "php_cli_server.h"
#endif

#include "ps_title.h"
#include "php_cli_process_title.h"
#include "php_cli_process_title_arginfo.h"

#ifndef PHP_WIN32
# define php_select(m, r, w, e, t)	select(m, r, w, e, t)
#else
# include "win32/select.h"
#endif

#if defined(PHP_WIN32) && defined(HAVE_OPENSSL_EXT)
# include "openssl/applink.c"
#endif

PHPAPI extern char *php_ini_opened_path;
PHPAPI extern char *php_ini_scanned_path;
PHPAPI extern char *php_ini_scanned_files;

#if defined(PHP_WIN32)
#if defined(ZTS)
ZEND_TSRMLS_CACHE_DEFINE()
#endif
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif

extern const char HARDCODED_INI[];
extern const opt_struct OPTIONS[];
extern sapi_module_struct cli_sapi_module;
extern const zend_function_entry additional_functions[];
extern void php_cli_usage(char *argv0);
extern int do_cli(int argc, char **argv);
extern void sapi_cli_ini_defaults(HashTable *configuration_hash);
#if defined(PHP_WIN32)
extern DWORD orig_cp;
extern BOOL WINAPI php_cli_win32_ctrl_handler(DWORD sig);
#endif

#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif
#ifndef STDERR_FILENO
#define STDERR_FILENO 2
#endif

/* {{{ main */
#ifdef PHP_CLI_WIN32_NO_CONSOLE
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
#else
int main(int argc, char *argv[])
#endif
{
#if defined(PHP_WIN32)
# ifdef PHP_CLI_WIN32_NO_CONSOLE
	int argc = __argc;
	char **argv = __argv;
# endif
	int num_args;
	wchar_t **argv_wide;
	char **argv_save = argv;
	BOOL using_wide_argv = 0;
#endif

	int c;
	int exit_status = SUCCESS;
	int module_started = 0, sapi_started = 0;
	char *php_optarg = NULL;
	int php_optind = 1, use_extended_info = 0;
	char *ini_path_override = NULL;
	struct php_ini_builder ini_builder;
	int ini_ignore = 0;
	sapi_module_struct *sapi_module_ptr = &cli_sapi_module;

	/*
	 * Do not move this initialization. It needs to happen before argv is used
	 * in any way.
	 */
	argv = save_ps_args(argc, argv);

#if defined(PHP_WIN32) && !defined(PHP_CLI_WIN32_NO_CONSOLE)
	php_win32_console_fileno_set_vt100(STDOUT_FILENO, TRUE);
	php_win32_console_fileno_set_vt100(STDERR_FILENO, TRUE);
#endif

	cli_sapi_module.additional_functions = additional_functions;

#if defined(PHP_WIN32) && defined(_DEBUG)
	{
		char *tmp = getenv("PHP_WIN32_DEBUG_HEAP");
		if (tmp && ZEND_ATOL(tmp)) {
			int tmp_flag;
			_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
			_CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
			_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
			_CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
			_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
			_CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
			tmp_flag = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
			tmp_flag |= _CRTDBG_DELAY_FREE_MEM_DF;
			tmp_flag |= _CRTDBG_LEAK_CHECK_DF;

			_CrtSetDbgFlag(tmp_flag);
		}
	}
#endif

#if defined(SIGPIPE) && defined(SIG_IGN)
	signal(SIGPIPE, SIG_IGN); /* ignore SIGPIPE in standalone mode so
								that sockets created via fsockopen()
								don't kill PHP if the remote site
								closes it.  in apache|apxs mode apache
								does that for us!  thies@thieso.net
								20000419 */
#endif

#ifdef ZTS
	php_tsrm_startup();
# ifdef PHP_WIN32
	ZEND_TSRMLS_CACHE_UPDATE();
# endif
#endif

	zend_signal_startup();

#ifdef PHP_WIN32
	_fmode = _O_BINARY;			/*sets default for file streams to binary */
	_setmode(_fileno(stdin), O_BINARY);		/* make the stdio mode be binary */
	_setmode(_fileno(stdout), O_BINARY);		/* make the stdio mode be binary */
	_setmode(_fileno(stderr), O_BINARY);		/* make the stdio mode be binary */
#endif

	php_ini_builder_init(&ini_builder);

	while ((c = php_getopt(argc, argv, OPTIONS, &php_optarg, &php_optind, 1, 2))!=-1) {
		switch (c) {
			case 'c':
				if (ini_path_override) {
					free(ini_path_override);
				}
				ini_path_override = strdup(php_optarg);
				break;
			case 'n':
				ini_ignore = 1;
				break;
			case 'd':
				/* define ini entries on command line */
				php_ini_builder_define(&ini_builder, php_optarg);
				break;
#ifndef PHP_CLI_WIN32_NO_CONSOLE
			case 'S':
				sapi_module_ptr = &cli_server_sapi_module;
				cli_server_sapi_module.additional_functions = server_additional_functions;
				break;
#endif
			case 'h': /* help & quit */
			case '?':
				php_cli_usage(argv[0]);
				goto out;
			case PHP_GETOPT_INVALID_ARG: /* print usage on bad options, exit 1 */
				php_cli_usage(argv[0]);
				exit_status = 1;
				goto out;
			case 'i': case 'v': case 'm':
				sapi_module_ptr = &cli_sapi_module;
				goto exit_loop;
			case 'e': /* enable extended info output */
				use_extended_info = 1;
				break;
		}
	}
exit_loop:

	sapi_module_ptr->ini_defaults = sapi_cli_ini_defaults;
	sapi_module_ptr->php_ini_path_override = ini_path_override;
	sapi_module_ptr->phpinfo_as_text = 1;
	sapi_module_ptr->php_ini_ignore_cwd = 1;
	sapi_startup(sapi_module_ptr);
	sapi_started = 1;

	sapi_module_ptr->php_ini_ignore = ini_ignore;

	sapi_module_ptr->executable_location = argv[0];

	if (sapi_module_ptr == &cli_sapi_module) {
		php_ini_builder_prepend_literal(&ini_builder, HARDCODED_INI);
	}

	sapi_module_ptr->ini_entries = php_ini_builder_finish(&ini_builder);

	/* startup after we get the above ini override so we get things right */
	if (sapi_module_ptr->startup(sapi_module_ptr) == FAILURE) {
		/* there is no way to see if we must call zend_ini_deactivate()
		 * since we cannot check if EG(ini_directives) has been initialized
		 * because the executor's constructor does not set initialize it.
		 * Apart from that there seems no need for zend_ini_deactivate() yet.
		 * So we goto out. */
		exit_status = 1;
		goto out;
	}
	module_started = 1;

#if defined(PHP_WIN32)
	php_win32_cp_cli_setup();
	orig_cp = (php_win32_cp_get_orig())->id;
	/* Ignore the delivered argv and argc, read from W API. This place
		might be too late though, but this is the earliest place ATW
		we can access the internal charset information from PHP. */
	argv_wide = CommandLineToArgvW(GetCommandLineW(), &num_args);
	PHP_WIN32_CP_W_TO_ANY_ARRAY(argv_wide, num_args, argv, argc)
	using_wide_argv = 1;

	SetConsoleCtrlHandler(php_cli_win32_ctrl_handler, TRUE);
#endif

	/* -e option */
	if (use_extended_info) {
		CG(compiler_options) |= ZEND_COMPILE_EXTENDED_INFO;
	}

	zend_first_try {
#ifndef PHP_CLI_WIN32_NO_CONSOLE
		if (sapi_module_ptr == &cli_sapi_module) {
#endif
			exit_status = do_cli(argc, argv);
#ifndef PHP_CLI_WIN32_NO_CONSOLE
		} else {
			exit_status = do_cli_server(argc, argv);
		}
#endif
	} zend_end_try();
out:
	if (ini_path_override) {
		free(ini_path_override);
	}
	php_ini_builder_deinit(&ini_builder);
	if (module_started) {
		php_module_shutdown();
	}
	if (sapi_started) {
		sapi_shutdown();
	}
#ifdef ZTS
	tsrm_shutdown();
#endif

#if defined(PHP_WIN32)
	(void)php_win32_cp_cli_restore();

	if (using_wide_argv) {
		PHP_WIN32_CP_FREE_ARRAY(argv, argc);
		LocalFree(argv_wide);
	}
	argv = argv_save;
#endif
	/*
	 * Do not move this de-initialization. It needs to happen right before
	 * exiting.
	 */
	cleanup_ps_args(argv);
	exit(exit_status);
}
/* }}} */
