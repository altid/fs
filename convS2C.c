#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>
#include "alt.h"

struct state
{
	Cmd cmd;
	char *base;
	int size;
	int (*fn)(struct state *s);	
};

static int parse_cmd(struct state *s);
static int parse_from(struct state *s);
static int parse_data(struct state *s);

Cmd*
convS2C(char *c)
{
	struct state s;
	Cmd cmd;

	s.fn = (*parse_cmd);
	s.cmd.type = ErrorCmd;
	strcpy(s.cmd.data, "An unknown error has occured");
	s.base = c;
	s.size = strlen(c);
	while((s.fn(&s) > 0));

	cmd.type = s.cmd.type;
	strcpy(cmd.buffer, s.cmd.buffer);
	strcpy(cmd.data, s.cmd.data);
	strcpy(cmd.svccmd, s.cmd.svccmd);

	return &cmd;
}

static int
parse_cmd(struct state *s)
{
	int n;

	n = 0;
	for(;;){
		if(n > s->size)
			return -1;
		switch(s->base[n]){
		case '\0':
		case '\r':
		case '\n':
			// Check we aren't at "quit"
			s->base[n] = '\0';
			s->cmd.type = CloneCmd;
			if(strncmp("quit", s->base, n) == 0)
				s->cmd.type = QuitCmd;
			else
				strcpy(s->cmd.data, s->base);
			return 0;
		case ' ':
		case '\t':
			// The overwhelming majority will be feed activity
			if(strncmp(s->base, "feed", 4) == 0)
				s->cmd.type = FeedCmd;
			else if(strncmp(s->base, "aside", 5) == 0)
				s->cmd.type = SideCmd;
			else if(strncmp(s->base, "nav", 3) == 0)
				s->cmd.type = NavCmd;
			else if(strncmp(s->base, "title", 5) == 0)
				s->cmd.type = TitleCmd;
			else if(strncmp(s->base, "image", 5) == 0)
				s->cmd.type = ImageCmd;
			else if(strncmp(s->base, "delete", 6) == 0)
				s->cmd.type = DeleteCmd;
			else if(strncmp(s->base, "remove", 6) == 0)
				s->cmd.type = RemoveCmd;
			else if(strncmp(s->base, "notification", 12) == 0)
				s->cmd.type = NotifyCmd;
			else if(strncmp(s->base, "error", 5) == 0)
				s->cmd.type = ErrorCmd;
			else if(strncmp(s->base, "status", 6) == 0)
				s->cmd.type = StatusCmd;
			else if(strncmp(s->base, "create", 6) == 0)
				s->cmd.type = CreateCmd;
			else {
				s->cmd.type = ServiceCmd;
				strncpy(s->cmd.svccmd, s->base, n);
			}
			s->size -= n;
			n++;
			s->base += n;
			s->fn = (*parse_from);
			return 1;
		default:
			n++;
		}
	}
}

static int
parse_from(struct state *s)
{
	int n;
	n = 0;
	for(;;){
		if(n > s->size || n > MaxBuflen)
			return -1;
		switch(s->base[n]){
		// leading spaces/tabs, ignore
		case ' ':
		case '\t':
			// NOTE: This moves our pointer forward so n++ isn't necessary
			s->size--;
			s->base++;
			break;
		case '\0':
			strncpy(s->cmd.buffer, s->base, n);
			return 0;
		case '\n':
		case '\r':
			s->base[n] = '\0';
			strncpy(s->cmd.buffer, s->base, n+1);
			// return if we have no data to parse
			if (n >= s->size)
				return 0;
			s->size -=n;
			n++;
			s->base += n;
			s->fn = (*parse_data);
			return 1;
		default:
			n++;
		}
	}
}

static int
parse_data(struct state *s)
{
	// We may eventually do some processing here
	strncpy(s->cmd.data, s->base, s->size);
	s->cmd.data[s->size] ='\0';
	return 0;	
}
