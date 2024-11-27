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
        if (fgets(p.username, sizeof(p.username), stdin) == NULL) {
            printf("[Feed] Erro ao ler o input. Tente novamente.\n");
            continue;
        }

        // Remove o caractere de nova linha '\n', se existir
        p.username[strcspn(p.username, "\n")] = '\0';

        // Verifica se o nome está vazio
        if (strlen(p.username) == 0) {
            printf("[Feed] Erro: O nome de utilizador não pode estar vazio.\n");
            continue;
        }

        // Verifica se o comprimento do nome é válido
        if (strlen(p.username) >= 19) {
            printf("[Feed] Erro: O nome de utilizador ultrapassa o número de caracteres permitido (19).\n");
             int c;
            while ((c = getchar()) != '\n' && c != EOF); // Descarta o resto do input
            continue;
        }

        // Verifica se o nome contém espaços
        if (strchr(p.username, ' ') != NULL) {
            printf("[Feed] Erro: O nome de utilizador não pode conter espaços.\n");
            continue;
        }

        // Envia o pedido ao manager
        strcpy(p.comando, "login");
        p.pid = getpid();
        write(fd_manager, &p, sizeof(Pedido));

        fd_cli = open(fifo_cli, O_RDONLY);
        if (fd_cli == -1) {
            perror("[Feed] Erro ao abrir o pipe do cliente");
            continue;
        }
        read(fd_cli, &r, sizeof(Resposta));
        close(fd_cli);

        if (strcmp(r.resposta, "OK") != 0) {
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
