#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>
#include "alt.h"

// TODO: Read runes instead of ascii

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

uint
convS2C(Cmd *cmd, char *c, uint n)
{
	struct state s;

	s.fn = (*parse_cmd);
	s.cmd.type = ErrorCmd;
	s.base = c;
	s.size = n;
	while((s.fn(&s) > 0));

	cmd->type = s.cmd.type;
	cmd->data = s.cmd.data;
	strcpy(cmd->buffer, s.cmd.buffer);
	strcpy(cmd->svccmd, s.cmd.svccmd);
	return sizeof(cmd);
}

static int
parse_cmd(struct state *s)
{
	int n = 0;
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
				s->cmd.data = strdup(s->base);
			return 0;
		case ' ':
		case '\t':
			// The overwhelming majority will be feed activity
			if(strncmp(s->base, "feed", 4) == 0)
				s->cmd.type = FeedCmd;
			else if(strncmp(s->base, "asi", 3) == 0)
				s->cmd.type = SideCmd;
			else if(strncmp(s->base, "nav", 3) == 0)
				s->cmd.type = NavCmd;
			else if(strncmp(s->base, "titl", 4) == 0)
				s->cmd.type = TitleCmd;
			else if(strncmp(s->base, "ima", 3) == 0)
				s->cmd.type = ImageCmd;
			else if(strncmp(s->base, "del", 3) == 0)
				s->cmd.type = DeleteCmd;
			else if(strncmp(s->base, "rem", 3) == 0)
				s->cmd.type = RemoveCmd;
			else if(strncmp(s->base, "noti", 4) == 0)
				s->cmd.type = NotifyCmd;
			else if(strncmp(s->base, "err", 3) == 0)
				s->cmd.type = ErrorCmd;
			else if(strncmp(s->base, "stat", 4) == 0)
				s->cmd.type = StatusCmd;
			else if(strncmp(s->base, "crea", 4) == 0)
				s->cmd.type = CreateCmd;
			else if(strncmp(s->base, "buff", 4) == 0)
				s->cmd.type = BufferCmd;
			else if(strncmp(s->base, "mark", 4) == 0)
				s->cmd.type = MarkdownCmd;
			else {
				s->cmd.type = ServiceCmd;
				snprint(s->cmd.svccmd, n, s->base);
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
	int n = 0;
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
		case 0:
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
	int n = 0;
	char c;

	for(;;) {
		if(n >= s->size || n >= MaxBuflen)
			return -1;
		c = s->base[n];
		switch(c){
		// Useful control chars
		case '\n':
		case '\t':
		case '\r':
			break;
		default:
			// Anything under a space is an unhandled control char
			if(c < ' '){
				if(n > 0 ){	
					s->cmd.data = strdup(s->base);
					s->cmd.data[n] = 0;
				}
				return 0;
			}
		}
		n++;
	}
}
