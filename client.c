#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>

#include "alt.h"

enum {
	Qcroot,
		Qtabs,
		Qctl,
		Qinput,
		Qtitle,
		Qstatus,
		Qaside,
		Qfeed,
		Qnotify,
	Qmax,
};

static char *cltab[] = {
	"/",
		"tabs",
		"ctl",
		"input",
		"title",
		"status",
		"aside",
		"feed",
		"notify",
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
	int	fd;
	int	showmarkdown;
};

static Client client[256];
static Buffer *root;
static int nclient;
static int time0;
static Srv *fs;

static Client*
newclient(char *aname)
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

	cl->current = bufferSearch(root, aname);
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

	f = emalloc(sizeof(*f));
	f->cl = newclient(r->ifcall.aname);
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
	Buffer *b;
	Notify *np;
	char buf[1024];
	int n;

	f = r->fid->aux;
	switch(f->level){
	case Qcroot:
		dirread9p(r, rootgen, f->cl);
		respond(r, nil);
		return;
	case Qfeed:
		// Catch EOF
		if(!f->cl->fd || !f->cl->current){
			r->ofcall.data[0] = 0;
			respond(r, nil);
			return;
		}
		b = f->cl->current;
		srvrelease(fs);
		qlock(b);
Again:
		n = pread(f->cl->fd, buf, sizeof(buf), r->ifcall.offset);
		if(n){
			// cut off the EOF
			r->ofcall.count = n;
			memmove(r->ofcall.data, buf, n);
		} else{
			rsleep(&b->rz);
			goto Again;
		}
		qunlock(b);
		memset(buf, 0, sizeof(buf));
		srvacquire(fs);
		respond(r, nil);
		return;
	case Qtitle:
		if(f->cl->current && f->cl->current->title){
			memset(buf, 0, sizeof(buf));
			snprint(buf, sizeof(buf), "%s", f->cl->current->title);
String:
			readstr(r, buf);
			respond(r, nil);
			return;	
		}
	case Qstatus:
		if(f->cl->current && f->cl->current->status){
			memset(buf, 0, sizeof(buf));
			snprint(buf, sizeof(buf), "%s", f->cl->current->status);
			goto String;
		}
	case Qaside:
		if(f->cl->current && f->cl->current->aside){
			memset(buf, 0, sizeof(buf));
			snprint(buf, sizeof(buf), "%s", f->cl->current->aside);
			goto String;
		}
	case Qnotify:
		if(f->cl->current && f->cl->current->notify){
			memset(buf, 0, sizeof(buf));
			for(np = f->cl->current->notify; np->next; np = np->next)
				// TODO: Move to fmt specifier for these
				snprint(buf, sizeof(buf), "%s\n", np->data);
			goto String;
		}
	//case Qtabs:
	// Iterate through base and show up all 'o them
		
	}
	if(!f->cl->current)
		respond(r, "no buffer selected");
	else
		respond(r, "no data available");
}

void
clwrite(Req *r)
{
	Clfid *f;
	char *s, *t, path[1024];
	int n;

	f = r->fid->aux;
	switch(f->level){
	case Qctl:
		n = r->ofcall.count = r->ifcall.count;
		s = emalloc(n+1);
		memmove(s, r->ifcall.data, n);
		t = strtok(s, " ");
		s = strtok(nil, "\n");
		if(strcmp(t, "buffer") == 0){
			if(f->cl->fd)
				close(f->cl->fd);
			f->cl->current = bufferSearch(root, s);
			memset(path, sizeof(path), 0);
			snprint(path, sizeof(path), "%s/%s/%s", logdir, root->name, s);
			f->cl->fd = open(path, OREAD);
			r->fid->aux = f;
			respond(r, nil);
		} else if(strcmp(t, "markdown")==0){
			if(f->cl->showmarkdown)
				f->cl->showmarkdown = 0;
			else
				f->cl->showmarkdown = 1;
			r->fid->aux = f;
			respond(r, nil);
		} else if(strcmp(t, "hidemarkdown")==0){
			f->cl->showmarkdown = 0;
			r->fid->aux = f;
			respond(r, nil);
		} else {
			snprint(path, sizeof(path), "%s %s", t, s);
			send(root->cmds, path);
			respond(r, nil);
		}
		return;
	case Qinput:
		if(!f->cl || !f->cl->current){
			respond(r, "No buffer selected");
			return;
		}
		n = r->ofcall.count = r->ifcall.count;
		n += strlen(f->cl->current->name) + 7;
		snprint(path, n, "input %s\n%s", f->cl->current->name, r->ifcall.data);
		send(root->cmds, path);
		respond(r, nil);
		return;
	}
	respond(r, "not implemented");
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
	root = emalloc(sizeof(*root));
	USED(root);
	root = (Buffer*)s->aux;
	fs = s;
	time0 = time(0);
}

void
clend(Srv*)
{
	postnote(PNGROUP, getpid(), "shutdown");
	exits(nil);
}
