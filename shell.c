#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SIZE 2048
#define MAX_ARGS 20

struct command {
	char *name;
	char *argv[MAX_ARGS];
	int fd[2];
};

struct pipeline {
	int count;
	struct command *cmd[];
};

static char *input;
int background = 0;
pid_t pid;
int status;
int pipe_num;
int redirection = 0;


int built_in(struct command *cmd) {

	if (strcmp(cmd->name, "exit") == 0) {
		exit(0);
	} else if (strcmp(cmd->name, "cd") == 0) {
		chdir(cmd->argv[1]);
		return 1;
	}

	return 0;
}


char *read_input() {

	int buffsize = SIZE;
	char *input = malloc(buffsize * sizeof(char));
	int i = 0;
	char c;

	if (input == NULL) {
		fprintf(stderr, "error: malloc failed\n");
		exit(1);
	}


	while (1) {

		c = getchar();

		if (c == EOF) {
			free(input);
			return NULL;
		}

		if (c == '\n') {
			input[i]='\0';
			return input;
		}

		if (c == '&') {
			background = 1;
			continue;
		}

		if (i >= buffsize) {
			buffsize = 2 * buffsize;
			input = realloc (input, buffsize);
		}

		input[i++] = c;

	}
}

void redirect (int direction, char *filename, char *mode) {
	if (direction == 1) {
		freopen(filename, mode, stdout);
	} else if (direction == 0) {
		freopen(filename, mode, stdin);
	} else if (direction == 2) {
		freopen(filename, mode, stderr);
	}
}


int tokenCheck(char *token) {

	if (strcmp(token, ">") == 0) {
		token = strtok(NULL, " ");
		redirect(1, token, "w");
		return 1;
	} else if (strcmp(token, ">>") == 0) {
		token = strtok(NULL, " ");
		redirect(1, token, "a");
		return 1;
	} else if (strcmp(token, "<") == 0) {
		token = strtok(NULL, " ");
		redirect(0, token, "r");
		return 1;
	} else if (strcmp(token, "2>") == 0) {
		token = strtok(NULL, " ");
		redirect(2, token, "w");
		return 1;
	}
	return 0;
}

struct command *get_cmd (char *input) {
	struct command *cmd = malloc(sizeof(struct command) + MAX_ARGS * sizeof(char *));
	int i = 0;
	char *token;

	token = strtok(input, " ");
	while (token != NULL) {
		if (tokenCheck(token)) {
			redirection = 1;
			break;
		}

		cmd->argv[i++] = token;

		if (i > MAX_ARGS) {
			fprintf(stderr, "Error! The maximum number of arguments is %d\n", MAX_ARGS);
			exit(1);
		}
		token = strtok(NULL, " ");
	}

	cmd->name = cmd->argv[0];
	cmd->argc = i;

	return cmd;

}

struct pipeline *get_cmd_with_pipes (char *input) {
	int i = 0;
	char *single_cmd;
	char *ptr;
	struct pipeline *pipeline = malloc(sizeof(struct pipeline) + sizeof(struct command)*MAX_CMDS);

	single_cmd = strtok_r(input, "|", &ptr);
	while (single_cmd != NULL) {
		pipeline->cmd[i++] = get_cmd(single_cmd);
		single_cmd = strtok_r(NULL, "|", &ptr);
	}

	pipeline->count = i;

	return pipeline;
}

void close_pipes(int (*pipes)[2]) {
	int i;

	for (i = 0; i < pipe_num; i++) {
		close(pipes[i][0]);
		close(pipes[i][1]);
	}
}

int exec_cmd (struct command *cmd, int (*pipes)[2]) {
	pid = fork();
	if (pid == 0) {

		int input_fd = cmd->fd[0];
		int output_fd = cmd->fd[1];

		if (input_fd != -1 && input_fd != STDIN_FILENO) {
			dup2(input_fd, STDIN_FILENO);
		}

		if (output_fd != -1 && output_fd != STDOUT_FILENO) {
			dup2(output_fd, STDOUT_FILENO);
		}

		if (pipes != NULL) {
			close_pipes(pipes);
		}

		execvp(cmd->name, cmd->argv);

	}
	return 0;
}


void exec_pipe (struct pipeline *pipeline) {
	int i = pipeline->count;
	if (i == 1) {
		pipeline->cmd[0]->fd[0]=STDIN_FILENO;
		pipeline->cmd[0]->fd[1]=STDOUT_FILENO;
		exec_cmd(pipeline->cmd[0], NULL);
		if (background == 1) {
			waitpid(pid, &status, WNOHANG);
			background = 0;
		} else {
			waitpid(pid, &status, NULL);
		}

	} else {
		pipe_num = i - 1;
		int (*pipes)[2] = calloc(pipe_num * sizeof(int[2]), 1);
		int j;

		pipeline->cmd[0]->fd[0] = STDIN_FILENO;
		for (j = 1; j < i; j++) {
			pipe(pipes[j-1]);
			pipeline->cmd[j-1]->fd[1] = pipes[j-1][1];
			pipeline->cmd[j]->fd[0] = pipes[j-1][0];
		}
		pipeline->cmd[pipe_num]->fd[1] = STDOUT_FILENO;

		for (j = 0; j < i; j++) {
			exec_cmd(pipeline->cmd[j], pipes);
		}

		close_pipes(pipes);

		for (j = 0; j < i; ++j) {
			waitpid(pid, &status, NULL);
		}
	}

}


char *get_dir() {
	char *path = getenv("PWD");
	char *basec = strdup(path);
	return basename(basec);
}

int main() {
	char *dir;
	while (1) {
		dir = get_dir();
		printf("[%s@%s %s]#", getenv("USERNAME"), getenv("HOSTNAME"), dir);

		input = read_input();
		struct pipeline *pipeline = get_cmd_with_pipes(input);

		if (input == NULL) {
			printf("finalización del programa");
			exit(0);
		} else if ((strcmp(input, "") == 0)  || built_in(pipeline->cmd[0])) { //No se admiten built-in en pipelines
			continue;
		}

		//exec_cmd(cmd);
		exec_pipe(pipeline);


		if (redirection == 1) {
			freopen("/dev/tty", "w", stdout);
			freopen("/dev/tty", "r", stdin);
			freopen("/dev/tty", "w", stderr);
			redirection = 0;
		}
	}
	exit(0);
}
