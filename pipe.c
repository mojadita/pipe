#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define F(fmt) "[pid=%d]:"__FILE__":%d:%s: " fmt, getpid(), __LINE__, __func__

#define SIZE(a) (sizeof a/sizeof *a)

#define ERR(fmt, ...) do{\
		fprintf(stderr, \
			F(fmt": %s (errno = %d)\n"),\
			##__VA_ARGS__,\
			strerror(errno),\
			errno);\
		exit(EXIT_FAILURE);\
	}while(0)

#define CLOSE(expr) do{\
		close(expr);\
		fprintf(stderr, \
			F("CLOSE("#expr" ==> %d);\n"), \
			(expr));\
	}while(0)

#define PIPE(var) do{\
		if(pipe(var)<0) ERR("pipe"); \
		fprintf(stderr,F("PIPE ==> {%d, %d};\n"), var[0], var[1]);\
	}while(0)

#define FORK() do{\
		if((res = fork()) < 0)ERR("fork");\
		fprintf(stderr,F("FORK ==> %d%s\n"), res, res ? " (PARENT)" : " (CHILD)");\
	}while(0)

#define DUP(expr1, expr2) do{\
		if((res = dup2(expr1, expr2)) < 0) ERR("dup");\
		fprintf(stderr,\
			F("DUP("#expr1" ==> %d, "#expr2" ==> %d);\n"),\
			(expr1), (expr2));\
	}while(0)

#define EXEC(prog,argv) do{\
		fprintf(stderr,\
			F("EXEC("#prog", "#argv");\n"));\
		execvp(prog,argv);\
		ERR("exec(\"%s\")", prog);\
	}while(0)

/* INCLUDE THE DEFINITIONS FOR THE PROGRAM ARGUMENTS LISTS */

#define prog(nam, var, ...) char * argv_##nam[] = { var, ##__VA_ARGS__, NULL };
#include "pipe.i"
#undef prog

/* INCLUDE THE PIPING PROGRAM DESCRIPTIONS */
struct pipe_program {
	pid_t pid;
	pid_t ppid;
    char *pname; 
    char **argv;
} pipe_programs[] = {
#define prog(nam, var, ...) 0, 0, var, argv_##nam,
#include "pipe.i"
#undef prog
};

/* size of last array */
size_t pipe_programs_n = SIZE(pipe_programs);

struct signal_entry {
	int		snum;
	char	*snam;
	char 	*sdesc;
} signal_table[] = {
#define signal(num, val, desc) val, #num, ""#num"-"#val": "desc,
#include "signal.i"
#undef signal
	0, NULL, NULL,
};

static size_t procinfo(int ix, struct pipe_program *p);
static pid_t WAIT(void);

int main()
{
    int res, i;
    struct pipe_program *p = pipe_programs;
	int input_fd = 0; /* first process is connected to standard input */
    static int pipe_fds[2] = { -1, -1 };

	for(i = 0; i < pipe_programs_n - 1; i++, p++) {

		PIPE(pipe_fds);

		FORK();

		if (res == 0) { /* child process, we have redirected nothing yet. */

			p->pid = getpid();
			p->ppid = getppid();

			/* print who we are */
			procinfo(i, p);

			/* redirect input, if needed */
			if (input_fd != 0) {
				DUP(input_fd, 0);
				CLOSE(input_fd); /* not used after redirection */
			}

			CLOSE(pipe_fds[0]); /* we don't use this */

			/* and output */
			DUP(pipe_fds[1], 1);
			CLOSE(pipe_fds[1]);

			EXEC(p->pname, p->argv);

			/* NOTREACHED */

		}
		/* parent process */

		/* save pid to be used later */
		p->pid = res; /* we'll use it later */
		p->ppid = getpid();

		/* close unused pipe descriptor */
		CLOSE(pipe_fds[1]);

		/* if we have an old input_fd, then close it */
		if (input_fd) CLOSE(input_fd);

		/* ... and save pipe read descriptor */
		input_fd = pipe_fds[0];
	} /* for */

	/* now we have our input connected to the output of the last process */
	FORK();
	if (res == 0) { /* child, last process in the pipe */

		p->pid = getpid();
		p->ppid = getppid();

		/* print who we are */
		procinfo(i, p);

		/* redirect input */
		if (input_fd != 0) {
			DUP(input_fd, 0);
			CLOSE(input_fd); /* not used after_redirection */
		}

		/* this time no output redirection */

		EXEC(p->pname, p->argv);

		/* NOTREACHED */
	}

	CLOSE(pipe_fds[1]);
	if (input_fd) CLOSE(input_fd);

	/* parent code... we did pipe_programs_n fork()'s so we
	 * have to do pipe_programs_n wait()'s */
	/* while we can wait for a child */
	while (WAIT() >= 0)
		continue;

	fprintf(stderr,
		F("Normal exit.\n"));

	exit(EXIT_SUCCESS);
}
       
static size_t procinfo(int ix, struct pipe_program *p)
{
	size_t res = 0;
	int j;
    static char buffer[1024];
    char *s = buffer;
    size_t bfsz = sizeof buffer;
    size_t n;

#define ACT() do{s+=n; bfsz-=n;}while(0)

	n = snprintf(s, bfsz,
		F("%d: pid = %d, ppid = %d, program = \"%s\": args = "),
		ix, p->pid, p->ppid, p->pname);
    ACT();
	for (j = 0; p->argv[j]; j++) {
		n = snprintf(s, bfsz,
			"%s\"%s\"",
			j ? ", " : "{",
			p->argv[j]);
        ACT();
    }
	n = snprintf(s, bfsz, ", NULL};\n");
    ACT();
    fputs(buffer, stderr);

	return res;
}

char *signal2str(int sig)
{
	int i;
	struct signal_entry *p;
	for (i = 0, p = signal_table; p->snum; i++, p++) {
		if (p->snum && p->snum == sig)
			break;
	}
	return p->sdesc ? p->sdesc : "UNKOWN SIGNAL";
}

static pid_t WAIT(void)
{
	int status;
	pid_t pid = wait(&status);
	struct pipe_program *p;
	int i;

	if (pid < 0) return pid;

	fprintf(stderr,
		F("WAIT() ==> PID <== %d, STATUS <== 0x%08x\n"),
			pid, status);
	for (i = 0, p = pipe_programs; i < pipe_programs_n; i++, p++)
		if (pid == p->pid) break;
	if (i < pipe_programs_n) {
		procinfo(i, p);
	} else {
		fprintf(stderr,
			F("Process PID=%d not found\n"),
			pid);
	}
	if (WIFEXITED(status)) {
		fprintf(stderr,
			F("Exit STATUS = %d\n"),
			WEXITSTATUS(status));
	}
	if (WIFSIGNALED(status)) {
		fprintf(stderr,
			F("Child Killed by SIGNAL = %s%s\n"),
			signal2str(WTERMSIG(status)),
			WCOREDUMP(status) ? " (core dumped)" : "");
	}
	return pid;
}
