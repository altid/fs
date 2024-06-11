typedef struct Buffer Buffer;
typedef struct Notify Notify;

struct Buffer
{
	char	title[256];
	char	status[256];
	char	*aside;
	int	fd;
	Notify	*notify;
	// callback function from server for processing input
	Buffer	*next;
};

struct Notify
{
	char	*data;
	Notify	*next;
};

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
