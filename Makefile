# CarryHandle
#
# Root build orchestration.
#
# CarryHandle is currently source-vendored. The hello example compiles
# the framework sources it needs directly from the repository.

.PHONY: all examples hello clean

all: examples

examples: hello

hello:
	@$(MAKE) --no-print-directory -C examples/hello

clean:
	@$(MAKE) --no-print-directory -C examples/hello clean
