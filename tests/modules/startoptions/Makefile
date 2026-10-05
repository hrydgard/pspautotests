all:
	$(MAKE) --no-print-directory -f child.mak
	$(MAKE) --no-print-directory -f startoptions.mak

clean:
	$(MAKE) --no-print-directory -f child.mak clean
	$(MAKE) --no-print-directory -f startoptions.mak clean

rebuild: clean all
