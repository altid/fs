#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>

#include "alt.h"

int
Nconv(Fmt *fp)
{
	Notify *n;

	n = va_arg(fp->args, Notify*);
	return fmtstrcpy(fp, n->data);
}
