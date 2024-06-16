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
				
	return nil;
}

char *
bufferPush(Buffer *base, char *name)
{
	Buffer *b, *ep;
	char p[1024];

	for(ep = base; ep->next; ep = ep->next){
		if(ep && strcmp(ep->name, name) == 0)
			return "buffer exists";
		if(ep->next == nil)
			break;
	}
	
	b = emalloc(sizeof(*b));
	b->name = estrdup(name);
	b->notify = nil;
	b->input = base->input;
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

	return nil;
}

Buffer *
bufferSearch(Buffer *base, char *name)
{
	Buffer *sp;

	for(sp = base; sp; sp = sp->next)
		if(strcmp(sp->name, name) == 0)
			return sp;
	return nil;
}

Buffer*
bufferCreate(void (*input)(char*), char *(*ctl)(char*, char*))
{
	Buffer *b;

	b = emalloc(sizeof(*b));
	b->name = nil;;
	b->input = input;
	b->ctl = ctl;
	b->notify = nil;
	b->next = nil;

	return b;
}

