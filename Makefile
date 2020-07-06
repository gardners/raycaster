.SUFFIXES: .bin .prg
.PRECIOUS:	%.ngd %.ncd %.twx vivado/%.xpr bin/%.bit bin/%.mcs bin/%.M65 bin/%.BIN

#COPT=	-Wall -g -std=gnu99 -fsanitize=address -fno-omit-frame-pointer -fsanitize-address-use-after-scope
#CC=	clang
COPT=	-Wall -g -std=gnu99 -I/opt/local/include -L/opt/local/lib -I/usr/local/include/libusb-1.0 -L/usr/local/lib
# -I/usr/local/Cellar/libusb/1.0.23/include/libusb-1.0/ -L/usr/local/Cellar/libusb/1.0.23/lib/libusb-1.0/
CC=	gcc
WINCC=	x86_64-w64-mingw32-gcc
WINCOPT=	$(COPT) -DWINDOWS -Imingw64/include -Lmingw64/lib

OPHIS=	Ophis/bin/ophis
OPHISOPT=	-4
OPHIS_MON= Ophis/bin/ophis -c

JAVA = java
KICKASS_JAR = KickAss/KickAss.jar

VIVADO=	./vivado_wrapper

CC65=  cc65
CA65=  ca65 --cpu 4510
LD65=  ld65 -t none
CL65=  cl65 --config src/tests/vicii.cfg

CBMCONVERT=	cbmconvert/cbmconvert

SDCARD_FILES=	$(SDCARD_DIR)/M65UTILS.D81 \
		$(SDCARD_DIR)/M65TESTS.D81

all:	raycaster.prg

$(SDCARD_DIR)/FREEZER.M65:
	git submodule init
	git submodule update
	( cd src/mega65-freezemenu && make FREEZER.M65 )
	cp src/mega65-freezemenu/FREEZER.M65 $(SDCARD_DIR)

$(CBMCONVERT):
	git submodule init
	git submodule update
	( cd cbmconvert && make -f Makefile.unix )

$(CC65):
	git submodule init
	git submodule update
	( cd cc65 && make -j 8 )

$(OPHIS):
	git submodule init
	git submodule update

# c-programs
tools:	$(TOOLS)

# assembly files (a65 -> prg)
utilities:	$(UTILITIES)


# ============================ done moved, print-warn, clean-target
# ophis converts the *.a65 file (assembly text) to *.prg (assembly bytes)
%.prg:	%.a65 $(OPHIS)
	$(warning =============================================================)
	$(warning ~~~~~~~~~~~~~~~~> Making: $@)
	$(OPHIS) $(OPHISOPT) $< -l $*.list -m $*.map -o $*.prg

%.prg:	utilities/%.a65 $(OPHIS)
	$(warning =============================================================)
	$(warning ~~~~~~~~~~~~~~~~> Making: $@)
	$(OPHIS) $(OPHISOPT) utilities/$< -l $*.list -m $*.map -o $*.prg

%.bin:	%.a65 $(OPHIS)
	$(warning =============================================================)
	$(warning ~~~~~~~~~~~~~~~~> Making: $@)
	$(OPHIS) $(OPHISOPT) $< -l $*.list -m $*.map -o $*.prg

%.o:	%.s $(CC65)
	$(CA65) $< -l $*.list

RAYCASTERHDRS=	RayCaster/RayCasterFixed.h \
		RayCaster/RayCaster.h \
		RayCaster/Renderer.h \
		RayCaster/RayCasterTables.h

RAYCASTERSRCS=	RayCaster/RayCasterFixed.c \
		main.c \
		mazegen.c \
		textures.c

raycaster.prg:       $(RAYCASTERSRCS) $(RAYCASTERHDRS)
#	git submodule init
#	git submodule update
	$(CL65) -I $(SRCDIR)/mega65-libc/cc65/include -O -o $*.prg --mapfile $*.map $(RAYCASTERSRCS) $(SRCDIR)/mega65-libc/cc65/src/*.c $(SRCDIR)/mega65-libc/cc65/src/*.s

pngtotextures:	pngtotextures.c Makefile
	$(CC) $(COPT) -I/usr/local/include -L/usr/local/lib -o pngtotextures pngtotextures.c -lpng

textures.c:	pngtotextures assets/*.png
	./pngtotextures assets/*.png > textures.c

/usr/bin/convert: 
	echo "Could not find the program 'convert'. Try the following:"
	echo "sudo apt-get install imagemagick"

$(TOOLDIR)/version.c: .FORCE
	echo 'const char *version_string="'`./gitversion.sh`'";' > $(TOOLDIR)/version.c

.FORCE:


clean:
	rm -f $(SDCARD_FILES) $(TOOLS) $(UTILITIES) $(TESTS)

cleangen:	clean

