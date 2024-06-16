typedef struct Buffer Buffer;
typedef struct Notify Notify;

struct Buffer
{
	char	*name;
	char	*title;
	char	*status;
	char	*aside;
	int	fd;
	Notify	*notify;

	Buffer	*next;

	// Passed by the service
	void	(*input)(char*);
	char	*(*ctl)(char*, char*);
};

struct Notify
{
	char	*data;
	Notify	*next;
};

Buffer *bufferCreate(void(*fn)(char*), char*(*fn)(char*, char*));
Buffer *bufferSearch(Buffer*, char*);
char *bufferDrop(Buffer*, char*);
char *bufferPush(Buffer*, char*);
void bufferDestroy(Buffer*);

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
