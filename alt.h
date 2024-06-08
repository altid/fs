#define	CLIENTID(c)	((int)(((Client*)(c)) - client))

typedef struct Buffer Buffer;
typedef struct Notify Notify;
typedef struct Service Service;
typedef struct Client Client;

struct Buffer
{
	char	title[256];
	char	status[256];
	char	feed[256];
	char	*aside;
};

struct Notify
{
	char	*data;
	Notify	*next;
};

struct Service
{
	Buffer	*buffer;
	Notify	*notifications;
	//input callback function here
	char	*name;
	Service	*next;
};

struct Client
{
	Buffer	*current;
	int	showmarkdown;
	int	ref;
};

Client* newclient(void);
void freeclient(Client*);
char* clientctl(Client*, char*, char*);
Service* newservice(void);
void freeservice(Service*);
char* servicectl(Service*, char*, char*);
void* emalloc(int);
char* estrdup(char*);

char *mtpt;
char *srvpt;
char *user;
long time0;
char *logdir;
Client client[256];
int nclient;
Service service[64];
int nservice;
int debug;
