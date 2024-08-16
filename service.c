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
};

struct Service
{
	Ref;

	Channel	*cmds;
	Channel   *input;
	Buffer	*base;
	char	*name;
	int	isInitialized;
	int	childpid;
};

static Srv *fs;
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

	// NOTE: If you're sending more commands than this before they are processed, up this number
	// But also it might be time to question your design, because commands really should not be taking long
	svc->cmds = chancreate(sizeof(Cmd*), 8);
	svc->base = bufferCreate(svc->cmds, svc->input);
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
	f = mallocz(sizeof(*f), 1);
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
	f = mallocz(sizeof(*f), 1);
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

enum {
	CmdRead,
	InputRead,
};


void
svcread(Req *r)
{

	Cmd *cmd;

	char buf[CmdSize];
	Svcfid *f;

	f = r->fid->aux;
	memset(buf, 0, CmdSize);

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
		// NOTE: This stays here so we always get a good ID back on the client from the initial read
		if(!f->svc->isInitialized) {
			snprint(buf, sizeof(buf), "%d\n", SERVICEID(f->svc));
			readstr(r, buf);
			respond(r, nil);
		} else {
			// Wait for any data/command from the client
			srvrelease(fs);
		 	cmd = recvp(f->svc->cmds);
			srvacquire(fs);
			if(cmd){
				snprint(buf, sizeof(buf), "%C", cmd);
				readstr(r, buf);
			}
			respond(r, nil);
		}
		return;

	}
	respond(r, "not implemented");
}


void
svcwrite(Req *r)
{
	int n;
	Svcfid *f;
	Cmd cmd;
	Buffer *b;
	Dir *d;
	char *p, *s;
	char path[1024];

	f = r->fid->aux;

	if(f->level == Qctl){
		n = r->ofcall.count = r->ifcall.count;
		p = mallocz(n, 1);
		memmove(p, r->ifcall.data, n);
		convS2C(&cmd, p, n);
		switch(cmd.type){
		case CloneCmd:
			if(!f->svc->isInitialized){
				f->svc->name = estrdup(cmd.data);
				strcpy(f->svc->base->name, cmd.data);
				memset(path, 0, sizeof(path));
				snprint(path, sizeof(path), "%s/%s", logdir, cmd.data);
				close(create(path, OREAD, DMDIR | 0755));
				clfs.aux = f->svc->base;
				f->svc->childpid = threadpostsrv(&clfs, strdup(cmd.data));
				if(f->svc->childpid >= 0){
					s = emalloc(10+strlen(cmd.data));
					strcpy(s, "/srv/");
					strcat(s, cmd.data);
					d = dirstat(s);
					d->mode = 0666;
					if(dirwstat(s, d) < 0){
						respond(r, "unable to set mode");
						return;
					}
					free(s);
					f->svc->isInitialized++;
					r->fid->aux = f;
					respond(r, nil);
				} else 
					respond(r, "Unable to post to srv");
				return;	
			}
			// Ignore, something weird going on.
			respond(r, nil);
			return;
		case CreateCmd:
			respond(r, bufferPush(f->svc->base, cmd.buffer));
			return;
		case NotifyCmd:
			respond(r, "not implemented yet");
			return;
		case DeleteCmd:
			respond(r, bufferDrop(f->svc->base, cmd.buffer));
			return;
		case ErrorCmd:
			respond(r, "not implemented yet");
			return;
		}
		if(b = bufferSearch(f->svc->base, cmd.buffer)) {
			qlock(&b->l);
			switch(cmd.type){
			case FeedCmd:
				d = dirfstat(b->fd);
				pwrite(b->fd, cmd.data, strlen(cmd.data), d->length);
				free(d);
				if(rwakeupall(&b->rz) == 0)
					b->unread++;
				break;
			case StatusCmd:
				strcpy(b->status, cmd.data);
				break;
			case TitleCmd:
				strcpy(b->title, cmd.data);
				break;
			case SideCmd:
				strcpy(b->aside, cmd.data);
				break;
			}
			qunlock(&b->l);
			respond(r, nil);
		} else
			respond(r, "buffer not found");
		return;
	}
	respond(r, "not implemented");
}

void
svcflush(Req *r)
{
	Cmd cmd;
	int i;

	for(i = 0; i < nservice; i++)
		if(service[i].ref > 0)
			if((bufferSearchTag(service[i].base, r->tag))){
				memset(&cmd, 0, CmdSize);
				cmd.type = FlushCmd;
				send(service[i].cmds, &cmd);
				break;
			}
	respond(r, nil);
}

void
svcdestroyfid(Fid *fid)
{
	Svcfid *f;

	if(f = fid->aux){
		if(f->svc && f->svc->childpid)
			postnote(PNPROC, f->svc->childpid, "done");
		// TODO: Uncomment this after we are good to go, this is our keepalive roughly
		//fid->aux = nil;
		//if(f->svc)
		//	freeservice(f->svc);
	}
	free(f);
}

void
svcstart(Srv* s)
{
	fs = s;
	if(mtpt != nil)
		unmount(nil, mtpt);
}

void
svcend(Srv*)
{
	int i;

	if(mtpt != nil)
		unmount(nil, mtpt);
	for(i = 0; i < nservice; i++)
		if(service[i].ref){
			postnote(PNPROC, service[i].childpid, "done");
		}
	postnote(PNPROC, getpid(), "done");
	threadexitsall(nil);
}
