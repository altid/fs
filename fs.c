#include <u.h>
#include <libc.h>
#include <ctype.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>

#include "alt.h"

void*
emalloc(int n)
{
	void *v;
	v = emalloc9p(n);
	setmalloctag(v, getcallerpc(&n));
	memset(v, 0, n);
	return v;
}

char*
estrdup(char *s)
{
	s = estrdup9p(s);
	setmalloctag(s, getcallerpc(&s));
	return s;
}

Srv svcfs = 
{
	.start=svcstart,
	.attach=svcattach,
	.stat=svcstat,
	.walk1=svcwalk1,
	.clone=svcclone,
	.open=svcopen,
	.read=svcread,
	.write=svcwrite,
	.flush=svcflush,
	.destroyfid=svcdestroyfid,
	.end=svcend,
};

void
usage(void)
{
	fprint(2, "usage: %s [-Dd] [-m mtpt] [-s service] [-l logdir]\n", argv0);
	exits("usage");
}

void
threadmain(int argc, char *argv[])
{
	fmtinstall('t', Tconv);
	fmtinstall('!', Nconv);
	user = getuser();
	mtpt = "/mnt/alt";
	logdir = "/tmp/alt";

	ARGBEGIN {
	case 'D': 
		chatty9p++;
		break;
	case 'm':
		mtpt = EARGF(usage());
		break;
	case 'l':
		logdir = EARGF(usage());
		break;
	case 'd':
		debug++;
		break;
	default:
		usage();
	} ARGEND;

	argv0 = "alt/fs";

	create(logdir, OREAD, DMDIR | 0755);
	threadpostmountsrv(&svcfs, nil, mtpt, MCREATE);
	exits(nil); 
}
