//
// Created by elouati on 09/11/2020.
//

#ifndef ENSISHELL_ENSISHELL_H
#define ENSISHELL_ENSISHELL_H
#include "variante.h"
#include "readcmd.h"

struct cell_job{
        int pid;
        char *commande;
        struct cell_job *next;
};
typedef struct cell_job list_job;
void jobs();
void pipe_management(int in_pipe, int out_pipe, int *pipe_fd);
void files_without_pipe(char *in_file, char *out_file);
void files_with_pipe_in(char *out_file);
void files_with_pipe_out(char *in_file);
void supprime_list_job(int pid);
void executing(char **commande,
               int bg,
               int in_pipe,
               int out_pipe,
               char *in_file,
               char *out_file);

void add_list_job(int pid, char * commande);


#endif //ENSISHELL_ENSISHELL_H
