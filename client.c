#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>

#include "alt.h"

Client*
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

void
freeclient(Client *cl)
{
	if(cl == nil || cl->ref == 0)
		return;
	cl->ref--;
	if(cl->ref > 0)
		return;

	memset(cl, 0, sizeof(*cl));
}

char*
clientctl(Client *cl, char *ctl, char *arg)
{
	USED(cl);
	print("Command in: %s\nArgs in: %s\n", ctl, arg);
	if(strcmp(ctl, "buffer") == 0){

	} else{
		// Clients should be polling for commands and input alike
		
	}
	return nil;
}
