# -----------------------------------------------------------------------------
# CarryHandle release publication helper.
#
# This module is intentionally opt-in. Consumer projects still own:
#   - how `make release` builds/packages their local artifact
#   - when a release is accepted
#   - release notes content
#   - release tag creation
#
# CarryHandle owns generic publication preflight mechanics.
#
# carryhandle.cfg remains application/build metadata. Release-specific values
# are supplied here/invocation-time and are not stored in the manifest.
#
# First slice:
#   make publish-release-check
#
# No remote release publication target is provided yet.
# -----------------------------------------------------------------------------

ifneq ($(BUILD),$(notdir $(CURDIR)))

CARRY_ROOT ?= $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)

CH_RELEASE_TOOL ?= \
	$(CARRY_ROOT)/tools/release/ch_release.py

CH_RELEASE_REPO ?= \
	$(PROJECT_DIR)

CH_RELEASE_MANIFEST ?= \
	$(PROJECT_DIR)/carryhandle.cfg

RELEASE_TAG ?=
RELEASE_ASSETS ?=
RELEASE_NOTES ?=
RELEASE_TITLE ?=
RELEASE_PROVIDER ?= auto
RELEASE_REMOTE ?= origin
RELEASE_ALLOW_DIRTY ?=

CH_RELEASE_ASSET_ARGS = \
	$(foreach asset,$(RELEASE_ASSETS),--asset "$(asset)")

CH_RELEASE_ALLOW_DIRTY_ARGS = \
	$(foreach path,$(RELEASE_ALLOW_DIRTY),--allow-dirty "$(path)")

CH_RELEASE_TITLE_ARGS = \
	$(if $(strip $(RELEASE_TITLE)),--title "$(RELEASE_TITLE)",)

.PHONY: publish-release-check help

publish-release-check:
	@test -n "$(strip $(RELEASE_TAG))" || \
		( echo "ERROR: RELEASE_TAG is required"; false )
	@test -n "$(strip $(RELEASE_ASSETS))" || \
		( echo "ERROR: RELEASE_ASSETS is required"; false )
	@test -n "$(strip $(RELEASE_NOTES))" || \
		( echo "ERROR: RELEASE_NOTES is required"; false )
	@test -f "$(CH_RELEASE_TOOL)" || \
		( echo "ERROR: missing CarryHandle release tool $(CH_RELEASE_TOOL)"; false )
	@PYTHONDONTWRITEBYTECODE=1 python3 "$(CH_RELEASE_TOOL)" check \
		--repo "$(CH_RELEASE_REPO)" \
		--manifest "$(CH_RELEASE_MANIFEST)" \
		--tag "$(RELEASE_TAG)" \
		$(CH_RELEASE_ASSET_ARGS) \
		--notes "$(RELEASE_NOTES)" \
		$(CH_RELEASE_TITLE_ARGS) \
		--provider "$(RELEASE_PROVIDER)" \
		--remote "$(RELEASE_REMOTE)" \
		$(CH_RELEASE_ALLOW_DIRTY_ARGS)

help::
	@echo
	@echo "Release publication:"
	@echo "  make publish-release-check"
	@echo "                     Validate an already-tagged release without publishing"
	@echo "                     Required: RELEASE_TAG RELEASE_ASSETS RELEASE_NOTES"
	@echo "                     Optional: RELEASE_TITLE RELEASE_PROVIDER RELEASE_REMOTE"
	@echo "                               RELEASE_ALLOW_DIRTY"

endif
