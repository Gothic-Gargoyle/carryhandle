# -----------------------------------------------------------------------------
# CarryHandle Dolphin test helper.
#
# Generic developer launch path extracted from DoomCube.
# -----------------------------------------------------------------------------

ifneq ($(BUILD),$(notdir $(CURDIR)))


DOLPHIN_IMAGE ?= \
	$(GCM_OUTPUT)

DOLPHIN ?= \
	flatpak run \
	--filesystem="$(PROJECT_DIR)" \
	org.DolphinEmu.dolphin-emu

DOLPHIN_ARGS ?= \
	-b \
	-e

TEST_PREPARE ?=


.PHONY: test


test: iso $(TEST_PREPARE)

	@test -f "$(DOLPHIN_IMAGE)" || \
		( echo "ERROR: missing test image $(DOLPHIN_IMAGE)"; false )

	@echo
	@echo "Launching $(TARGET)..."
	@echo

	$(DOLPHIN) \
		$(DOLPHIN_ARGS) \
		"$(DOLPHIN_IMAGE)"


endif
