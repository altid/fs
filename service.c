#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>

#include "alt.h"

Service*
newservice(void)
{
	Service *sv;
	char buf[1024];
	
	sv = &service[nservice];
	nservice++;

	sv->buffer = nil;
	sv->notifications = nil;
	snprint(buf, sizeof(buf), "default");
	sv->name = estrdup(buf);
	
	return sv;
}

void
freeservice(Service *s)
{
	if(s == nil)
		return;
	memset(s, 0, sizeof(*s));
}

char*
servicectl(Service *svc, char *ctl, char *arg)
{
	char *target, *buffer;
	USED(svc, ctl, arg);
	//ctl is like title:##meskarune
	target = strstr(":",  ctl);
	buffer = strstr(nil, ctl);
	print("%s and %s\n", buffer, target);
	return nil;
}
