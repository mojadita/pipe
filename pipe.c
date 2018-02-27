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
			F("close("#expr" === %d);\n"), \
			(expr));\
	}while(0)

#define PIPE(var) do{\
		if(pipe(var)<0) ERR("pipe"); \
		fprintf(stderr,F("PIPE ==> %d, %d\n"), var[0], var[1]);\
	}while(0)

#define FORK() do{\
		if((res = fork()) < 0)ERR("fork");\
		fprintf(stderr,F("FORK ==> %d\n"), res);\
	}while(0)

#define DUP(expr1, expr2) do{\
		if((res = dup2(expr1, expr2)) < 0) ERR("dup");\
		fprintf(stderr,\
			F("DUP("#expr1" === %d, "#expr2" === %d);\n"),\
			(expr1), (expr2));\
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

static size_t printus(int ix, struct pipe_program *p);
static pid_t WAIT(int *status);

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
			printus(i, p);

			/* redirect input, if needed */
			if (input_fd != 0) {
				DUP(input_fd, 0);
				CLOSE(input_fd); /* not used after redirection */
			}

			CLOSE(pipe_fds[0]); /* we don't use this */

			/* and output */
			DUP(pipe_fds[1], 1);
			CLOSE(pipe_fds[1]);

			execvp(p->pname, p->argv);

			ERR("execvp: %s", p->pname);
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
		printus(i, p);

		/* redirect input */
		if (input_fd != 0) {
			DUP(input_fd, 0);
			CLOSE(input_fd); /* not used after_redirection */
		}

		/* this time no output redirection */

		execvp(p->pname, p->argv);

		ERR("execvp");
		/* NOTREACHED */
	}

	CLOSE(pipe_fds[1]);
	if (input_fd) CLOSE(input_fd);

	/* parent code... we did pipe_programs_n fork()'s so we
	 * have to do pipe_programs_n wait()'s */
	int status;
	pid_t cld_pid;
	/* while we can wait for a child */
	while ((cld_pid = WAIT(&status)) > 0) {
		for (i = 0, p = pipe_programs; i < pipe_programs_n; i++, p++) {
			if (cld_pid == p->pid) {
				fprintf(stderr,
					F("Child finished: pid = %d\n"),
					cld_pid);
				printus(i, p);
				break;
			}
		}
	} /* while */
	exit(EXIT_SUCCESS);
}
       
static size_t printus(int ix, struct pipe_program *p)
{
	size_t res = 0;
	int j;

	res += printf(
		F("%d: pid = %d, ppid = %d, program = \"%s\": args = "),
		ix, p->pid, p->ppid, p->pname);
	for (j = 0; p->argv[j]; j++)
		res += printf(
			"%s\"%s\"",
			j ? ", " : "{",
			p->argv[j]);
	res += printf("};\n");

	return res;
}

static pid_t WAIT(int *status)
{
	pid_t res = wait(status);
	fprintf(stderr, F("WAIT() ==> %d\n"), res);
	return res;
}
