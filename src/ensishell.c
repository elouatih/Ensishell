/*****************************************************
 * Copyright Grégory Mounié 2008-2015                *
 *           Simon Nieuviarts 2002-2009              *
 * This code is distributed under the GLPv3 licence. *
 * Ce code est distribué sous la licence GPLv3+.     *
 *****************************************************/

#include "ensishell.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <glob.h>
#include "variante.h"
#include "readcmd.h"

#ifndef VARIANTE
#error "Variante non défini !!"
#endif

/* Guile (1.8 and 2.0) is auto-detected by cmake */
/* To disable Scheme interpreter (Guile support), comment the
 * following lines.  You may also have to comment related pkg-config
 * lines in CMakeLists.txt.
 */

glob_t globbuf;

int len(char **cmd){
        int l = 0;
        char **ptr = cmd;
        while (*(ptr)!=NULL){
                l++;
                ptr++;
        }
        return l;
}

int in(char c, char* str){
        for (int i=0; i<strlen(str); i++){
                if (c == str[i]){
                        return 1;
                }
        }
        return 0;
}

int count_jokers(char **cmd, int n){
        int c = 0;
        for (int i=0; i<n; i++){
                if(in('*', cmd[i]) || in('{', cmd[i]) || in('~', cmd[i])) c++;
        }
        return c;
}

char **traite_jokers(char **cmd, int *longueur){
        int n = len(cmd);
        int n_jokers = count_jokers(cmd, n);
        int n_no_jokers = n - n_jokers;
        //printf("%d, %d \n", n_jokers, n_no_jokers);
        if(n_jokers == 0){
                *longueur = n;
                return cmd;

        }else{
                char **joker = malloc(n_jokers*sizeof(char *));
                char **no_joker = malloc(n_no_jokers*sizeof(char *));
                int j=0, k=0;
                for (int i=0; i<n; i++){
                        if(in('*', cmd[i]) || in('{', cmd[i]) || in('~', cmd[i])) {joker[j] = cmd[i]; j++;}
                        else {no_joker[k] = cmd[i];k++;}
                }
                char *pattern = joker[0];
                if(in('~',pattern) && !(in('{', pattern))){
                        glob(pattern, GLOB_DOOFFS | GLOB_TILDE, NULL, &globbuf);
                }else if(!in('~',pattern) && (in('{', pattern))){
                        glob(pattern, GLOB_DOOFFS | GLOB_BRACE, NULL, &globbuf);
                }else if(in('~',pattern) && (in('{', pattern))){
                        glob(pattern, GLOB_DOOFFS | GLOB_TILDE | GLOB_BRACE, NULL, &globbuf);
                }else{
                        glob(pattern, 0, NULL, &globbuf);
                }
                if (n_jokers > 1) {
                        for (int i = 1; i < n_jokers; i++) {
                                char *pa = joker[i];
                                if(in('~',pa) && !(in('{', pa))){
                                        glob(pa, GLOB_DOOFFS | GLOB_TILDE | GLOB_APPEND, NULL, &globbuf);
                                }else if(!in('~',pa) && (in('{', pa))){
                                        glob(pa, GLOB_DOOFFS | GLOB_BRACE | GLOB_APPEND, NULL, &globbuf);
                                }else if(in('~',pa) && (in('{', pa))){
                                        glob(pa, GLOB_DOOFFS | GLOB_TILDE | GLOB_BRACE | GLOB_APPEND, NULL, &globbuf);
                                }else{
                                        glob(pa, GLOB_DOOFFS | GLOB_APPEND, NULL, &globbuf);
                                }
                        }
                }
                int length = (int)globbuf.gl_pathc;
                char **ptr = globbuf.gl_pathv;
                char **result = malloc((n_no_jokers + length + 1)*sizeof(char*));
                for(int i=0; i<n_no_jokers; i++){
                        *(result+i) = no_joker[i];
                }
                for(int i=0; i<=length; i++){
                        *(result+n_no_jokers+i) = ptr[i];
                }
                *longueur = n_no_jokers + length;
                return result;
        }
}


list_job *list = NULL;
int pipe_fd[2];

void add_list_job(int pid, char * commande) {
        list_job * intermediaire = list;
        list = malloc(sizeof(list_job));
        list->pid = pid;
        list->commande = strdup(commande);
        list->next = NULL;
        list->next=intermediaire;
}

void supprime_list_job(int pid) {
        list_job sent = {-1, "rien", list};
        list_job *p = &sent;
        while (p->next != NULL && p->next->pid != pid){
                p = p->next;
        }
        if (p->next != NULL){
                list_job *style = p->next;
                p->next = style->next;
                free(style);
        }
        list = sent.next;
}

void jobs(){
        list_job * p = list;
        if (p == NULL){
                printf("[NO PROCESS] \n");
        }
        while(p != NULL){
                int status;
                if(waitpid(p->pid, &status, WNOHANG) == 0){
                        printf("[%d]   Running  %s \n", p->pid, p->commande);
                }
                else{
                        printf("[%d]   Stopped  %s \n", p->pid, p->commande);
                        supprime_list_job(p->pid);

                }
                p = p->next;
        }
}
#if USE_GUILE == 1
#include <libguile.h>


int question6_executer(char *line)
{
	/* Question 6: Insert your code to execute the command line
	 * identically to the standard execution scheme:
	 * parsecmd, then fork+execvp, for a single command.
	 * pipe and i/o redirection are not required.
	 */
	struct cmdline *l;
	l = parsecmd(&line);
	int i;
	for (i=0; l->seq[i]!=0; i++){
	        char **cmd = l->seq[i];
	        if(strcmp(cmd[0], "jobs")==0){
	                jobs();
	        }else{
	                executing(
                                        cmd,
                                        l->bg,
                                        i > 0,
                                        l->seq[i + 1] != 0,
                                        l->in,
                                        l->out
                                );
	        }
	}
	return 0;

}

SCM executer_wrapper(SCM x)
{
        return scm_from_int(question6_executer(scm_to_locale_stringn(x, 0)));
}
#endif


void terminate(char *line) {
#if USE_GNU_READLINE == 1
	/* rl_clear_history() does not exist yet in centOS 6 */
	clear_history();
#endif
	if (line)
	  free(line);
	printf("exit\n");
	exit(0);
}

void pipe_management(int in_pipe, int out_pipe, int *fd){
        if(in_pipe) {
                dup2(fd[0], 0);
        }
        if(out_pipe) {
                dup2(fd[1], 1);
        }
}

void files_without_pipe(char *in_file, char *out_file){
        int fd_file;
        if (in_file != NULL) {
                if ((fd_file = open(in_file, O_RDONLY)) == -1) {
                        printf("[ERROR INPUT FILE] %s\n", in_file);
                        return;
                }

                dup2(fd_file, 0);
                close(fd_file);
        }
        if (out_file != NULL) {
                if ((fd_file = open(out_file, O_WRONLY|O_TRUNC|O_CREAT, 0666)) == -1) {
                        printf("[ERROR OUTPUT FILE] %s\n", out_file);
                        return;
                }

                dup2(fd_file, 1);
                close(fd_file);
        }
}
void files_with_pipe_in(char *out_file){
        int fd_file;
        if ((fd_file = open(out_file, O_WRONLY|O_TRUNC|O_CREAT, 0666)) == -1) {
                printf("[ERROR OUTPUT FILE] %s\n", out_file);
                return;
        }

        dup2(fd_file, 1);
        close(fd_file);

}
void files_with_pipe_out(char *in_file){
        int fd_file;
        if ((fd_file = open(in_file, O_RDONLY)) == -1) {
                printf("[ERROR INPUT FILE] %s\n", in_file);
                return;
        }

        dup2(fd_file, 0);
        close(fd_file);
}



void executing(char **commande,
               int bg,
               int in_pipe,
               int out_pipe,
               char *in_file,
               char *out_file) {
        /* The fork() function returns an integer which can be either '-1' or '0'
         * for the a child process
         */
        int pid = fork();


        // [CHILD PROCESS] pid < 0
        if (pid < 0) {
                printf("[ERROR]\n");
                return;
        }
                // [CHILD PROCESS] pid == 0
        else if (pid == 0) {
                pipe_management(in_pipe, out_pipe, pipe_fd);
                if (!out_pipe && !in_pipe){
                        files_without_pipe(in_file, out_file);
                }

                if (in_file != NULL && out_pipe) {
                        files_with_pipe_out(in_file);
                }
                if (out_file != NULL && in_pipe) {
                        files_with_pipe_in(out_file);
                }

                execvp(commande[0], commande);
                return;
        }

        // [PARENT PROCESS] pid = child pid
        close(pipe_fd[1]);

        if(out_pipe) {
                return;
        }
        if (!bg) {
                int status;
                waitpid(pid, &status, 0);
        }
        if(bg) {
                add_list_job(pid, commande[0]);
        }
}

int main() {
        printf("Variante %d: %s\n", VARIANTE, VARIANTE_STRING);

#if USE_GUILE == 1
        scm_init_guile();
        /* register "executer" function in scheme */
        scm_c_define_gsubr("executer", 1, 0, 0, executer_wrapper);
#endif

	while (1) {
		struct cmdline *l;
		char *line=0;
		int i;
		char *prompt = "ensishell>";

		/* Readline use some internal memory structure that
		   can not be cleaned at the end of the program. Thus
		   one memory leak per command seems unavoidable yet */
		line = readline(prompt);
		if (line == 0 || ! strncmp(line,"exit", 4)) {
			terminate(line);
		}

#if USE_GNU_READLINE == 1
		add_history(line);
#endif


#if USE_GUILE == 1
		/* The line is a scheme command */
		if (line[0] == '(') {
			char catchligne[strlen(line) + 256];
			sprintf(catchligne, "(catch #t (lambda () %s) (lambda (key . parameters) (display \"mauvaise expression/bug en scheme\n\")))", line);
			scm_eval_string(scm_from_locale_string(catchligne));
			free(line);
                        continue;
                }
#endif

		/* parsecmd free line and set it up to 0 */
		l = parsecmd( & line);

		/* If input stream closed, normal termination */
		if (!l) {
		  
			terminate(0);
		}
		

		
		if (l->err) {
			/* Syntax error, read another command */
			printf("error: %s\n", l->err);
			continue;
		}

		if (l->in) printf("in: %s\n", l->in);
		if (l->out) printf("out: %s\n", l->out);
		if (l->bg) printf("background (&)\n");
                pipe(pipe_fd);
		/* Display each command of the pipe */
		for (i=0; l->seq[i]!=0; i++) {
			char **cmd = l->seq[i];
			int le;
			char **new_cmd = traite_jokers(cmd, &le);
                        if (strcmp(cmd[0], "jobs") == 0) {
                                jobs();
                        }else {
                                executing(
                                        new_cmd,
                                        l->bg,
                                        i > 0,
                                        l->seq[i + 1] != 0,
                                        l->in,
                                        l->out
                                );
                        }
		}
	}
}
