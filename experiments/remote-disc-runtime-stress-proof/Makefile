PROJECT_DIR := $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
CARRY_ROOT  := $(abspath $(PROJECT_DIR)/../..)

TARGET := ch_remote_runtime_stress_proof
BUILD  := build

SOURCES := \
	$(PROJECT_DIR)/source

CFILES := \
	main.c

CH_CFILES := \
	ch_dvd.c

APP_OPTFLAGS := -O2
APP_WARNINGS := -Wall

include $(CARRY_ROOT)/make/gamecube.mk
