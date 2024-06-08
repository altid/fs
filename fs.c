#include <u.h>
#include <libc.h>
#include <ctype.h>
#include <fcall.h>

#include "alt.h"

enum {
	Qroot,
		Qclone,
		Qclients,
			Qctl,
			Qtitle,
			Qstatus,
			Qfeed,
			Qaside,
			Qnotify,
			Qtabs,
			Qinput,
		Qservices,
			Qsclone,
			Qservice,
				Qsctl,
				Qsinput,
		Qmax,
};

static char *qinfo[Qmax] = {
	[Qroot]	"/",
	[Qclone]		"clone",
	[Qclients]		nil,
	[Qctl]			"ctl",
	[Qtitle]			"title",
	[Qstatus]			"status",
	[Qfeed]			"feed",
	[Qaside]			"aside",
	[Qnotify]			"notify",
	[Qtabs]			"tabs",
	[Qinput]			"input",
	[Qservices]	"services",
	[Qsclone]			"clone",
	[Qservice]			nil,
	[Qsctl]				"ctl",
	[Qsinput]				"input",
};

typedef struct Fid Fid;
struct Fid
{
	int	fid;
	ulong	qtype;
	ulong	uniq;
	int	busy;
	Client	*client;
	Service	*service;
	vlong	foffset;
	Fid	*next;
};

static char	*whitespace = "\t\r\n";
static int		chatty9p = 0;
static Fid		*fids;
static Fcall	rhdr, thdr;
static uchar	mdata[8192 + IOHDRSZ];
static int		messagesize = sizeof mdata;


static void	io(int, int);
static Qid		mkqid(Fid*);
static Fid		*findfid(int);
static int		dostat(Fid*, void*, int);

static char	*Auth(Fid*), *Attach(Fid*), *Version(Fid*),
		*Flush(Fid*), *Walk(Fid*), *Open(Fid*),
		*Create(Fid*), *Read(Fid*), *Write(Fid*),
		*Clunk(Fid*), *Remove(Fid*), *Stat(Fid*),
		*Wstat(Fid*);

static char *(*fcalls[])(Fid*) = {
	[Tattach]	Attach,
	[Tauth]	Auth,
	[Tclunk]	Clunk,
	[Tflush]	Flush,
	[Topen]	Open,
	[Tread]	Read,
	[Tremove]	Remove,
	[Tstat]	Stat,
	[Tversion]	Version,
	[Twalk]	Walk,
	[Twrite]	Write,
	[Twstat]	Wstat,
};

static char *
Flush(Fid *f)
{
	USED(f);
	return 0;
}

static char *
Auth(Fid *f)
{
	// TODO: Implement auth
	USED(f);
	return "alt/fs: authentication not required";
}

static char *
Attach(Fid *f)
{
	if(f->busy)
		Clunk(f);
	f->client = nil;
	f->service = nil;
	f->qtype = Qroot;
	f->busy = 1;
	thdr.qid = mkqid(f);
	return 0;
}

static char *
Version(Fid*)
{
	Fid *f;
	for(f = fids; f; f = f->next)
		if(f->busy)
			Clunk(f);
	if(rhdr.msize < 256)
		return "message size too small";
	if(rhdr.msize > sizeof(mdata))
		thdr.msize = sizeof mdata;
	else
		thdr.msize = rhdr.msize;
	thdr.version = "9P2000";
	if(strncmp(rhdr.version, "9P", 2) != 0)
		thdr.version = "unknown";
	return 0;
}

static char*
Walk(Fid *f)
{
	char *name, *err;
	int i, j; //isclient, isservice
	Fid *nf;
	ulong qtype;

	if(!f->busy)
		return "walk of unused fid";

	nf = nil;
	qtype = f->qtype;
	if(rhdr.fid != rhdr.newfid){
		nf = findfid(rhdr.newfid);
		if(nf->busy)
			return "fid in use";
		f = nf;
	}
	err = nil;
	i = 0;
	if(rhdr.nwname > 0){
		// We want to state if we're in a client or a service here in state
		for(; i<rhdr.nwname; i++){
			if(i >= MAXWELEM){
				err = "too many elements in path";
				break;
			}
			name = rhdr.wname[i];
			switch(qtype){
			case Qroot:
				if(strcmp(name, "..") == 0)
					goto Accept;
				// TODO: Check if we are at a client
				if(f->client == nil)
					goto Out;
				qtype = Qclients;
			Accept:
				thdr.wqid[i] = mkqid(f);
				break;
			case Qclients:
				if(strcmp(name, "..") == 0){
					qtype = Qroot;
					f->client = nil;
					goto Accept;
				}
				for(j = Qclients + 1; j < Qmax; j++)
					if(strcmp(name, qinfo[j]) == 0){
						qtype = j;
						break;
					}
				if(j < Qmax)
					goto Accept;
				goto Out;
			//case Qservices:
			//case Qservice:
			default:
				err = "file is not a directory";
				goto Out;
			}
		}
		Out:
		if(i < rhdr.nwname && err == nil)
			err = "file not found";
	}

	if(err != nil)
		return err;
	if(rhdr.fid != rhdr.newfid && i == rhdr.nwname){
		nf->busy = 1;
		nf->qtype = qtype;
		nf->client = f->client;
		nf->service = f->service;
		//if(nf->client != nil)
		//	incref(nf->client);
	} else if (nf == nil && rhdr.nwname > 0){
		Clunk(f);
		f->busy = 1;
		f->qtype = qtype;
		//if(f->client != nil)
		//	incref(f->client);
	}
	thdr.nwqid = i;
	return 0;
}

static char *
Clunk(Fid *f)
{
	f->busy = 0;
	//freeservice, freeclient
	f->service = nil;
	f->client = nil;
	return nil;
}

static char *
Open(Fid *f)
{
	int mode;

	if(!f->busy)
		return "open of unused fid";
	mode = rhdr.mode;

 	// TODO: clone and sclone and feed
	// with feed, set up the real fid
	// with clone + sclone, return Qctl and Qsctl after creating each
	// though we do want access to setting a name after initialization
	// we can do numbered and just reject any matching names
	thdr.qid = mkqid(f);
	thdr.iounit = messagesize - IOHDRSZ;
	return 0;
}

static char *
Create(Fid *f)
{
	USED(f);
	return "permission denied";
}

static char *
Read(Fid *f)
{
	USED(f);
	// switch on qtype, do the right thing.
	// Qroot: we want clone, services, and [0..9] clients
	// Qclient: we want all of our named files, etc
	// Qservices: we want clone and named services
	// Qservice: we want input, data, etc
	return "Open";
}

static char *
Write(Fid *f)
{
	USED(f);
	// switch on qtype, do the write thing.
	return "Chimken";
}

static char *
Remove(Fid *f)
{
	Clunk(f);
	return "permission denied";
}

static char *
Stat(Fid *f)
{
	static uchar statbuf[1024];

	if(!f->busy)
		return "stat on unused fd";
	thdr.nstat = dostat(f, statbuf, sizeof statbuf);
	if(thdr.nstat <= BIT16SZ)
		return "stat buffer too small";
	thdr.stat = statbuf;
	return 0;
}

static char *
Wstat(Fid *f)
{
	//Dir d;
	//int n;
	//char buf[1024];
	// TODO: Anything we want to allow here
	USED(f);
	return "permission denied";
}

static Qid
mkqid(Fid *f)
{
	Qid q;

	q.vers = 0;
	q.path = f->qtype;
	if(f->service || f->client)
		q.path |= f->uniq * 0x100;

	switch(f->qtype){
	case Qservice:
	case Qservices:
	case Qclients:
	case Qroot:
		q.type = QTDIR;
	default:
		q.type = QTFILE;
	}
	return q;
}

static int
dostat(Fid *f, void *p, int n)
{
	Dir d;

	if(f->qtype == Qservice)
		d.name = f->service->name;
	// TODO: look up the n for the client if we're in client
	else
		d.name = qinfo[f->qtype];
	d.uid = d.gid = d.muid = "none";
	d.qid = mkqid(f);
	if(d.qid.type & QTDIR)
		d.mode = 0755|DMDIR;
	else
		d.mode = 0644;
	d.atime = d.mtime = time(0);
	d.length = 0;
	return convD2M(&d, p, n);
}

static Fid *
findfid(int fid)
{
	Fid *f, *ff;

	ff = nil;
	for(f = fids; f; f = f->next)
		if(f->fid == fid)
			return f;
		else if(!ff && !f->busy)
			ff = f;
	if(ff != nil){
		ff->fid = fid;
		return ff;
	}

	f = emalloc(sizeof *f);
	f->fid = fid;
	f->busy = 0;
	f->client = nil;
	f->service = nil;
	f->next = fids;
	fids = f;
	return f;
}

static void
io(int in, int out)
{
	char *err;
	int n;

	while((n = read9pmsg(in, mdata, messagesize)) != 0){
		if(n < 0)
			fprint(2, "mount read: %r");
		if(convM2S(mdata, n, &rhdr) != n)
			fprint(2, "convM2S format error: %r");
		thdr.data = (char*)mdata + IOHDRSZ;
		thdr.fid = rhdr.fid;
		if(!fcalls[rhdr.type])
			err = "bad fcall request";
		else
			err = (*fcalls[rhdr.type])(findfid(rhdr.fid));
		thdr.tag = rhdr.tag;
		thdr.type = rhdr.type + 1;
		if(err){
			thdr.type = Rerror;
			thdr.ename = err;
		}
		n = convS2M(&thdr, mdata, messagesize);
		if(write(out, mdata, n) != n)
			fprint(2, "mount write\n");
	}
}

static void
usage(void)
{
	fprint(2, "usage: %s [-Dd] [-m mtpt] [-s service] [-l logdir]\n", argv0);
	exits("usage");
}

void
main(int argc, char *argv[])
{
	// We could use quotefmtinstall here
	// add in tabs at very least
	int p[2];

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
	case 's':
		srvpt = EARGF(usage());
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

	if(pipe(p) < 0)
		sysfatal("Can't make pipe: %r");

	create(logdir, OREAD, DMDIR | 0755);
	switch(rfork(RFPROC|RFNAMEG|RFNOTEG|RFNOWAIT|RFENVG|RFFDG|RFMEM)){
	case 0:
		close(p[0]);
		io(p[1], p[1]);
		postnote(PNPROC, 1, "shutdown");
		exits(0);
	case -1:
		sysfatal("fork");
	default:
		close(p[1]);
		// TODO: We want to srv if we have srvpt
		if(mount(p[0], -1, mtpt, MREPL|MCREATE, "") == -1)
			sysfatal("can't mount: %r");
		exits(0);
	}
}

void*
emalloc(int n)
{
	void *p;

	if((p = malloc(n)) != nil){
		memset(p, 0, n);
		return p;
	}
	sysfatal("out of memory");
}

char*
estrdup(char *s)
{
	char *d;
	int n;

	n = strlen(s)+1;
	d = emalloc(n);
	memmove(d, s, n);
	return d;
}

