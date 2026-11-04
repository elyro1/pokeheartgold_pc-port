# Read the original asset definitions without running any asset producers.
WORK_DIR := .
NODEP := 1
include config.mk
include filesystem.mk

.PHONY: pc-resource-metadata
pc-resource-metadata:
	@:
