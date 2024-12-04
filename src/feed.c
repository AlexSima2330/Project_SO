#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include "comunicacao.h"

// Variável global para controlar a thread de leitura
int continuar_lendo = 1;



// Função para a thread que lê mensagens
/* void *thread_read(void *arg) {
    char fifo_cli[40];
    sprintf(fifo_cli, FIFO_CLI, getpid());
    int fd = open(fifo_cli, O_RDONLY);

    if (fd == -1) {
        perror("[Feed] Erro ao abrir FIFO para leitura");
        pthread_exit(NULL);
    }

    Mensagem m;
    while (continuar_lendo) {
        if (read(fd, &m, sizeof(Mensagem)) > 0) {
            printf("\n[Feed] Nova mensagem no tópico '%s': %s (de %s)\n",
                   m.topic, m.body, m.username);
        }
    }

    close(fd);
    return NULL;
} */

void *thread_read(void *arg) {
    char fifo_cli[40];
    sprintf(fifo_cli, FIFO_CLI, getpid());
    int fd = open(fifo_cli, O_RDONLY);

    if (fd == -1) {
        perror("[Feed] Erro ao abrir FIFO para leitura");
        pthread_exit(NULL);
    }

    Resposta r;
    while (1) {
        if (read(fd, &r, sizeof(Resposta)) > 0) {
            printf("\n[Feed] Notificação do manager: %s\n", r.resposta);

            // Verifica se a mensagem indica que o feed foi removido
            if (strstr(r.resposta, "Você foi removido da plataforma.") != NULL) {
                printf("[Feed] O programa será encerrado.\n");
                close(fd);
                exit(0); // Encerra o programa
            }
        } else {
            // Trata a desconexão do FIFO
            perror("[Feed] Erro ao ler do FIFO (o manager pode ter sido encerrado)");
            close(fd);
            pthread_exit(NULL);
        }
    }

    close(fd);
    return NULL;
}



int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Uso: %s <username>\n", argv[0]);
        return 1;
    }

    char fifo_cli[40];
    Pedido p;
    Resposta r;

    // Obter o username do argumento da linha de comando
    strcpy(p.username, argv[1]);
    p.pid = getpid();

    // Criar FIFO exclusivo para o feed
    sprintf(fifo_cli, FIFO_CLI, getpid());
    unlink(fifo_cli);
    if (mkfifo(fifo_cli, 0600) == -1) {
        perror("[Feed] Erro ao criar FIFO exclusivo");
        return 1;
    }

    // Abrir o FIFO principal para comunicação com o manager
    int fd_manager = open(FIFO_SRV, O_WRONLY);
    if (fd_manager == -1) {
        printf("[Feed] O manager não está ativo.\n");
        unlink(fifo_cli);
        return 1;
    }

    // Enviar o pedido de login ao manager
    strcpy(p.comando, "login");
    write(fd_manager, &p, sizeof(Pedido));

    // Abrir o FIFO exclusivo para leitura da resposta
    int fd_cli = open(fifo_cli, O_RDONLY);
    if (fd_cli == -1) {
        perror("[Feed] Erro ao abrir FIFO exclusivo para leitura");
        unlink(fifo_cli);
        return 1;
    }

    // Ler a resposta do manager
    if (read(fd_cli, &r, sizeof(Resposta)) > 0) {
        if (strcmp(r.resposta, "OK") != 0) {
            printf("[Feed] Erro: %s\n", r.resposta);
            close(fd_cli);
            unlink(fifo_cli);
            return 1;
        }
    } else {
        printf("[Feed] Erro ao receber resposta do manager.\n");
        close(fd_cli);
        unlink(fifo_cli);
        return 1;
    }
    close(fd_cli);

    printf("[Feed] Identificação aceita! Bem-vindo, %s.\n", p.username);

    // Iniciar a thread de leitura
    pthread_t tid_read;
    continuar_lendo = 1;
    //pthread_create(&tid_read, NULL, thread_read, NULL);

    // Loop de comandos do utilizador
    char comando[256];
    while (continuar_lendo) {
        printf("> ");
        fflush(stdout);

        if (!fgets(comando, sizeof(comando), stdin)) {
            break; // Sai caso ocorra um erro ao ler
        }

        comando[strcspn(comando, "\n")] = 0; // Remove o '\n'

        if (strcmp(comando, "topics") == 0) {
            strcpy(p.comando, "topics");
            write(fd_manager, &p, sizeof(Pedido));
        } else if (strncmp(comando, "msg ", 4) == 0) {
            char *args = comando + 4;
            char *topico = strtok(args, " ");
            char *duracao = strtok(NULL, " ");
            char *mensagem = strtok(NULL, "");

            if (!topico || !duracao || !mensagem) {
                printf("[Feed] Comando inválido. Uso: msg <topico> <duração> <mensagem>\n");
                continue;
            }

            strcpy(p.topic, topico);
            p.time_to_live = atoi(duracao);
            strcpy(p.mensagem, mensagem);
            strcpy(p.comando, "msg");
            write(fd_manager, &p, sizeof(Pedido));
        } else if (strncmp(comando, "subscribe ", 10) == 0) {
            char *topico = comando + 10;
            strcpy(p.topic, topico);
            strcpy(p.comando, "subscribe");
            write(fd_manager, &p, sizeof(Pedido));
        } else if (strncmp(comando, "unsubscribe ", 12) == 0) {
            char *topico = comando + 12;
            strcpy(p.topic, topico);
            strcpy(p.comando, "unsubscribe");
            write(fd_manager, &p, sizeof(Pedido));
        } else if (strcmp(comando, "exit") == 0) {
            strcpy(p.comando, "exit");
            write(fd_manager, &p, sizeof(Pedido));
            continuar_lendo = 0;
            break;
        } else {
            printf("[Feed] Comando não reconhecido.\n");
            continue;
        }

        // Ler a resposta do manager
        fd_cli = open(fifo_cli, O_RDONLY);
        if (fd_cli == -1) {
            perror("[Feed] Erro ao abrir FIFO exclusivo para resposta");
            continue;
        }
        if (read(fd_cli, &r, sizeof(Resposta)) > 0) {
            printf("[Feed] Resposta do manager: %s\n", r.resposta);
        }
        close(fd_cli);
    }

    // Finalizar
    pthread_join(tid_read, NULL);
    unlink(fifo_cli);
    return 0;
}
