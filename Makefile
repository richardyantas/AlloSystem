#=========================================================================
# AlloCore main makefile
#=========================================================================

include Makefile.config

# Include configuration files of AlloCore modules
# TODO: Permit selective inclusive of modules for building a library
# and doing unit tests.

MODULE_DIRS := $(GFX_DIR) $(IO_DIR) $(MATH_DIR) $(PRO_DIR) $(SND_DIR) $(SPA_DIR) $(SYS_DIR) $(TYP_DIR)

include $(SRC_DIR)$(GFX_DIR)Makefile.config
include $(SRC_DIR)$(IO_DIR)Makefile.config
include $(SRC_DIR)$(MATH_DIR)Makefile.config
include $(SRC_DIR)$(PRO_DIR)Makefile.config
include $(SRC_DIR)$(SND_DIR)Makefile.config
include $(SRC_DIR)$(SPA_DIR)Makefile.config
include $(SRC_DIR)$(SYS_DIR)Makefile.config
include $(SRC_DIR)$(TYP_DIR)Makefile.config

# Prefix full path to source files
GFX_SRC		:= $(addprefix $(SRC_DIR)$(GFX_DIR), $(GFX_SRC))
IO_SRC		:= $(addprefix $(SRC_DIR)$(IO_DIR), $(IO_SRC))
MATH_SRC	:= $(addprefix $(SRC_DIR)$(MATH_DIR), $(MATH_SRC))
PRO_SRC		:= $(addprefix $(SRC_DIR)$(PRO_DIR), $(PRO_SRC))
TYP_SRC		:= $(addprefix $(SRC_DIR)$(TYP_DIR), $(TYP_SRC))
SND_SRC		:= $(addprefix $(SRC_DIR)$(SND_DIR), $(SND_SRC))
SPA_SRC		:= $(addprefix $(SRC_DIR)$(SPA_DIR), $(SPA_SRC))
SYS_SRC		:= $(addprefix $(SRC_DIR)$(SYS_DIR), $(SYS_SRC))

# These are all the source files
SRCS		= \
		$(GFX_SRC) $(IO_SRC) $(PRO_SRC) $(MATH_SRC) \
		$(SND_SRC) $(SPA_SRC) $(SYS_SRC) $(TYP_SRC)

OBJS		= $(addsuffix .o, $(basename $(notdir $(SRCS))))

CPPFLAGS	+= $(addprefix -I, $(INC_DIRS) $(RINC_DIRS) $(BUILD_DIR)/include)
LDFLAGS		:= $(addprefix -L, $(LIB_DIRS) $(BUILD_DIR)/lib) $(LDFLAGS)

#--------------------------------------------------------------------------
# Rules
#--------------------------------------------------------------------------

include Makefile.rules

#.PHONY: clean cleanall test


# Compile and run source files in examples/ folder
examples/%.cpp experimental/%.cpp: $(SLIB_PATH) FORCE
#	@$(CXX) $(CXXFLAGS) -o $(BIN_DIR)$(*F) $@ $(LDFLAGS) -whole-archive $(SLIB_PATH)
	$(CXX) $(CXXFLAGS) -o $(BIN_DIR)$(*F) $@ $(SLIB_PATH) $(LDFLAGS)
#	@$(CXX) $(CXXFLAGS) -o $(BIN_DIR)$(*F) $@ $(SLIB_PATH) `ls $(BIN_DIR)lib/*.a`
ifneq ($(AUTORUN), 0)
	@$(BIN_DIR)$(*F)
endif


# AlloCore extensions
extensions: allojit alloutil

allojit alloutil:
	@$(MAKE) -C src/$@ install BUILD_DIR=../../$(BUILD_DIR) DESTDIR=../../$(BUILD_DIR)


# AlloCore externals
externals: gamma glv

gamma glv:
	@$(MAKE) -C externals/$@ install DESTDIR=../../$(BUILD_DIR)


#$(EXTRA_MODULES):
#	@$(MAKE) -C modules/$@ external
#	@$(MAKE) -C modules/$@ install DESTDIR=`pwd`/$(BUILD_DIR)


# Install library into path specified by DESTDIR
# Include files are copied into DESTDIR/include/LIB_NAME and
# library files are copied to DESTDIR/lib
install: $(SLIB_PATH)
	@$(INSTALL) -d $(DESTDIR)/lib

	@for v in `cd $(INC_DIR) && find * -type d ! -path '*.*'`; do \
		$(INSTALL) -d $(DESTDIR)/include/$$v; \
		$(INSTALL) -c -m 644 $(INC_DIR)$$v/*.h* $(DESTDIR)/include/$$v;\
	done

#	@$(INSTALL) -d $(addprefix $(DESTDIR)/include/$(LIB_NAME)/, $(MODULE_DIRS))
	@$(INSTALL) -C -m 644 $(SLIB_PATH) $(DESTDIR)/lib
	@$(INSTALL) -C -m 644 $(DEV_DIR)lib/*.a $(DESTDIR)/lib
#	@$(INSTALL) -c -m 644 $(EXT_LIB_DIR)* $(DESTDIR)/lib
	@$(RANLIB) $(DESTDIR)/lib/$(SLIB_FILE)


# Build unit tests
test: $(SLIB_PATH)
	@$(MAKE) -C $(TEST_DIR)
