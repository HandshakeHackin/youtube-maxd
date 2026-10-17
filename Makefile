# SPDX-License-Identifier: GPL-3.0-or-later
SHELL := /bin/bash
.DEFAULT_GOAL := zip
.NOTPARALLEL:

export PS5_PAYLOAD_SDK ?= $(HOME)/.cache/prosperotube/sdk
export PROSPEROTUBE_SHADER_TOOLS ?= $(HOME)/.cache/prosperotube/shader-tools
export PROSPEROTUBE_NODE_HOME ?= $(HOME)/.cache/prosperotube/node/node-v24.10.0-linux-x64
export PROSPEROTUBE_PS5_DIST ?= $(CURDIR)/dist
export PATH := $(PROSPEROTUBE_NODE_HOME)/bin:/usr/lib/llvm-18/bin:$(PATH)

.PHONY: all zip deps host-deps test-deps test verify help
all: zip

zip: deps
	bash scripts/build_ps5.sh
	$(MAKE) verify

deps:
	bash scripts/bootstrap_build_deps.sh

host-deps:
	bash scripts/install_host_deps.sh

test-deps: deps
	python3 -m venv .venv
	.venv/bin/python -m pip install -r companion/requirements.lock

test: test-deps
	bash scripts/test_all.sh

verify:
	python3 scripts/verify_app_zip.py "$(PROSPEROTUBE_PS5_DIST)/PPSA99012.zip"

help:
	@printf '%s\n' 'make host-deps  Install Ubuntu 24.04 host packages (sudo)' 'make [zip]     Fetch pinned dependencies, build and verify the app ZIP' 'make deps      Prepare the SDK, shader compiler and service dependencies' 'make test      Prepare the host test environment and run offline checks' 'make verify    Verify the existing ZIP and write SHA256SUMS'
