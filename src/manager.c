#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include "comunicacao.h"

typedef struct {
    char username[20];                   // Nome do usuário
    char subscribed_topics[MAX_TOPICS][20]; // Tópicos subscritos
    int num_topics;                      // Número de tópicos subscritos
} Usuario;

typedef struct {
    char name[20];                       // Nome do tópico
    Mensagem mensagens[MAX_MESSAGES];    // Mensagens do tópico
    int num_mensagens;                   // Quantidade de mensagens no tópico
} Topico;

Usuario usuarios[MAX_USERS];
Topico topicos[MAX_TOPICS];
int num_usuarios = 0;
int num_topicos = 0;

void registrar_usuario(const char *username, Resposta *r) {
    for (int i = 0; i < num_usuarios; i++) {
        if (strcmp(usuarios[i].username, username) == 0) {
            strcpy(r->resposta, "Erro: Username já em uso!");
            return;
        }
    }

    if (num_usuarios < MAX_USERS) {
        strcpy(usuarios[num_usuarios].username, username);
        usuarios[num_usuarios].num_topics = 0;
        num_usuarios++;
        strcpy(r->resposta, "OK");
        printf("[Manager] Usuário %s registrado com sucesso.\n", username);
    } else {
        strcpy(r->resposta, "Erro: Limite de usuários atingido!");
    }
}

void processar_pedido(Pedido *p) {
    char fifo_cli[40];
    sprintf(fifo_cli, FIFO_CLI, p->pid);
    int fd_cli = open(fifo_cli, O_WRONLY);
    if (fd_cli == -1) {
        perror("[Manager] Erro ao abrir o pipe do cliente");
        return;
    }

    Resposta r;

    if (strcmp(p->comando, "login") == 0) {
        registrar_usuario(p->username, &r);
    } 
    else if (strcmp(p->comando, "subscribe") == 0) {
        for (int i = 0; i < num_usuarios; i++) {
            if (strcmp(usuarios[i].username, p->username) == 0) {
                strcpy(usuarios[i].subscribed_topics[usuarios[i].num_topics], p->topic);
                usuarios[i].num_topics++;
                strcpy(r.resposta, "Subscrito com sucesso!");
                write(fd_cli, &r, sizeof(Resposta));
                close(fd_cli);
                return;
            }
        }
        strcpy(r.resposta, "Erro: Usuário não encontrado!");
    } 
    else if (strcmp(p->comando, "msg") == 0) {
        for (int i = 0; i < num_topicos; i++) {
            if (strcmp(topicos[i].name, p->topic) == 0) {
                if (topicos[i].num_mensagens < MAX_MESSAGES) {
                    Mensagem *m = &topicos[i].mensagens[topicos[i].num_mensagens++];
                    strcpy(m->username, p->username);
                    strcpy(m->topic, p->topic);
                    strcpy(m->body, p->mensagem);
                    m->time_to_live = p->time_to_live;
                    strcpy(r.resposta, "Mensagem enviada com sucesso!");
                } else {
                    strcpy(r.resposta, "Erro: Limite de mensagens atingido!");
                }
                write(fd_cli, &r, sizeof(Resposta));
                close(fd_cli);
                return;
            }
        }
        strcpy(r.resposta, "Erro: Tópico não encontrado!");
    } 
    else {
        strcpy(r.resposta, "Comando desconhecido!");
    }

    write(fd_cli, &r, sizeof(Resposta));
    close(fd_cli);
}

int main() {
    mkfifo(FIFO_SRV, 0600);
    int fd_manager = open(FIFO_SRV, O_RDONLY);
    if (fd_manager == -1) {
        perror("[Manager] Erro ao abrir o pipe principal");
        return 1;
    }

    int fd_dummy = open(FIFO_SRV, O_WRONLY);

    printf("[Manager] Aguardando pedidos...\n");

    Pedido p;
    while (read(fd_manager, &p, sizeof(Pedido)) > 0) {
        processar_pedido(&p);
    }

    close(fd_manager);
    close(fd_dummy);
    return 0;
}
