#include <u.h>
#include <libc.h>
#include <ctype.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>

#include "alt.h"

// TODO: Start turning off output if we aren't connected to anything

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

static char *nametab[] = {
	"/",
		"clone",
		nil,
			"ctl",
			"title",
			"status",
			"feed",
			"aside",
			"notify",
			"tabs",
			"input",
		"services",
			"clone",
			nil,
				"ctl",
				"input",
		nil,
};

typedef struct Altfid Altfid;
struct Altfid
{
	int	level;
	Client	*client;
	Service	*service;
	int	fd;
	vlong	foffset;
};

static char *whitespace = "\t\r\n";

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

static void*
wfaux(Altfid *f)
{
	if(f->level < Qclients)
		return nil;
	else if(f->level < Qservices)
		return f->client;
	return f->service;
}

static void
fsmkqid(Qid *q, int level, void *aux)
{

	q->type = 0;
	q->vers = 0;
	switch(level){
	case Qroot:
	case Qclients:
	case Qservices:
	case Qservice:
		q->type = QTDIR;
	default:
		q->path = (level<<24) | (((uintptr)aux ^ time0) & 0x00ffffff);
	}
}

static void
fsmkdir(Dir *d, int level, void *aux)
{
	Service *sv;
	char buf[1024];

	memset(d, 0, sizeof(*d));
	fsmkqid(&d->qid, level, aux);
	d->mode = 0444;
	d->atime = d->mtime = time0;
	d->uid = estrdup(user);
	d->gid = estrdup(user);
	d->muid = estrdup(user);
	if(d->qid.type & QTDIR)
		d->mode |= DMDIR | 0111;
	switch(level){
	case Qclients:
		snprint(buf, sizeof(buf), "%d", CLIENTID(aux));
		d->name = estrdup(buf);
		break;
	case Qservice:
		sv = (Service*)aux;
		d->name = sv->name;
		break;
	case Qctl:
	case Qsctl:
	case Qclone:
	case Qsclone:
		d->mode = 0666;
		if(0){
	case Qinput:
		d->mode = 0222;
		}
	default:
		d->name = estrdup(nametab[level]);
	}
}

static void
fsattach(Req *r)
{
	Altfid *f;

	if(r->ifcall.aname && r->ifcall.aname[0]){
		respond(r, "invalid attach specifier");
		return;
	}
	f = emalloc(sizeof(*f));
	f->level = Qroot;
	fsmkqid(&r->fid->qid, f->level, wfaux(f));
	r->ofcall.qid = r->fid->qid;
	r->fid->aux = f;
	respond(r, nil);
}

static void
fsstat(Req *r)
{
	Altfid *f;

	f = r->fid->aux;
	fsmkdir(&r->d, f->level, wfaux(f));
	respond(r, nil);
}

static char*
fswalk1(Fid *fid, char *name, Qid *qid)
{
	Altfid *f;
	int i, j;
	i = 0;

	if(!(fid->qid.type&QTDIR))
		return "walk in non-directory";

	f = fid->aux;
	if(strcmp(name, "..")==0){
		switch(f->level){
		case Qroot:
			break;
		case Qclients:
			freeclient(f->client);
			f->client = nil;
			break;
		case Qservices:
			f->level = Qroot;
			break;
		default:
			if(f->level > Qservices)
				f->level = Qservices;
			else
				f->level = Qclients;
		}
	} else if(strcmp(name, "services")==0){
		i = Qservices;
		goto Out;
	} else {
		if(nservice){
			for(j=0; j < nservice; j++){
				if(strcmp(name, service[j].name) == 0){
					f->service = &service[j];
					i = Qservice;
					break;
				}
			}
		} else {
			for(i = f->level+1; i < nelem(nametab); i++){
				if(nametab[i]){
					if(strcmp(name, nametab[i]) == 0)
						goto Out;
					// anything else?
				}
				if(i == Qclients){
					j = atoi(name);
					if(j >= 0 && j < nclient){
						f->client = &client[j];
						incref(f->client);
						goto Out;
					}
				}
			}
		}
Out:
		if(i >= nelem(nametab))
			return "directory entry not found";
		f->level = i;
	}
	fsmkqid(qid, f->level, wfaux(f));
	fid->qid = *qid;
	return nil;
}

static char*
fsclone(Fid *oldfid, Fid *newfid)
{
	Altfid *f, *o;
	o = oldfid->aux;
	if(o == nil)
		return "bad fid";
	f = emalloc(sizeof(*f));
	memmove(f, o, sizeof(*f));
	if(f->client)
		incref(f->client);
	newfid->aux = f;
	return nil;
}

static void
fsopen(Req *r)
{
	Altfid *f;
	Client *cl;
	Service *svc;
	char buf[256];

	// Switch and create on clones, etc
	f = r->fid->aux;
	cl = f->client;
	svc = f->service;
	USED(svc);
	switch(f->level){
	case Qclone:
		if((cl = newclient()) == nil){
			respond(r, "no more clients");
			return;
		}
		f->level = Qctl;
		f->client = cl;
		fsmkqid(&r->fid->qid, f->level, wfaux(f));
		r->ofcall.qid = r->fid->qid;
		break;
	case Qsclone:
		if((svc = newservice()) == nil){
			respond(r, "no more services");
		}
		f->level = Qsctl;
		f->service = svc;
		fsmkqid(&r->fid->qid, f->level, wfaux(f));
		r->ofcall.qid = r->fid->qid;
		break;
	case Qfeed:
		if(cl->current){
			snprint(buf, sizeof(buf), "%s/%s", logdir, cl->current->feed);
			print("%s\n", buf);
			f->fd = open(buf, 0644);
			f->foffset = 0;
		}
	}
	respond(r, nil);
}

static int
rootgen(int i, Dir *d, void *)
{
	i += Qroot+1;
	if(i < Qclients){
		fsmkdir(d, i, 0);
		return 0;
	}
	i -= Qclients;
	if(i < nclient){
		fsmkdir(d, Qclients, &client[i]);
		return 0;
	}
	// Final entry is just our services dir
	if(i == nclient){
		fsmkdir(d, Qservices, 0);
		return 0;
	}
	return -1;
}

static int
servicesgen(int i, Dir *d, void *)
{
	i += Qservices + 1;
	if(i < Qservice){
		fsmkdir(d, i, 0);
		return 0;
	}
	i -= Qservices + 2;
	if(i < nservice){
		fsmkdir(d, Qservice, &service[i]);
		return 0;
	}
	return -1;
}

static int
clientgen(int i, Dir *d, void *aux)
{
	// TODO: Mask the unusable files if we have no current buffer
	i += Qclients+1;
	if(i > Qinput)
		return -1;
	fsmkdir(d, i, aux);
	return 0;
}

static int
servicegen(int i, Dir *d, void *aux)
{
	i += Qservice+1;
print("%d %d\n", i, Qmax);
	if(i >= Qmax)
		return -1;
	fsmkdir(d, i, aux);
	return 0;
}

static void
fsread(Req *r)
{
	char buf[1024];
	Altfid *f;
	Client *cl;
	Service *svc;

	f = r->fid->aux;
	cl = f->client;
	svc = f->service;
	
	if(f->level > Qctl && f->level < Qservices && !cl->current){
		respond(r, "no current buffer selected");
		return;
	}

	switch(f->level){
	case Qroot:
print("Root\n");
		dirread9p(r, rootgen, nil);
		respond(r, nil);
		return;
	case Qclients:
print("Clients\n");
		dirread9p(r, clientgen, nil);
		respond(r, nil);
		return;
	case Qservices:
print("Services\n");
		dirread9p(r, servicesgen, nil);
		respond(r, nil);
		return;
	case Qservice:
print("Service\n");
		dirread9p(r, servicegen, nil);
		respond(r, nil);
		return;
	case Qtitle:
		snprint(buf, sizeof(buf), "%s\n", cl->current->title);
	String:
		readstr(r, buf);
		respond(r, nil);
		return;
	case Qctl: 
		snprint(buf, sizeof(buf), "%d\n", CLIENTID(f->client));
		goto String;
	case Qstatus:
		snprint(buf, sizeof(buf), "%s\n", cl->current->status);
		goto String;
	case Qaside:
		snprint(buf, sizeof(buf), "%s\n", cl->current->aside);
		goto String;
	case Qsctl:
		snprint(buf, sizeof(buf), "%s\n", svc->name);
		goto String;
	case Qfeed:
		pread(f->fd, buf, sizeof(buf), f->foffset);
		goto String;
	case Qsinput:
		// forward any pending input from client
		// TODO: Channel for input?
		break;
	case Qnotify:
		// TODO: notify fmt %N, install at start
		//snprint(buf, sizeof(buf), "%N\n", svc->notify);
		break;
	case Qtabs:
		// TODO: tabs fmt %T, install at start
		//snprint(buf, sizeof(buf), "%T\n", svc);
		goto String;
		
	}
	respond(r, "not implemented");
}

static void
fswrite(Req *r)
{
	int n;
	Altfid *f;
	char *s, *t;

	f = r->fid->aux;
	switch(f->level){
	case Qsctl:
	case Qctl:
		n = r->ofcall.count = r->ifcall.count;
		s = emalloc(n+1);
		memmove(s, r->ifcall.data, n);
		while(n > 0 && strchr("\r\n", s[n-1]))
			n--;
		s[n] = 0;
		// TODO: We don't use any of this in any meaningful way, remove t from calls
		t = s;
		while(*t && strchr(whitespace, *t)==0)
			t++;
		while(*t && strchr(whitespace, *t))
			*t++ = 0;
		if(f->level == Qctl)
			t = clientctl(f->client, s, t);
		else
			t = servicectl(f->service, s, t);
		free(s);
		respond(r, t);
		return;
	case Qinput:
		// TODO: User wrote a string to us, forward to server (cb?)
		//f->svc->callback(r->ifcall.data, r->ifcall.count);
		return;
	}
	respond(r, "not implemented");
}

static void
fsflush(Req *r)
{
	respond(r, nil);
}

static void
fsdestroyfid(Fid *fid)
{
	Altfid *f;

	if(f = fid->aux){
		fid->aux = nil;
		if(f->client)
			freeclient(f->client);
		// TODO: uncomment so services hold open an FD to show their livelihood
		//if(f->service)
		//	freeservice(f->service);
		free(f);
	}	
}

static void
fsstart(Srv*)
{
	/* Overwrite if we have one, force a reconnect of everything */
	if(mtpt != nil)
		unmount(nil, mtpt);
}

static void
fsend(Srv*)
{
	postnote(PNGROUP, getpid(), "shutdown");
	exits(nil);
}

Srv fs = 
{
	.start=fsstart,
	.attach=fsattach,
	.stat=fsstat,
	.walk1=fswalk1,
	.clone=fsclone,
	.open=fsopen,
	.read=fsread,
	.write=fswrite,
	.flush=fsflush,
	.destroyfid=fsdestroyfid,
	.end=fsend,
};

void
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
	user = getuser();
	mtpt = "/mnt/alt";
	logdir = "/tmp/alt";
	time0 = time(0);

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

	create(logdir, OREAD, DMDIR | 0755);
	postmountsrv(&fs, srvpt, mtpt, MCREATE);
	exits(nil); 
}
