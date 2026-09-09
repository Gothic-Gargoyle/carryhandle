# -----------------------------------------------------------------------------
# CarryHandle release publication helper.
#
# This module is intentionally opt-in. Consumer projects still own:
#   - how `make release` builds/packages their local artifact
#   - when a release is accepted
#   - release notes content
#   - release tag creation
#
# CarryHandle owns generic release-publication mechanics:
#   make publish-release-check  -> validate without publishing
#   make publish-release        -> publish and verify remote assets
#
# carryhandle.cfg remains application/build metadata. Release-specific values
# are supplied here/invocation-time and are not stored in the manifest.
#
# Publication is deliberately separate from consumer build/package logic and
# from Git tag creation/push. Existing releases are never updated in place.
# -----------------------------------------------------------------------------

ifneq ($(BUILD),$(notdir $(CURDIR)))

CARRY_ROOT ?= $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)

CH_RELEASE_TOOL ?= \
	$(CARRY_ROOT)/tools/release/ch_release.py

CH_RELEASE_REPO ?= \
	$(if $(PROJECT_DIR),$(PROJECT_DIR),$(CURDIR))

CH_RELEASE_MANIFEST ?= \
	$(PROJECT_DIR)/carryhandle.cfg

RELEASE_TAG ?=
RELEASE_TAG_MESSAGE ?= $(RELEASE_TAG)
RELEASE_VERSION = $(patsubst v%,%,$(strip $(RELEASE_TAG)))

RELEASE_ASSETS ?=
RELEASE_NOTES ?=
RELEASE_TITLE ?=
RELEASE_PROVIDER ?= auto
RELEASE_REMOTE ?= origin
RELEASE_ALLOW_DIRTY ?=

# Standard CarryHandle consumer release contract.
#
# Every consumer supplies a release-bundle target.  CarryHandle owns the
# orchestration around it.
CH_RELEASE_PACKAGE_TARGET ?= release-bundle
CH_RELEASE_CLEAN_TARGET ?= clean

# VERSION is the conventional default consumed by existing CarryHandle
# projects.  Projects may override/extend this variable when their package
# target needs additional make variables.
CH_RELEASE_PACKAGE_VARS ?= VERSION="$(RELEASE_VERSION)"

CH_RELEASE_ASSET_ARGS = \
	$(foreach asset,$(RELEASE_ASSETS),--asset "$(asset)")

CH_RELEASE_ALLOW_DIRTY_ARGS = \
	$(foreach path,$(RELEASE_ALLOW_DIRTY),--allow-dirty "$(path)")

CH_RELEASE_TITLE_ARGS = \
	$(if $(strip $(RELEASE_TITLE)),--title "$(RELEASE_TITLE)",)

.PHONY: release-check release-tag release release-push-check release-push \
	publish-release-check publish-release help


# Validate a prospective local release.
#
# Untracked files are deliberately ignored.  Generated build output and
# unrelated untracked development files must not prevent a release, but
# staged or unstaged tracked source changes do.
release-check:
	@test -n "$(strip $(RELEASE_TAG))" || \
		( echo "ERROR: RELEASE_TAG is required"; false )
	@git check-ref-format "refs/tags/$(RELEASE_TAG)" >/dev/null || \
		( echo "ERROR: invalid release tag: $(RELEASE_TAG)"; false )
	@git -C "$(CH_RELEASE_REPO)" rev-parse --git-dir >/dev/null 2>&1 || \
		( echo "ERROR: not a Git repository: $(CH_RELEASE_REPO)"; false )
	@test -z "$$(git -C "$(CH_RELEASE_REPO)" diff --name-only)" || \
		( echo "ERROR: unstaged tracked changes are not allowed:"; \
		  git -C "$(CH_RELEASE_REPO)" diff --name-only; false )
	@test -z "$$(git -C "$(CH_RELEASE_REPO)" diff --cached --name-only)" || \
		( echo "ERROR: staged changes are not allowed:"; \
		  git -C "$(CH_RELEASE_REPO)" diff --cached --name-only; false )
	@if git -C "$(CH_RELEASE_REPO)" rev-parse -q --verify \
		"refs/tags/$(RELEASE_TAG)" >/dev/null; then \
		echo "ERROR: local tag already exists: $(RELEASE_TAG)"; \
		false; \
	fi
	@echo "CarryHandle release preflight: PASS"
	@echo "Tag     : $(RELEASE_TAG)"
	@echo "Version : $(RELEASE_VERSION)"
	@echo "Commit  : $$(git -C "$(CH_RELEASE_REPO)" rev-parse HEAD)"


# Create the annotated tag only after all release packaging has succeeded.
release-tag: release-check
	@git -C "$(CH_RELEASE_REPO)" tag -a "$(RELEASE_TAG)" \
		-m "$(RELEASE_TAG_MESSAGE)"
	@test "$$(git -C "$(CH_RELEASE_REPO)" rev-list -n1 "$(RELEASE_TAG)")" = \
		"$$(git -C "$(CH_RELEASE_REPO)" rev-parse HEAD)" || \
		( echo "ERROR: created release tag does not resolve to HEAD"; false )
	@echo "CarryHandle release tag created"
	@echo "Tag    : $(RELEASE_TAG)"
	@echo "Commit : $$(git -C "$(CH_RELEASE_REPO)" rev-parse HEAD)"
	@echo "Remote push has NOT been performed."


# Public local release entry point for every CarryHandle consumer.
#
# The consumer owns release-bundle.  CarryHandle owns the common safety,
# version/tag lifecycle and clean-build orchestration.
release: release-check
	@echo
	@echo "============================================================"
	@echo " CarryHandle release $(RELEASE_TAG)"
	@echo "============================================================"
	@echo
	@if [ -n "$(strip $(CH_RELEASE_CLEAN_TARGET))" ]; then \
		echo "Cleaning via $(CH_RELEASE_CLEAN_TARGET)..."; \
		$(MAKE) "$(CH_RELEASE_CLEAN_TARGET)"; \
	fi
	@echo "Building consumer release via $(CH_RELEASE_PACKAGE_TARGET)..."
	@$(MAKE) \
		RELEASE_TAG="$(RELEASE_TAG)" \
		RELEASE_VERSION="$(RELEASE_VERSION)" \
		$(CH_RELEASE_PACKAGE_VARS) \
		"$(CH_RELEASE_PACKAGE_TARGET)"
	@$(MAKE) \
		CH_RELEASE_REPO="$(CH_RELEASE_REPO)" \
		RELEASE_TAG="$(RELEASE_TAG)" \
		RELEASE_TAG_MESSAGE="$(RELEASE_TAG_MESSAGE)" \
		release-tag
	@echo
	@echo "CarryHandle local release: PASS"
	@echo "Push explicitly with:"
	@echo "  make release-push RELEASE_TAG=$(RELEASE_TAG)"


# Verify that the local release tag is the exact current commit and that a
# branch/remote exist before performing any network mutation.
release-push-check:
	@test -n "$(strip $(RELEASE_TAG))" || \
		( echo "ERROR: RELEASE_TAG is required"; false )
	@git -C "$(CH_RELEASE_REPO)" rev-parse -q --verify \
		"refs/tags/$(RELEASE_TAG)" >/dev/null || \
		( echo "ERROR: local release tag does not exist: $(RELEASE_TAG)"; false )
	@test "$$(git -C "$(CH_RELEASE_REPO)" rev-list -n1 "$(RELEASE_TAG)")" = \
		"$$(git -C "$(CH_RELEASE_REPO)" rev-parse HEAD)" || \
		( echo "ERROR: release tag does not target current HEAD"; false )
	@test -n "$$(git -C "$(CH_RELEASE_REPO)" symbolic-ref --short -q HEAD)" || \
		( echo "ERROR: release-push requires a named branch, not detached HEAD"; false )
	@git -C "$(CH_RELEASE_REPO)" remote get-url "$(RELEASE_REMOTE)" >/dev/null || \
		( echo "ERROR: Git remote does not exist: $(RELEASE_REMOTE)"; false )
	@test -z "$$(git -C "$(CH_RELEASE_REPO)" diff --name-only)" || \
		( echo "ERROR: unstaged tracked changes are not allowed:"; \
		  git -C "$(CH_RELEASE_REPO)" diff --name-only; false )
	@test -z "$$(git -C "$(CH_RELEASE_REPO)" diff --cached --name-only)" || \
		( echo "ERROR: staged changes are not allowed:"; \
		  git -C "$(CH_RELEASE_REPO)" diff --cached --name-only; false )
	@echo "CarryHandle release push preflight: PASS"
	@echo "Tag    : $(RELEASE_TAG)"
	@echo "Branch : $$(git -C "$(CH_RELEASE_REPO)" symbolic-ref --short HEAD)"
	@echo "Remote : $(RELEASE_REMOTE)"


# Push branch + release tag as one atomic remote update.  If either ref would
# be rejected, neither is updated.
release-push: release-push-check
	@branch="$$(git -C "$(CH_RELEASE_REPO)" symbolic-ref --short HEAD)"; \
	git -C "$(CH_RELEASE_REPO)" push --atomic "$(RELEASE_REMOTE)" \
		"HEAD:refs/heads/$$branch" \
		"refs/tags/$(RELEASE_TAG)"
	@echo "CarryHandle release push: PASS"
	@echo "Tag: $(RELEASE_TAG)"


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


publish-release:
	@test -n "$(strip $(RELEASE_TAG))" || \
		( echo "ERROR: RELEASE_TAG is required"; false )
	@test -n "$(strip $(RELEASE_ASSETS))" || \
		( echo "ERROR: RELEASE_ASSETS is required"; false )
	@test -n "$(strip $(RELEASE_NOTES))" || \
		( echo "ERROR: RELEASE_NOTES is required"; false )
	@test -f "$(CH_RELEASE_TOOL)" || \
		( echo "ERROR: missing CarryHandle release tool $(CH_RELEASE_TOOL)"; false )
	@PYTHONDONTWRITEBYTECODE=1 python3 "$(CH_RELEASE_TOOL)" publish \
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
	@echo "Release workflow:"
	@echo "  make release RELEASE_TAG=vX.Y.Z"
	@echo "                     Clean, package and create an annotated local tag"
	@echo "  make release-push RELEASE_TAG=vX.Y.Z"
	@echo "                     Atomically push the current branch and release tag"
	@echo
	@echo "Consumer contract:"
	@echo "  release-bundle     Project-specific packaging target called by release"
	@echo
	@echo "Release publication:"
	@echo "  make publish-release-check"
	@echo "                     Validate an already-tagged release without publishing"
	@echo "  make publish-release"
	@echo "                     Publish a preflight-valid release and verify assets"
	@echo "                     Required: RELEASE_TAG RELEASE_ASSETS RELEASE_NOTES"
	@echo "                     Optional: RELEASE_TITLE RELEASE_PROVIDER RELEASE_REMOTE"
	@echo "                               RELEASE_ALLOW_DIRTY"

endif
