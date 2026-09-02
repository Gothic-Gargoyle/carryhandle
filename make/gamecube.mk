# -----------------------------------------------------------------------------
# CarryHandle reusable GameCube build core.
#
# Consumer owns:
#   devkit/libogc2 setup
#   compiler/linker flags
#   include/library paths
#   clean/image/test/release targets
#
# This file only contains the build machinery shared by DoomCube and
# Quake2Cube.
# -----------------------------------------------------------------------------

ifndef PROJECT_DIR
$(error "Consumer must define PROJECT_DIR before including gamecube.mk")
endif

ifndef PROJECT_MAKEFILE
$(error "Consumer must define PROJECT_MAKEFILE before including gamecube.mk")
endif

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT  := $(PROJECT_DIR)/$(TARGET)
export DEPSDIR := $(PROJECT_DIR)/$(BUILD)
export VPATH   := $(SOURCES)

CPPFILES ?=
sFILES   ?=
SFILES   ?=

export LD := $(if $(strip $(CPPFILES)),$(CXX),$(CC))

export OFILES := \
	$(CPPFILES:.cpp=.o) \
	$(CFILES:.c=.o) \
	$(sFILES:.s=.o) \
	$(SFILES:.S=.o)

.PHONY: all $(BUILD)

all: $(BUILD)

$(BUILD):
	@mkdir -p "$(PROJECT_DIR)/$(BUILD)"
	@$(MAKE) \
		--no-print-directory \
		-C "$(PROJECT_DIR)/$(BUILD)" \
		-f "$(PROJECT_MAKEFILE)" \
		"$(PROJECT_DIR)/$(TARGET).dol"

else

DEPENDS := $(OFILES:.o=.d)

$(OUTPUT).dol: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)

-include $(DEPENDS)

endif
