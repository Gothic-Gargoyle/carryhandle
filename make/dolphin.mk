# CarryHandle shared Dolphin launcher.
#
# Contract:
#   make iso      -> build/package the native GameCube image (defined in image.mk)
#   make dolphin  -> launch the EXISTING GCM_OUTPUT only
#   make test     -> incrementally build/package via iso, then launch Dolphin
#   make run      -> not defined here; hardware/wiiload remains owned elsewhere
#
# Important:
# - no clean step lives in this workflow
# - TEST_TARGET is intentionally NOT used here; project TEST_TARGET values may
#   name helper targets (for example q2-test-image), not image files
# - GCM_OUTPUT is the shared image.mk output path
# - normal HOME/XDG are preserved so user Flatpak installations remain visible

ifneq ($(BUILD),$(notdir $(CURDIR)))

DOLPHIN_FLATPAK ?= flatpak
DOLPHIN_APP_ID ?= org.DolphinEmu.dolphin-emu
DOLPHIN_IMAGE ?= $(GCM_OUTPUT)
DOLPHIN_FILESYSTEM ?= $(abspath $(dir $(DOLPHIN_IMAGE)))

.PHONY: dolphin test help

define CARRYHANDLE_DOLPHIN_LAUNCH
	@test -n "$(strip $(DOLPHIN_IMAGE))" || \
		( echo "ERROR: DOLPHIN_IMAGE/GCM_OUTPUT is empty"; false )
	@test -f "$(DOLPHIN_IMAGE)" || \
		( echo "ERROR: missing Dolphin image $(DOLPHIN_IMAGE)"; false )
	@echo
	@echo "Launching $(notdir $(DOLPHIN_IMAGE)) in Dolphin..."
	@echo
	$(DOLPHIN_FLATPAK) run \
		--filesystem="$(DOLPHIN_FILESYSTEM)" \
		$(DOLPHIN_APP_ID) \
		-b -e "$(DOLPHIN_IMAGE)"
endef

# Runtime-only target: never build, package, or clean.
dolphin:
	$(CARRYHANDLE_DOLPHIN_LAUNCH)

# Normal developer loop: image.mk handles incremental build/package.
# Deliberately no TEST_TARGET delegation and no clean.
test: iso
	$(CARRYHANDLE_DOLPHIN_LAUNCH)


help::
	@echo
	@echo "Dolphin:"
	@echo "  make dolphin       Launch the existing GCM/ISO without rebuilding"
	@echo "  make test          Incrementally build the image, then launch Dolphin"


endif
