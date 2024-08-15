typedef struct Buffer Buffer;
typedef struct Notify Notify;
typedef struct Cmd Cmd;

enum
{
	CloneCmd,
	CreateCmd,
	DeleteCmd,
	RemoveCmd,
	NotifyCmd,
	ErrorCmd,
	StatusCmd,
	SideCmd,
	NavCmd,
	TitleCmd,
	ImageCmd,
	FeedCmd,
	QuitCmd,
	ServiceCmd,
	InputCmd,
	FlushCmd,
	BufferCmd,
	MarkdownCmd,

	// TODO: Move data to the stack
	MaxBuflen = 128,
	MaxDatalen = 1024,
	CmdSize = MaxBuflen * 2 + 1,
};

struct Buffer
{
	QLock       l;
	char	name[MaxBuflen];
	char	title[512];
	char	status[512];
	char	*aside;
	int	fd;	// feed
	int	tag;	// feed
	int	unread;
	Channel	*cmds;
	Channel   *input;
	Notify	*notify;
	Buffer	*next;
	Rendez	rz;
};

struct Notify
{
	char	*data;
	Notify	*next;
};

struct Cmd
{
	// Potentially big
	int	type;
	char	buffer[MaxBuflen];
	char	svccmd[MaxBuflen];
	char	*data;
};

Buffer *bufferCreate(Channel*, Channel*);
Buffer *bufferSearch(Buffer*, char*);
Buffer *bufferSearchTag(Buffer*, ulong);
char *bufferDrop(Buffer*, char*);
char *bufferPush(Buffer*, char*);
void bufferDestroy(Buffer*);

int Tconv(Fmt*);
int Nconv(Fmt*);
int Cconv(Fmt*);

void* emalloc(int);
char* estrdup(char*);

char *mtpt;
char *srvpt;
char *user;
char *logdir;
int debug;

uint convS2C(Cmd*, char*, uint);

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
