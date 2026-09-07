# -----------------------------------------------------------------------------
# CarryHandle native GameCube GCM/FST image helper.
#
# Generic portion extracted from DoomCube's proven native-disc workflow.
#
# The consumer owns staging the files that go inside the disc.
# -----------------------------------------------------------------------------

ifneq ($(BUILD),$(notdir $(CURDIR)))


GCM_BUILDER ?= \
	$(CARRY_ROOT)/tools/native-gcm/ch_gcm.py

GCM_APPLOADER ?= \
	$(CARRY_ROOT)/tools/native-gcm/apploader.bin

GCM_DOL ?= \
	$(PROJECT_DIR)/$(TARGET).dol

GCM_ROOT ?= \
	$(PROJECT_DIR)/$(BUILD)/disc

GCM_OUTPUT ?= \
	$(PROJECT_DIR)/$(TARGET).iso

GCM_TITLE ?= \
	$(TARGET)

GCM_REGION ?= PAL

GCM_PREPARE ?=

GCM_EXTRA_ARGS ?=


CLEAN_FILES += \
	"$(GCM_OUTPUT)"


.PHONY: iso


iso: all $(GCM_PREPARE)

	@test -f "$(GCM_DOL)" || \
		( echo "ERROR: missing DOL $(GCM_DOL)"; false )

	@test -f "$(GCM_BUILDER)" || \
		( echo "ERROR: missing GCM builder $(GCM_BUILDER)"; false )

	@test -f "$(GCM_APPLOADER)" || \
		( echo "ERROR: missing apploader $(GCM_APPLOADER)"; false )

	@test -d "$(GCM_ROOT)" || \
		( echo "ERROR: missing disc root $(GCM_ROOT)"; false )

	python3 "$(GCM_BUILDER)" \
		--dol "$(GCM_DOL)" \
		--apploader "$(GCM_APPLOADER)" \
		--root "$(GCM_ROOT)" \
		--output "$(GCM_OUTPUT)" \
		--title "$(GCM_TITLE)" \
		--region "$(GCM_REGION)" \
		$(GCM_EXTRA_ARGS)

	@echo
	@echo "Built native GameCube image:"
	@ls -lh "$(GCM_OUTPUT)"
	@echo


endif
