CC = @CC_FOR_BUILD@
CXX = @CXX_FOR_BUILD@
CFLAGS = @CFLAGS_FOR_BUILD@
CXXFLAGS = @CXXFLAGS_FOR_BUILD@
CPPFLAGS = @CPPFLAGS_FOR_BUILD@

noinst_PROGRAMS = autorevision
autorevision_SOURCES = autorevision.cpp

# Add Autorevision header to source directory
# Skip if cross-compiling (autorevision.h should be pre-created)
all-local: $(builddir)/autorevision$(EXEEXT)
	@if [ -f $(top_builddir)/src/autorevision.h ]; then \
		echo "autorevision.h already exists, skipping generation"; \
	else \
		$(builddir)/autorevision$(EXEEXT) +cstr -v $(top_srcdir) $(top_builddir)/src/autorevision.h; \
	fi

# Remove Autorevision header from source directory
clean-local:
	-rm -f $(top_builddir)/src/autorevision.h
