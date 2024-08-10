</$objtype/mkfile

BIN=/$objtype/bin/alt

TARG=\
	fs\

HFILES=alt.h

OFILES=\
	fs.$O\
	buffer.$O\
	client.$O\
	convS2C.$O\
	service.$O\
	notification.$O\
	tabs.$O\

</sys/src/cmd/mkmany

install:V:
	mkdir -p $BIN
	mk $MKFLAGS fs.install
