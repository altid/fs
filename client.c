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
	int	tag;
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
static int flushtag;
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
	if(cl->current)
		cl->current->tag = 0;
	cl->showmarkdown = 0;
	cl->fd = -1;

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
	char path[1024];

	f = mallocz(sizeof(*f), 1);
	f->cl = newclient(r->ifcall.aname);

	/* Attach request has buffer */
	if(r->ifcall.aname[0]){
		memset(path, 0, sizeof(path));
		snprint(path, sizeof(path), "%s/%s/%s", logdir, root->name, r->ifcall.aname);
		f->cl->fd = open(path, OREAD);
	}
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
	f = mallocz(sizeof(*f), 1);
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
	n = 0;
	switch(f->level){
	case Qcroot:
		dirread9p(r, rootgen, f->cl);
		respond(r, nil);
		return;
	case Qfeed:
		// Catch EOF
		if(f->cl->fd < 0 || !f->cl->current){
			r->ofcall.count = 0;
			r->ofcall.data[0] = 0;
			respond(r, nil);
			return;
		}
		srvrelease(fs);
		b = f->cl->current;
Again:
		// Check if we have a tag here, abort early if so.
		if(b->tag != flushtag){
			memset(buf, 0, sizeof(buf));
			n = pread(f->cl->fd, buf, sizeof(buf), r->ifcall.offset);
			if(n > 0){
				// cut off the EOF
			 	buf[n] = 0;
				readstr(r, buf);
			} else {
				qlock(&b->l);
				b->unread = 0;
				rsleep(&b->rz);
				qunlock(&b->l);
				goto Again;
			}
		} else
			flushtag = -1;
		srvacquire(fs);
		respond(r, nil);
		return;
	case Qtitle:
		if(f->cl->current && f->cl->current->title){
			b = f->cl->current;
			memset(buf, 0, sizeof(buf));
			qlock(&b->l);
			snprint(buf, sizeof(buf), "%s", b->title);
			qunlock(&b->l);
String:
			readstr(r, buf);
			respond(r, nil);
			return;	
		}
		break;
	case Qstatus:
		if(f->cl->current && f->cl->current->status){
			b = f->cl->current;
			memset(buf, 0, sizeof(buf));
			qlock(&b->l);
			snprint(buf, sizeof(buf), "%s",b->status);
			qunlock(&b->l);
			goto String;
		}
		break;
	case Qaside:
		if(f->cl->current && f->cl->current->aside){
			b = f->cl->current;
			memset(buf, 0, sizeof(buf));
			qlock(&b->l);
			snprint(buf, sizeof(buf), "%s", b->aside);
			qunlock(&b->l);
			goto String;
		}
		break;
	case Qnotify:
		if(f->cl->current && f->cl->current->notify){
			b = f->cl->current;
			memset(buf, 0, sizeof(buf));
			qlock(&b->l);
			for(np = b->notify; np; np = np->next)
				n = snprint(buf + n, sizeof(buf), "!%s\n", np->data);
			qunlock(&b->l);
			goto String;
		}
		break;
	case Qtabs:
		qlock(&root->l);
		memset(buf, 0, sizeof(buf));
		for(b = root->next; b; b = b->next){
			n += snprint(buf + n, sizeof(buf) - n, "%N\n", b);
		}
		qunlock(&root->l);
		goto String;
	}
	if(!f->cl->current)
		respond(r, "no buffer selected");
	else
		respond(r, "no data available");
}

void
clwrite(Req *r)
{
	Buffer *b;
	Clfid *f;
	Cmd cmd;
	char path[1024];

	f = r->fid->aux;
	memset(&cmd, 0, sizeof(Cmd));
	switch(f->level){
	case Qctl:
		convS2C(&cmd, r->ifcall.data, r->ifcall.count);
		switch(cmd.type){
		case BufferCmd:
			b = bufferSearch(root, cmd.buffer);
			if(!b){
				respond(r, "No buffers available");
				return;
			}
			qlock(&b->l);
			if(f->cl->fd > 0){
				b->tag = flushtag = 1;
				rwakeup(&b->rz);
				close(f->cl->fd);
			}
			f->cl->current = b;
			b->tag = r->tag;
			qunlock(&b->l);
			memset(path, 0, sizeof(path));
			snprint(path, sizeof(path), "%s/%s/%s", logdir, root->name, cmd.buffer);
			f->cl->fd = open(path, OREAD);
			r->fid->aux = f;
			goto Out;
		case MarkdownCmd:
			f->cl->showmarkdown = !f->cl->showmarkdown;
			goto Out;
		default:
			sendp(root->cmds, &cmd);
			sendp(root->cmds, nil);
			goto Out;
		}
	case Qinput:
		r->ofcall.count = 0;
		if(f->cl->current == nil){
			respond(r, "No buffer selected");
			return;
		}
		cmd.type = InputCmd;
		strcpy(cmd.buffer, f->cl->current->name);
		cmd.data = strdup(r->ifcall.data);
		cmd.data[r->ifcall.count] = 0;
		sendp(root->cmds, &cmd);
		sendp(root->cmds, nil);
	}
Out:
	respond(r, nil);
	return;
}

void
clflush(Req *r)
{
	Buffer *b;
	flushtag = r->tag;
	if(b = bufferSearchTag(root, flushtag)){
		qlock(&b->l);
		rwakeup(&b->rz);
		qunlock(&b->l);
	}
	respond(r, nil);
}

void
cldestroyfid(Fid *fid)
{
	Clfid *f;

	if(f = fid->aux){
		// TODO: Uncomment once we use this in aux/listen
		//fid->aux = nil;
		//if(f->cl)
		//	freeclient(f->cl);
	}
	free(f);
}

void
clstart(Srv *s)
{
	// TODO: Set up note handler
	root = emalloc(sizeof(*root));
	USED(root);
	root = (Buffer*)s->aux;
	flushtag = -1;
	fs = s;
	time0 = time(0);
}

void
clend(Srv*)
{
	exits(nil);
}
