# -*- Mode: BSDmakefile; -*-                                              
##
## Makefile
##
## © Copyright Jamie A. Jennings 2018, 2019, 2020.
## LICENSE: MIT License (https://opensource.org/licenses/mit-license.html)
## AUTHOR: Jamie A. Jennings

default: COMPILER RUNTIME

.PHONY: COMPILER
COMPILER: 
	@echo "Building rpeg compiler..."
	@$(MAKE) -C compiler

.PHONY: RUNTIME
RUNTIME: 
	@echo "Building rpeg runtime..."
	@$(MAKE) -C runtime

.PHONY: clean
clean:
	$(MAKE) -C runtime clean
	$(MAKE) -C compiler clean
	$(MAKE) -C util clean
