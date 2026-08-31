#---------------------------------------------------------------------------------
# CarryHandle hello example
#---------------------------------------------------------------------------------

.SUFFIXES:

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment.")
endif

ifeq ($(strip $(DEVKITPPC)),)
$(error "Please set DEVKITPPC in your environment.")
endif

PROJECT_DIR := $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
CARRY_ROOT  := $(abspath $(PROJECT_DIR)/../..)

include $(DEVKITPRO)/libogc2/gamecube_rules

TARGET   := hello
BUILD    := build

MANIFEST             := $(PROJECT_DIR)/carryhandle.cfg
MANIFEST_TOOL        := $(CARRY_ROOT)/tools/ch_manifest.py
GENERATED_APP_HEADER := $(PROJECT_DIR)/$(BUILD)/ch_application_config.h

CARD_BUILD_TOOL       := $(CARRY_ROOT)/tools/ch_card_build.py
CARD_ASSET_TOOL       := $(CARRY_ROOT)/tools/ch_card_assets.py
CARD_BANNER           := $(PROJECT_DIR)/assets/card-banner.png
CARD_ICON             := $(PROJECT_DIR)/assets/card-icon.png

GENERATED_CARD_HEADER := $(PROJECT_DIR)/$(BUILD)/ch_card_presentation_data.h
GENERATED_CARD_SOURCE := $(PROJECT_DIR)/$(BUILD)/ch_card_presentation_data.c
GENERATED_CARD_BINARY := $(PROJECT_DIR)/$(BUILD)/ch_card_presentation.bin
GENERATED_CARD_STAMP  := $(PROJECT_DIR)/$(BUILD)/.ch_card_presentation.stamp


SOURCES  := $(PROJECT_DIR)/source \
            $(CARRY_ROOT)/source

INCLUDES := $(CARRY_ROOT)/include

CFLAGS   := -g -O2 -Wall -Wextra $(MACHDEP) $(INCLUDE)
CXXFLAGS := $(CFLAGS)

LDFLAGS  := -g $(MACHDEP) -Wl,-Map,$(notdir $@).map

LIBS     := -logc -lm
LIBDIRS  :=

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT  := $(PROJECT_DIR)/$(TARGET)
export VPATH   := $(SOURCES)
export DEPSDIR := $(PROJECT_DIR)/$(BUILD)

CFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c))) \
            $(notdir $(GENERATED_CARD_SOURCE))
CPPFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
sFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
SFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.S)))

ifeq ($(strip $(CPPFILES)),)
export LD := $(CC)
else
export LD := $(CXX)
endif

export OFILES := \
    $(CPPFILES:.cpp=.o) \
    $(CFILES:.c=.o) \
    $(sFILES:.s=.o) \
    $(SFILES:.S=.o)

export INCLUDE := \
    $(foreach dir,$(INCLUDES),-I$(dir)) \
    -I$(PROJECT_DIR)/$(BUILD) \
    -I$(LIBOGC_INC)

export LIBPATHS := \
    $(foreach dir,$(LIBDIRS),-L$(dir)/lib) \
    -L$(LIBOGC_LIB)

.PHONY: $(BUILD) clean

$(BUILD): $(GENERATED_APP_HEADER) $(GENERATED_CARD_STAMP)
	@mkdir -p $(PROJECT_DIR)/$(BUILD)
	@$(MAKE) --no-print-directory \
		-C $(PROJECT_DIR)/$(BUILD) \
		-f $(PROJECT_DIR)/Makefile

$(GENERATED_CARD_STAMP): \
		$(MANIFEST) \
		$(MANIFEST_TOOL) \
		$(CARD_BUILD_TOOL) \
		$(CARD_ASSET_TOOL) \
		$(CARD_BANNER) \
		$(CARD_ICON)
	@mkdir -p $(PROJECT_DIR)/$(BUILD)
	@PYTHONDONTWRITEBYTECODE=1 python3 $(CARD_BUILD_TOOL) \
		--manifest $(MANIFEST) \
		--output-bin $(GENERATED_CARD_BINARY) \
		--output-h $(GENERATED_CARD_HEADER) \
		--output-c $(GENERATED_CARD_SOURCE)
	@touch $(GENERATED_CARD_STAMP)

$(GENERATED_APP_HEADER): $(MANIFEST) $(MANIFEST_TOOL)
	@mkdir -p $(PROJECT_DIR)/$(BUILD)
	@PYTHONDONTWRITEBYTECODE=1 python3 $(MANIFEST_TOOL) \
		$(MANIFEST) \
		--output-header $(GENERATED_APP_HEADER)

clean:
	@echo clean ...
	@rm -rf \
		$(PROJECT_DIR)/$(BUILD) \
		$(PROJECT_DIR)/$(TARGET).elf \
		$(PROJECT_DIR)/$(TARGET).dol

else

DEPENDS := $(OFILES:.o=.d)

$(OUTPUT).dol: $(OUTPUT).elf
$(OUTPUT).elf: $(GENERATED_APP_HEADER) $(OFILES)

-include $(DEPENDS)

endif
