# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

OBJDIR = obj

OBJECTS=\
	$(OBJDIR)/gestures.o \
	$(OBJDIR)/immediate_interpreter.o

TEST_OBJECTS=\
	$(OBJDIR)/gestures_unittest.o \
	$(OBJDIR)/immediate_interpreter_unittest.o

TEST_MAIN=\
	$(OBJDIR)/test_main.o

TEST_EXE=test
SONAME=$(OBJDIR)/libgestures.so.0

ALL_OBJECTS=\
	$(TEST_OBJECTS) \
	$(TEST_MAIN) \
	$(OBJECTS)

ALL_OBJECT_FILES=\
	$(OBJECTS) \
	$(TEST_OBJECTS) \
	$(TEST_MAIN)

DEPDIR = .deps

DESTDIR = .

CXXFLAGS+=\
	-g \
	-fno-exceptions \
	-fno-strict-aliasing \
	-fPIC \
	-Wall \
	-Wclobbered \
	-Wempty-body \
	-Werror \
	-Wignored-qualifiers \
	-Wmissing-field-initializers \
	-Wsign-compare \
	-Wtype-limits \
	-Wuninitialized \
	-D__STDC_FORMAT_MACROS=1 \
	-D_FILE_OFFSET_BITS=64 \
	-DGESTURES_INTERNAL=1 \
	-I..

LINK_FLAGS=\
	-lbase \
	-lpthread \
	-lrt

TEST_LINK_FLAGS=\
	-lgcov \
	-lgtest

# Local compilation needs these flags, esp for code coverage testing
ifeq (g++,$(CXX))
CXXFLAGS+=\
	-O1 \
	--coverage \
	-ftest-coverage \
	-fprofile-arcs
endif

all: $(SONAME)

$(SONAME): $(OBJECTS)
	$(CXX) -shared -o $@ $(OBJECTS) -Wl,-h$(SONAME:$(OBJDIR)/%=%) \
		$(LINK_FLAGS)

$(TEST_EXE): $(ALL_OBJECTS)
	$(CXX) -o $@ $(CXXFLAGS) $(ALL_OBJECTS) $(LINK_FLAGS) $(TEST_LINK_FLAGS)

$(OBJDIR)/%.o : src/%.cc
	mkdir -p $(OBJDIR) $(DEPDIR) || true
	$(CXX) $(CXXFLAGS) -MD -c -o $@ $<
	@mv $(@:$.o=$.d) $(DEPDIR)

install: $(SONAME)
	install -D -m 0644 $(SONAME) \
		$(DESTDIR)/usr/lib/$(SONAME:$(OBJDIR)/%=%)
	ln -s $(SONAME:$(OBJDIR)/%=%) \
		$(DESTDIR)/usr/lib/$(SONAME:$(OBJDIR)/%.0=%)
	install -D -m 0644 \
		include/gestures.h $(DESTDIR)/usr/include/gestures/gestures.h

clean:
	rm -rf $(OBJDIR) $(DEPDIR) $(TEST_EXE) html app.info app.info.orig

# Unittest coverage

LCOV_EXE=/usr/bin/lcov

$(LCOV_EXE):
	sudo emerge -DNuv1 dev-util/lcov

cov: $(TEST_EXE) $(LCOV_EXE)
	lcov -d . --zerocounters
	./$(TEST_EXE)
	lcov --directory . --capture --output-file $(OBJDIR)/app.info
	sed -i.orig 's|/obj/src/|/src/|g' $(OBJDIR)/app.info
	sed -i.orig 's|/gestures/gestures/|/gestures/|g' $(OBJDIR)/app.info
	genhtml --no-function-coverage -o html $(OBJDIR)/app.info || \
		genhtml -o html $(OBJDIR)/app.info
	./tools/local_coverage_rate.sh $(OBJDIR)/app.info

.PHONY : clean cov all

-include $(ALL_OBJECT_FILES:$(OBJDIR)/%.o=$(DEPDIR)/%.d)
