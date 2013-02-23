#-include ./Debug/makefile

all:
	(cd Debug; make $@)

#liblinda.so:
#	$(shell cd Debug; make $@)

clean:
	(cd Debug; make $@)

install:
	sudo cp Debug/liblinda.so /usr/lib
	sudo mkdir -p /usr/include/linda
	sudo cp inc/*.h /usr/include/linda

.PHONY: all clean dependents
.SECONDARY:
