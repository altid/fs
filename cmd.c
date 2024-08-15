#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>

#include "alt.h"

int
Cconv(Fmt *fp)
{
	char s[CmdSize];
	Cmd *c;

	c = va_arg(fp->args, Cmd*);
	switch(c->type){
	case ServiceCmd:
		if(c->data != nil)
			snprint(s, sizeof(s), "%s %s\n%s", c->svccmd, c->buffer, c->data);
		else
			snprint(s, sizeof(s), "%s %s", c->svccmd, c->buffer);
		break;
	case InputCmd:
		snprint(s, sizeof(s), "input %s\n%s", c->buffer, c->data);
		break;
	case FlushCmd:
		snprint(s, sizeof(s), "flush");
		break;
	}
	return fmtstrcpy(fp, s);
}
