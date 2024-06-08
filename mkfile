</$objtype/mkfile

BIN=/$objtype/bin/alt

TARG=\
	fs\

HFILES=alt.h

OFILES=\
	fs.$O\
	buffer.$O\
	client.$O\
	service.$O\
	notification.$O\

</sys/src/cmd/mkmany

install:V:
	mkdir -p $BIN
	mk $MKFLAGS fs.install
