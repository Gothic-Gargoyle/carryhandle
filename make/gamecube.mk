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
#   default parallel build policy
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
# Common build options
# -----------------------------------------------------------------------------

DEBUG ?= 1
TRACE ?= 0

APP_DEBUG_CFLAGS ?=
APP_TRACE_CFLAGS ?=

# Use all available logical processors by default.
#
# Explicit GNU Make parallelism always wins:
#
#   make -j4
#   make --jobs=4
#
# CH_JOBS overrides CarryHandle's detected default when no explicit GNU Make
# job setting or inherited jobserver is already active:
#
#   make CH_JOBS=4
#
CH_JOBS ?= \
	$(shell getconf _NPROCESSORS_ONLN 2>/dev/null || nproc 2>/dev/null || echo 1)

CH_MAKE_JOB_FLAGS := \
	$(filter -j% j% --jobs% --jobserver-auth=% --jobserver-fds=%,$(MAKEFLAGS))

ifeq ($(strip $(CH_MAKE_JOB_FLAGS)),)
MAKEFLAGS += -j$(CH_JOBS)
endif

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

.DEFAULT_GOAL := all

export OUTPUT  := $(PROJECT_DIR)/$(TARGET)
export DEPSDIR := $(PROJECT_DIR)/$(BUILD)

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
	help \
	run


all: $(BUILD)


$(BUILD):
	@mkdir -p "$(PROJECT_DIR)/$(BUILD)"

	@$(MAKE) \
		--no-print-directory \
		-C "$(PROJECT_DIR)/$(BUILD)" \
		-f "$(PROJECT_MAKEFILE)" \
		"$(PROJECT_DIR)/$(TARGET).dol"


help::
	@echo "CarryHandle GameCube build"
	@echo
	@echo "Targets:"
	@echo "  make / make all    Incrementally compile the application"
	@echo "  make run           Compile, then launch on hardware with RUNNER"
	@echo "  make clean         Remove generated build outputs"
	@echo
	@echo "Options:"
	@echo "  DEBUG=0|1          Application debug hooks (default: $(DEBUG))"
	@echo "  TRACE=0|1          Application trace hooks (default: $(TRACE))"
	@echo "  CH_JOBS=N          Default parallel jobs (detected: $(CH_JOBS))"
	@echo "  make -jN           Explicit GNU Make job count; overrides CH_JOBS"


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

# Search source directories only for source prerequisites.
#
# A general VPATH also applies to object targets.  That lets an unrelated
# build leave foo.o beside foo.c and causes this build to reuse that object
# instead of producing its own foo.o in $(BUILD).  Restrict lookup to the
# source suffixes CarryHandle's object inventory supports.
vpath %.c $(SOURCES)
vpath %.cpp $(SOURCES)
vpath %.s $(SOURCES)
vpath %.S $(SOURCES)

DEPENDS := \
	$(OFILES:.o=.d)

$(OUTPUT).dol: $(OUTPUT).elf

$(OUTPUT).elf: $(OFILES)

-include $(DEPENDS)

endif
