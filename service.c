#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>

#include "alt.h"

#define SERVICEID(c)	((int)(((Service*)(c)) - service))

enum {
	Qsroot,
		Qclone,
		Qservices,
			Qctl,
	Qmax,
};

static char *svctab[] = {
	"/",
		"clone",
		nil,
			"ctl",
	nil,
};

Srv clfs =
{
	.start=clstart,
	.attach=clattach,
	.stat=clstat,
	.walk1=clwalk1,
	.clone=clclone,
	.open=clopen,
	.read=clread,
	.write=clwrite,
	.flush=clflush,
	.destroyfid=cldestroyfid,
	.end=clend,
};

typedef struct Svcfid Svcfid;
typedef struct Service Service;

struct Svcfid
{	int	level;
	Service 	*svc;
	// Who knows
};

struct Service
{
	Ref;

	Channel	*cmds;
	Buffer	*base;
	char	*name;
	int	isInitialized;
	int	childpid;
};

Service service[64];
int nservice;

static Service*
newservice(void)
{
	Service *svc;
	int i;

	for(i = 0; i < nservice; i++)
		if(service[i].ref == 0)
			break;
	if(i >= nelem(service))
		return nil;
	if(i == nservice)
		nservice++;
	svc = &service[i];

	svc->ref++;
	// TODO: Eventually create send and receive for return a string from commands
	// NOTE: If you're sending more commands than this before they are processed, up this number
	// But also it might be time to question your design, because commands really should not be taking long
	svc->cmds = chancreate(1024, 16);
	svc->base = bufferCreate(svc->cmds);
	svc->isInitialized = 0;
	
	return svc;
}

static void
freeservice(Service *s)
{
	if(s == nil)
		return;
	chanfree(s->cmds);
	memset(s, 0, sizeof(*s));
}

static void*
wfaux(Svcfid *f)
{
	if(f->level < Qservices)
		return nil;
	return f->svc;
}

static void
svcmkqid(Qid *q, int level, void *)
{
	q->type = 0;
	q->vers = 0;
	switch(level){
	case Qsroot:
	case Qservices:
		q->type = QTDIR;
	default:
		;
	}
}

static void
svcmkdir(Dir *d, int level, void *aux)
{
	char buf[1024];

	memset(d, 0, sizeof(*d));
	svcmkqid(&d->qid, level, aux);
	d->mode = 0444;
	d->atime = d->mtime = time(0);
	d->uid = estrdup(user);
	d->gid = estrdup(user);
	d->muid = estrdup(user);
	if(d->qid.type & QTDIR)
		d->mode |= DMDIR | 0111;
	switch(level){
	case Qservices:
		memset(buf, 0, sizeof(buf));
		snprint(buf, sizeof(buf), "%d", SERVICEID(aux));
		d->name = estrdup(buf);
		break;
	case Qctl:
	case Qclone:
		d->mode = 0666;
	default:
		d->name = estrdup(svctab[level]);
	}
}

void
svcattach(Req *r)
{
	Svcfid *f;

	// No anames
	if(r->ifcall.aname && r->ifcall.aname[0]){
		respond(r, "invalid attach specifier");
		return;
	}
	f = emalloc(sizeof(*f));
	f->level = Qsroot;
	svcmkqid(&r->fid->qid, f->level, wfaux(f));
	r->ofcall.qid = r->fid->qid;
	r->fid->aux = f;
	respond(r, nil);
}

void
svcstat(Req *r)
{
	Svcfid *f;

	f = r->fid->aux;
	svcmkdir(&r->d, f->level, wfaux(f));
	respond(r, nil);
}

char*
svcwalk1(Fid *fid, char *name, Qid *qid)
{
	Svcfid *f;
	int i, j;

	if(!(fid->qid.type&QTDIR))
		return "walk in non-directory";

	f = fid->aux;
	if(strcmp(name, "..")==0){
		switch(f->level){
		case Qsroot:
			break;
		case Qservices:
			f->level = Qsroot;
			break;
		default:
			f->level = Qservices;
		}
	} else {
		for(i = f->level+1; i < nelem(svctab); i++){
			if(svctab[i])
				if(strcmp(name, svctab[i]) == 0)
					goto Out;
			if(i == Qservices){
				j = atoi(name);
				if(j >= 0 && j < nservice){
					f->svc = &service[j];
					incref(f->svc);
					goto Out;
				}
			}
		}
Out:
		if(i >= nelem(svctab))
			return "directory entry not found";
		f->level = i;
	}
	svcmkqid(qid, f->level, wfaux(f));
	fid->qid = *qid;
	return nil;
}

char *
svcclone(Fid *oldfid, Fid *newfid)
{
	Svcfid *f, *o;
	o = oldfid->aux;
	if(o == nil)
		return "bad fid";
	f = emalloc(sizeof(*f));
	memmove(f, o, sizeof(*f));
	if(f->svc)
		incref(f->svc);
	newfid->aux = f;
	return nil;
}

void
svcopen(Req *r)
{
	Svcfid *f;
	Service *svc;

	f = r->fid->aux;
	if(f->level == Qclone){
		if((svc = newservice()) == nil){
			respond(r, "no more services");
			return;
		}
		f->level = Qctl;
		f->svc = svc;
		svcmkqid(&r->fid->qid, f->level, wfaux(f));
		r->ofcall.qid = r->fid->qid;
	}
	respond(r, nil);
}

static int
rootgen(int i, Dir *d, void *)
{
	i += Qsroot+1;
	if(i < Qservices){
		svcmkdir(d, i, 0);
		return 0;
	}
	i -= Qservices;
	if(i < nservice){
		svcmkdir(d, Qservices, &service[i]);
		return 0;
	}
	return -1;
}

static int
servicegen(int i, Dir *d, void *aux)
{
	i += Qservices+1;
	if(i >= Qmax)
		return -1;
	svcmkdir(d, i, aux);
	return 0;
}

void
svcread(Req *r)
{
	char buf[1024];
	Svcfid *f;

	f = r->fid->aux;

	switch(f->level){
	case Qsroot:
		dirread9p(r, rootgen, nil);
		respond(r, nil);
		return;
	case Qservices:
		dirread9p(r, servicegen, nil);
		respond(r, nil);
		return;
	case Qctl:
		memset(buf, 0, sizeof(buf));
		// NOTE: This stays here so we always get a good ID back on the client from the initial read
		if(!f->svc->isInitialized) {
			snprint(buf, sizeof(buf), "%d\n", SERVICEID(f->svc));
			readstr(r, buf);
		} else {
			recv(f->svc->cmds, buf);
			print("%s\n", buf);
		}
		respond(r, nil);
		return;

	}
	respond(r, "not implemented");
}

static char*
svcctl(Service *svc, char *s, char *data)
{
	// Probably notifications as well?
	Buffer *b;
	char *cmd, *targ;

	cmd = strtok(s, " ");
	targ = strtok(nil, "\n");
	if(strcmp(cmd, "feed")==0) {
		if(b = bufferSearch(svc->base, targ)) {
			seek(b->fd, 0, 2);
			write(b->fd, data, strlen(data));
			return nil;
		}
		return "buffer not found";
	} else if(strcmp(cmd, "status")==0){
		if(b = bufferSearch(svc->base, targ)) {
			strcpy(b->status, data);
			return nil;
		}
		return "buffer not found";
	} else if(strcmp(cmd, "title")==0){
		if(b = bufferSearch(svc->base, targ)) {
			strcpy(b->title, data);
			return nil;
		}
		return "buffer not found";
	} else if(strcmp(cmd, "status")==0){
		if(b = bufferSearch(svc->base, targ)) {
			strcpy(b->status, data);
			return nil;
		}
		return "buffer not found";
	} else if(strcmp(cmd, "aside")==0){
		if(b = bufferSearch(svc->base, targ)) {
			strcpy(b->aside, data);
			return nil;
		}
		return "buffer not found";
	} else if(strcmp(cmd, "notify")==0){
		// Create notification here
		return "not yet implemented";
	} else if(strcmp(cmd, "create")==0)
		return bufferPush(svc->base, targ);
	else if(strcmp(cmd, "close")==0)
		return bufferDrop(svc->base, targ);
	else 
		return "command not supported";
}

void
svcwrite(Req *r)
{
	int n;
	char *s, *t;
	Svcfid *f;
	char path[1024];

	f = r->fid->aux;

	if(f->level == Qctl){
		n = r->ofcall.count = r->ifcall.count;
		s = emalloc(n+1);
		memmove(s, r->ifcall.data, n);
		while(n > 0 && strchr("\r\n", s[n-1]))
			n--;
		s[n] = 0;
		if(f->svc->isInitialized){
			t = s;
			while(*t && strchr("\t\r\n", *t)==0)
				t++;
			while(*t && strchr("\t\r\n", *t))
				*t++ = 0;
			t = svcctl(f->svc, s, t);
			respond(r, t);
		} else {
			// Set a short limit on this probably
			f->svc->name = estrdup(s);
			f->svc->base->name = estrdup(s);
			memset(path, 0, sizeof(path));
			snprint(path, sizeof(path), "%s/%s", logdir, s);
			close(create(path, OREAD, DMDIR | 0755));
			clfs.aux = f->svc->base;
			f->svc->childpid = threadpostsrv(&clfs, s);
			if(f->svc->childpid >= 0){
				f->svc->isInitialized++;
				respond(r, nil);
			} else 
				respond(r, "Unable to post to srv");
		}
		free(s);
		return;
	}
	respond(r, "not implemented");
}

void
svcflush(Req *r)
{
	respond(r, nil);
}

void
svcdestroyfid(Fid *fid)
{
	Svcfid *f;

	if(f = fid->aux){
		if(f->svc && f->svc->childpid)
			postnote(PNGROUP, f->svc->childpid, "shutdown");
		// TODO: Uncomment this after we are good to go, this is our keepalive roughly
		//fid->aux = nil;
		//if(f->svc)
		//	freeservice(f->svc);
	}
	free(f);
}

void
svcstart(Srv*)
{
	if(mtpt != nil)
		unmount(nil, mtpt);
}

void
svcend(Srv*)
{
	postnote(PNGROUP, getpid(), "shutdown");
	threadexitsall(nil);
}
