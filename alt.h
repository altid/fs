typedef struct Buffer Buffer;
typedef struct Notify Notify;

struct Buffer
{
	QLock       l;
	char	*name;
	char	title[1024];
	char	status[1024];
	char	aside[1024];
	int	fd;	// feed
	int	tag;	// feed
	int	unread;
	Channel	*cmds;
	Notify	*notify;
	Buffer	*next;
	Rendez	rz;
};

struct Notify
{
	char	*data;
	Notify	*next;
};

Buffer *bufferCreate(Channel*);
Buffer *bufferSearch(Buffer*, char*);
Buffer *bufferSearchTag(Buffer*, ulong);
char *bufferDrop(Buffer*, char*);
char *bufferPush(Buffer*, char*);
void bufferDestroy(Buffer*);

int Tconv(Fmt*);
int Nconv(Fmt*);

void* emalloc(int);
char* estrdup(char*);

char *mtpt;
char *srvpt;
char *user;
char *logdir;
int debug;

void clattach(Req*);
void clstat(Req*);
char *clwalk1(Fid*, char*, Qid*);
char *clclone(Fid*, Fid*);
void clopen(Req*);
void clread(Req*);
void clwrite(Req*);
void clflush(Req*);
void cldestroyfid(Fid*);
void clstart(Srv*);
void clend(Srv*);

void svcattach(Req*);
void svcstat(Req*);
char *svcwalk1(Fid*, char*, Qid*);
char *svcclone(Fid*, Fid*);
void svcopen(Req*);
void svcread(Req*);
void svcwrite(Req*);
void svcflush(Req*);
void svcdestroyfid(Fid*);
void svcstart(Srv*);
void svcend(Srv*);
