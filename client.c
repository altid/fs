#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>

#include "alt.h"

enum {
	Qcroot,
		Qtitle,
		Qstatus,
		Qaside,
		Qfeed,
		Qinput,
		Qnotify,
		Qctl,
	Qmax,
};

static char *cltab[] = {
	"/",
		"title",
		"status",
		"aside",
		"feed",
		"input",
		"notify",
		"ctl",
	nil,
};

typedef struct Clfid Clfid;
typedef struct Client Client;

struct Clfid
{
	int	level;
	Client	*cl;
};

struct Client
{
	Ref;

	Buffer	*current;
	int	showmarkdown;
};

static Client client[256];
static Buffer *root;
static int nclient;
static int time0;

static Client*
newclient(void)
{
	Client *cl;
	int i;

	for(i = 0; i < nclient; i++)
		if(client[i].ref == 0)
			break;
	if(i >= nelem(client))
		return nil;
	if(i == nclient)
		nclient++;
	cl = &client[i];
	cl->ref++;

	cl->current = nil;
	cl->showmarkdown = 0;

	return cl;
}

static void
freeclient(Client *cl)
{
	if(cl == nil || cl->ref == 0)
		return;
	cl->ref--;
	if(cl->ref > 0)
		return;

	memset(cl, 0, sizeof(*cl));
}

static void
clmkqid(Qid *q, int level, void *aux)
{
	q->type = 0;
	q->vers = 0;
	if(level == Qcroot)
		q->type = QTDIR;
	else
		q->path = (level<<24) | (((uintptr)aux ^ time0) & 0x00ffffff);
}

static void
clmkdir(Dir *d, int level, void *aux)
{
	memset(d, 0, sizeof(*d));
	clmkqid(&d->qid, level, aux);
	d->mode = 0444;
	d->atime = d->mtime = time(0);
	d->uid = estrdup(user);
	d->gid = estrdup(user);
	d->muid = estrdup(user);
	if(d->qid.type & QTDIR)
		d->mode |= DMDIR | 0111;
	switch(level){
	case Qctl:
		d->mode = 0666;
	default:
		d->name = estrdup(cltab[level]);
	}
}

void
clattach(Req *r)
{
	Clfid *f;

	// TODO: We want to actually use the aname here
	if(r->ifcall.aname && r->ifcall.aname[0]){
		print(r->ifcall.aname);
		// TODO: Check and establish buffer, abort if it's not available
	} else {
		// TODO: If we have any valid buffers, we take the top of the list here
	}
	f = emalloc(sizeof(*f));
	f->level = Qcroot;
	clmkqid(&r->fid->qid, f->level, nil);
	r->ofcall.qid = r->fid->qid;
	r->fid->aux = f;
	respond(r, nil);
}



void
clstat(Req *r)
{
	Clfid *f;

	f = r->fid->aux;
	clmkdir(&r->d, f->level, f->cl);
	respond(r, nil);
}


char*
clwalk1(Fid *fid, char *name, Qid *qid)
{
	Clfid *f;
	int i;

	if(!(fid->qid.type&QTDIR))
		return "walk in non-directory";

	f = fid->aux;
	for(i=f->level+1; i < nelem(cltab); i++){
		if(cltab[i]){
			if(strcmp(name, cltab[i]) == 0)
				break;
		}
	}
	if(i >= nelem(cltab))
		return "directory entry not found";
	f->level = i;
	clmkqid(qid, f->level, f->cl);
	fid->qid = *qid;
	return nil;	
}

char *
clclone(Fid *oldfid, Fid *newfid)
{
	Clfid *f, *o;

	o = oldfid->aux;
	if(o == nil)
		return "bad fid";
	f = emalloc(sizeof(*f));
	memmove(f, o, sizeof(*f));
	if(f->cl)
		incref(f->cl);
	newfid->aux = f;
	return nil;
}

void
clopen(Req *r)
{
	respond(r, nil);
	// Use feed for fid, with offset as well.
}

static int
rootgen(int i, Dir *d, void *aux)
{
	i += Qcroot+1;
	if(i < Qmax){
		clmkdir(d, i, aux);
		return 0;
	}
	return -1;
}

void
clread(Req *r)
{
	Clfid *f;

	f = r->fid->aux;
	switch(f->level){
	case Qcroot:
		dirread9p(r, rootgen, f->cl);
		respond(r, nil);
		return;
	//case Qfeed:
	}
	respond(r, "not implemented");
}

void
clwrite(Req *r)
{
	// TODO: Input, ctl
	respond(r, nil);
}

void
clflush(Req *r)
{
	respond(r, nil);
}

void
cldestroyfid(Fid *fid)
{
	Clfid *f;

	if(f = fid->aux){
		//fid->aux = nil;
		//if(f->cl)
		//	freeclient(f->cl);
	}
	free(f);
}

void
clstart(Srv *s)
{
	root = (Buffer*)s->aux;
	time0 = time(0);
}

void
clend(Srv*)
{
	postnote(PNGROUP, getpid(), "shutdown");
	exits(nil);
}
