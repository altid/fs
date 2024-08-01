#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>

#include "alt.h"

static void
bufferFree(Buffer *match)
{
	if(match->name)
		free(match->name);
	free(match);
}

char *
bufferDrop(Buffer *base, char *name)
{
	Buffer *mp, *bp;
	qlock(&base->l);
	for(bp = base; bp->next; bp = bp->next){
		mp = bp->next;
		if(strcmp(bp->next->name, name) == 0){
			if(mp && mp->next)
				bp->next = mp->next;
			else
				bp->next = nil;
			if(mp)
				bufferFree(mp);
		}
	}
	qunlock(&base->l);			
	return nil;
}

char *
bufferPush(Buffer *base, char *name)
{
	Buffer *b, *ep;
	char p[1024];

	qlock(&base->l);
	for(ep = base; ep->next; ep = ep->next){
		if(ep && strcmp(ep->name, name) == 0){
			qunlock(&base->l);
			return "buffer exists";
		}
		if(ep->next == nil)
			break;
	}
	
	b = mallocz(sizeof(*b), 1);
	b->name = estrdup(name);
	b->notify = nil;
	b->unread = 0;
	b->tag = -1;
	b->rz.l = &b->l;
	memset(b->title, 0, sizeof(b->title));
	memset(b->status, 0, sizeof(b->status));
	memset(b->aside, 0, sizeof(b->aside));
	snprint(p, sizeof(p), "%s/%s/%s", logdir, base->name, name);
	if(access(p, 0) == 0)
		b->fd = open(p, OWRITE);
	else
		b->fd = create(p, OWRITE, 0644);
	seek(b->fd, 0, 2);
	ep->next = b;
	qunlock(&base->l);
	return nil;
}

Buffer *
bufferSearch(Buffer *base, char *name)
{
	Buffer *sp;
	qlock(&base->l);
	for(sp = base; sp; sp = sp->next)
		if(strcmp(sp->name, name) == 0){
			qunlock(&base->l);
			return sp;
		}
	qunlock(&base->l);
	return nil;
}

Buffer *
bufferSearchTag(Buffer *base, ulong tag)
{
	Buffer *sp;
	qlock(&base->l);
	for(sp = base; sp; sp = sp->next)
		if(sp->tag == tag){
			qunlock(&base->l);
			return sp;
		}
	qunlock(&base->l);
	return nil;
}

Buffer*
bufferCreate(Channel *cmds)
{
	Buffer *b;

	b = mallocz(sizeof(*b), 1);
	b->name = nil;
	memset(b->title, 0, sizeof(b->title));
	memset(b->status, 0, sizeof(b->status));
	memset(b->aside, 0, sizeof(b->aside));
	b->cmds = cmds;
	b->tag = -1;
	b->unread = 0;
	b->notify = nil;
	b->next = nil;
	b->rz.l = &b->l;

	return b;
}
