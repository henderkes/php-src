cli: $(SAPI_CLI_PATH)

$(SAPI_CLI_PATH): $(PHP_GLOBAL_OBJS) $(PHP_BINARY_OBJS) $(PHP_CLI_OBJS)
	$(BUILD_CLI)

install-cli: $(SAPI_CLI_PATH)
	@echo "Installing PHP CLI binary:        $(INSTALL_ROOT)$(bindir)/"
	@$(mkinstalldirs) $(INSTALL_ROOT)$(bindir)
	@$(LIBTOOL) --mode=install $(INSTALL) -m 0755 $(SAPI_CLI_PATH) $(INSTALL_ROOT)$(bindir)/$(program_prefix)php$(program_suffix)$(EXEEXT)
	@echo "Installing PHP CLI man page:      $(INSTALL_ROOT)$(mandir)/man1/"
	@$(mkinstalldirs) $(INSTALL_ROOT)$(mandir)/man1
	@$(INSTALL_DATA) sapi/cli/php.1 $(INSTALL_ROOT)$(mandir)/man1/$(program_prefix)php$(program_suffix).1

cli-lib: $(SAPI_CLI_LIB_PATH)

$(SAPI_CLI_LIB_PATH): $(PHP_CLI_LIB_OBJS) $(OVERALL_TARGET)
	$(BUILD_CLI_LIB)

install-cli-lib: cli-lib
	@echo "Installing PHP CLI library:        $(INSTALL_ROOT)$(orig_libdir)/"
	@$(mkinstalldirs) $(INSTALL_ROOT)$(orig_libdir)
	@$(INSTALL_CLI_LIB)
