# -----------------------------------------------------------------------------
# CarryHandle reusable GameCube build core.
#
# Generic build machinery and developer targets extracted from the proven
# DoomCube workflow.
#
# Consumer owns:
#   game/application source list
#   application-specific compiler/linker policy
#   game-specific disc staging
#   game-specific tests/regression targets
#
# CarryHandle owns:
#   outer/inner devkitPPC build recursion
#   selected CarryHandle source modules
#   common clean/run mechanics
#   DEBUG/TRACE hooks
# -----------------------------------------------------------------------------

ifndef PROJECT_DIR
$(error "Consumer must define PROJECT_DIR before including gamecube.mk")
endif

ifndef PROJECT_MAKEFILE
$(error "Consumer must define PROJECT_MAKEFILE before including gamecube.mk")
endif


CARRY_ROOT ?= \
	$(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)


# -----------------------------------------------------------------------------
# DoomCube-style common build options
# -----------------------------------------------------------------------------

DEBUG ?= 1
TRACE ?= 0

APP_DEBUG_CFLAGS ?=
APP_TRACE_CFLAGS ?=

CH_CFILES ?=

CH_SOURCE_DIRS ?= \
	$(CARRY_ROOT)/source \
	$(CARRY_ROOT)/source/video

RUNNER ?= wiiload
RUNNER_ARGS ?=
RUN_IMAGE ?= $(PROJECT_DIR)/$(TARGET).dol

CLEAN_FILES ?=
CLEAN_DIRS ?=


# -----------------------------------------------------------------------------
# Selected CarryHandle modules
# -----------------------------------------------------------------------------

ifneq ($(strip $(CH_CFILES)),)

SOURCES += \
	$(CH_SOURCE_DIRS)

CFILES += \
	$(CH_CFILES)

INCLUDE += \
	-I$(CARRY_ROOT)/include

endif


# -----------------------------------------------------------------------------
# Optional common DEBUG / TRACE compiler hooks
# -----------------------------------------------------------------------------

ifeq ($(DEBUG),1)

CFLAGS += \
	$(APP_DEBUG_CFLAGS)

endif


ifeq ($(TRACE),1)

CFLAGS += \
	$(APP_TRACE_CFLAGS)

endif


# -----------------------------------------------------------------------------
# Outer build
# -----------------------------------------------------------------------------

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT  := $(PROJECT_DIR)/$(TARGET)
export DEPSDIR := $(PROJECT_DIR)/$(BUILD)
export VPATH   := $(SOURCES)

CPPFILES ?=
sFILES   ?=
SFILES   ?=

export LD := \
	$(if $(strip $(CPPFILES)),$(CXX),$(CC))

export OFILES := \
	$(CPPFILES:.cpp=.o) \
	$(CFILES:.c=.o) \
	$(sFILES:.s=.o) \
	$(SFILES:.S=.o)


.PHONY: \
	all \
	$(BUILD) \
	clean \
	run


all: $(BUILD)


$(BUILD):
	@mkdir -p "$(PROJECT_DIR)/$(BUILD)"

	@$(MAKE) \
		--no-print-directory \
		-C "$(PROJECT_DIR)/$(BUILD)" \
		-f "$(PROJECT_MAKEFILE)" \
		"$(PROJECT_DIR)/$(TARGET).dol"


clean:
	@echo clean ...

	@rm -rf \
		"$(PROJECT_DIR)/$(BUILD)" \
		"$(PROJECT_DIR)/$(TARGET).elf" \
		"$(PROJECT_DIR)/$(TARGET).dol" \
		"$(PROJECT_DIR)/$(TARGET).elf.map" \
		$(CLEAN_FILES) \
		$(CLEAN_DIRS)


run: all
	@test -f "$(RUN_IMAGE)" || \
		( echo "ERROR: missing run image $(RUN_IMAGE)"; false )

	$(RUNNER) \
		$(RUNNER_ARGS) \
		"$(RUN_IMAGE)"


# -----------------------------------------------------------------------------
# Inner build
# -----------------------------------------------------------------------------

else

DEPENDS := \
	$(OFILES:.o=.d)

$(OUTPUT).dol: $(OUTPUT).elf

$(OUTPUT).elf: $(OFILES)

-include $(DEPENDS)

endif
