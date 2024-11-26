#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include "comunicacao.h"

void menu() {
    printf("\nEscolha uma opção:\n");
    printf("1 - Subscrever a um tópico\n");
    printf("2 - Enviar mensagem para um tópico\n");
    printf("3 - Sair\n");
    printf("Opção: ");
}

int main() {
    char fifo_cli[40];
    Pedido p;
    Resposta r;
    int fd_cli;

    sprintf(fifo_cli, FIFO_CLI, getpid());
    mkfifo(fifo_cli, 0600);

    int fd_manager = open(FIFO_SRV, O_WRONLY);
    if (access(FIFO_SRV, F_OK) != 0) {
        printf("[Feed] O manager não está ativo.\n");
        unlink(fifo_cli);
        return 1;
    }

    do {
        printf("Digite o seu nome: ");
        scanf("%s", p.username);

        if (strlen(p.username) >= 20){
            printf("[Feed] Erro: O nome de utilizador ultrupassa o número de caracteres permitido. \n");
            continue; // volta a percorrer o ciclo do incio
        }

        strcpy(p.comando, "login");
        p.pid = getpid();

        write(fd_manager, &p, sizeof(Pedido));

        fd_cli = open(fifo_cli, O_RDONLY);
        read(fd_cli, &r, sizeof(Resposta));
        close(fd_cli);

        if(strcmp(r.resposta, "OK") != 0){
            printf("[Feed] Username já está em uso. Tente novamente.\n");
        }
    } while (strcmp(r.resposta, "OK") != 0);

    printf("[Feed] Identificação aceita! Bem-vindo, %s.\n", p.username);

    int opcao;
    do {
        menu();
        scanf("%d", &opcao);

        switch (opcao) {
            case 1: // Subscrever
                printf("Digite o nome do tópico: ");
                scanf("%s", p.topic);
                strcpy(p.comando, "subscribe");
                write(fd_manager, &p, sizeof(Pedido));
                break;

            case 2: // Enviar mensagem
                printf("Digite o nome do tópico: ");
                scanf("%s", p.topic);
                printf("Digite a mensagem: ");
                scanf(" %[^\n]", p.mensagem);
                printf("Tempo de vida da mensagem (0 = não persistente): ");
                scanf("%d", &p.time_to_live);
                strcpy(p.comando, "msg");
                write(fd_manager, &p, sizeof(Pedido));
                break;

            case 3: // Sair
                printf("Saindo...\n");
                break;

            default:
                printf("Opção inválida!\n");
        }

        if (opcao == 1 || opcao == 2) {
            fd_cli = open(fifo_cli, O_RDONLY);
            read(fd_cli, &r, sizeof(Resposta));
            printf("[Feed] Resposta do manager: %s\n", r.resposta);
            close(fd_cli);
        }
    } while (opcao != 3);

    unlink(fifo_cli);
    return 0;
}
