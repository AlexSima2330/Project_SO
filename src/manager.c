#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include "comunicacao.h"
#include <pthread.h>

void processar_pedido(Pedido *p);

Usuario usuarios[MAX_USERS];
Topico topicos[MAX_TOPICS];
int num_usuarios = 0;
int num_topicos = 0;
int num_topics = 0;

void registrar_usuario(const char *username, pid_t pid, Resposta *r) {
    for (int i = 0; i < num_usuarios; i++) {
        if (strcmp(usuarios[i].username, username) == 0) {
            strcpy(r->resposta, "Erro: Username já em uso!");
            return;
        }
    }

    if (num_usuarios < MAX_USERS) {
        strcpy(usuarios[num_usuarios].username, username);
        usuarios[num_usuarios].num_topics = 0;
        usuarios[num_usuarios].pid = pid; // Salva o PID do usuário
        num_usuarios++;
        strcpy(r->resposta, "OK");
        printf("[Manager] Usuário %s registrado com sucesso. (PID: %d)\n", username, pid);
    } else {
        strcpy(r->resposta, "Erro: Limite de usuários atingido!");
    }
}



void *thread_read(void *arg) {
    int fd_manager = *(int *)arg; // Recupera o descritor do pipe principal
    Pedido p;

    while (1) {
        // Lê o pedido do pipe principal
        if (read(fd_manager, &p, sizeof(Pedido)) > 0) {
            // Processa o pedido usando a função existente
            processar_pedido(&p);
        }
    }

    return NULL; // Nunca será alcançado neste loop infinito
}


void processar_pedido(Pedido *p) {
    char fifo_cli[40];
    sprintf(fifo_cli, FIFO_CLI, p->pid); // Formata o nome do FIFO exclusivo do cliente
    int fd_cli = open(fifo_cli, O_WRONLY);
    if (fd_cli == -1) {
        perror("[Manager] Erro ao abrir o pipe do cliente");
        return;
    }

    Resposta r;

    // Processa o comando "login"
    if (strcmp(p->comando, "login") == 0) {
    registrar_usuario(p->username, p->pid, &r);
}


    // Processa o comando "subscribe"
   
else if (strcmp(p->comando, "subscribe") == 0) {
    int found = 0;

    // Verifica se o tópico já existe
    for (int i = 0; i < num_topicos; i++) {
        if (strcmp(topicos[i].name, p->topic) == 0) {
            found = 1; // Tópico já existe
            strcpy(r.resposta, "Tópico subscrito com sucesso!");

            // Envia mensagens persistentes do tópico para o feed
            for (int j = 0; j < topicos[i].num_mensagens; j++) {
                Mensagem *m = &topicos[i].mensagens[j];
                if (m->time_to_live > 0) { // Mensagem ainda válida
                    int fd_feed = open(fifo_cli, O_WRONLY);
                    if (fd_feed != -1) {
                        write(fd_feed, m, sizeof(Mensagem)); // Envia a mensagem
                        close(fd_feed);
                        printf("[Manager] Mensagem enviada para '%s': '%s'\n", p->username, m->body);
                    }
                }
            }
            break;
        }
    }

    // Se o tópico não existir, cria-o
    if (!found) {
        if (num_topicos < MAX_TOPICS) {
            strcpy(topicos[num_topicos].name, p->topic);
            topicos[num_topicos].num_mensagens = 0;
            num_topicos++;
            printf("[Manager] Novo tópico '%s' criado automaticamente durante a subscrição.\n", p->topic);
            strcpy(r.resposta, "Novo tópico criado e subscrito com sucesso!");
        } else {
            strcpy(r.resposta, "Erro: Limite de tópicos atingido!");
        }
    }

    // Associa o usuário ao tópico
    for (int i = 0; i < num_usuarios; i++) {
        if (strcmp(usuarios[i].username, p->username) == 0) {
            strcpy(usuarios[i].subscribed_topics[usuarios[i].num_topics], p->topic);
            usuarios[i].num_topics++;
            break;
        }
    }

    write(fd_cli, &r, sizeof(Resposta));
    close(fd_cli);
}



    // Processa o comando "msg"
    else if (strcmp(p->comando, "msg") == 0) {
    int found = 0;

    // Verifica se o tópico já existe
    for (int i = 0; i < num_topicos; i++) {
        if (strcmp(topicos[i].name, p->topic) == 0) {
            found = 1;

            // Adiciona a mensagem ao tópico
            if (topicos[i].num_mensagens < MAX_MESSAGES) {
                Mensagem *m = &topicos[i].mensagens[topicos[i].num_mensagens++];
                strcpy(m->username, p->username);
                strcpy(m->topic, p->topic);
                strcpy(m->body, p->mensagem);
                m->time_to_live = p->time_to_live;

                printf("[Manager] Mensagem adicionada ao tópico '%s' por '%s'.\n", p->topic, p->username);

                // Envia a mensagem para todos os feeds subscritos
                for (int j = 0; j < num_usuarios; j++) {
                    Usuario *u = &usuarios[j];

                    // Verifica se o usuário subscreveu ao tópico
                    for (int k = 0; k < u->num_topics; k++) {
                        if (strcmp(u->subscribed_topics[k], p->topic) == 0) {
                            char fifo_cli[40];
                            sprintf(fifo_cli, FIFO_CLI, u->pid); // Formata o FIFO exclusivo do feed
                            int fd_feed = open(fifo_cli, O_WRONLY);
                            if (fd_feed != -1) {
                                write(fd_feed, m, sizeof(Mensagem)); // Envia a mensagem para o feed
                                close(fd_feed);
                                printf("[Manager] Mensagem enviada para '%s' (usuário '%s').\n", fifo_cli, u->username);
                            } else {
                                perror("[Manager] Erro ao enviar mensagem para o feed");
                            }
                        }
                    }
                }

                strcpy(r.resposta, "Mensagem enviada com sucesso!");
            } else {
                strcpy(r.resposta, "Erro: Limite de mensagens atingido!");
            }

            write(fd_cli, &r, sizeof(Resposta));
            close(fd_cli);
            return;
        }
    }

    // Se o tópico não for encontrado, informe erro
    if (!found) {
        strcpy(r.resposta, "Erro: Tópico não encontrado!");
        write(fd_cli, &r, sizeof(Resposta));
        close(fd_cli);
    }
}


    // Processa comandos desconhecidos
    else {
        strcpy(r.resposta, "Comando desconhecido!");
    }

    write(fd_cli, &r, sizeof(Resposta));
    close(fd_cli);
}

 void salvar_mensagens() {
    // Obtemos o nome do ficheiro a partir da variável de ambiente
    const char *filename = getenv("MSG_FICH");
    if (!filename) {
        filename = "mensagens_persistentes.txt"; // Valor padrão caso a variável não esteja definida
    }

    FILE *arquivo = fopen(filename, "w");
    if (!arquivo) {
        perror("[Manager] Erro ao abrir o ficheiro de mensagens para salvaguarda");
        return;
    }

    // Itera sobre todos os tópicos e mensagens
    for (int i = 0; i < num_topicos; i++) {
        for (int j = 0; j < topicos[i].num_mensagens; j++) {
            Mensagem *m = &topicos[i].mensagens[j];
            if (m->time_to_live > 0) { // Salva apenas mensagens com TTL restante
                fprintf(arquivo, "%s %s %d %s\n", m->topic, m->username, m->time_to_live, m->body);
            }
        }
    }

    fclose(arquivo);
    printf("[Manager] Mensagens persistentes salvaguardadas em '%s'.\n", filename);
}


void carregar_mensagens() {
    const char *filename = getenv("MSG_FICH");
    if (!filename) {
        filename = "mensagens_persistentes.txt";
    }

    FILE *arquivo = fopen(filename, "r");
    if (!arquivo) {
        printf("[Manager] Nenhum ficheiro de mensagens encontrado.\n");
        return;
    }

    printf("[Manager] Recuperando mensagens persistentes de '%s'.\n", filename);

    while (!feof(arquivo)) {
        Mensagem m;
        if (fscanf(arquivo, "%s %s %d %[^\n]", m.topic, m.username, &m.time_to_live, m.body) == 4) {
            if (m.time_to_live > 0) { // Mensagem ainda válida
                // Verifica se o tópico já existe
                int i;
                for (i = 0; i < num_topicos; i++) {
                    if (strcmp(topicos[i].name, m.topic) == 0) {
                        break;
                    }
                }

                // Se o tópico não existir, cria-o
                if (i == num_topicos && num_topicos < MAX_TOPICS) {
                    strcpy(topicos[num_topicos].name, m.topic);
                    topicos[num_topicos].num_mensagens = 0;
                    num_topicos++;
                }

                // Adiciona a mensagem ao tópico
                if (i < num_topicos) {
                    if (topicos[i].num_mensagens < MAX_MESSAGES) {
                        topicos[i].mensagens[topicos[i].num_mensagens++] = m;
                        printf("[Manager] Mensagem recuperada: Tópico '%s', Autor '%s', TTL %d, Corpo '%s'\n",
                               m.topic, m.username, m.time_to_live, m.body);
                    }
                }
            }
        }
    }

    fclose(arquivo);
}





void *thread_scanf(void *arg) {
    char comando[100];
    printf("[Manager] Digite um comando:\n");

    while (1) {
        printf("[Manager] CMD> ");
        fflush(stdout); // Garante que o prompt seja exibido imediatamente
        scanf("%s", comando);

        if (strcmp(comando, "close") == 0) {
            printf("[Manager] Encerrando...\n");
            salvar_mensagens();
            exit(0);
        } else if (strcmp(comando, "status") == 0) {
            printf("[Manager] Número de usuários: %d\n", num_usuarios);
            printf("[Manager] Número de tópicos: %d\n", num_topicos);
       } else if (strcmp(comando, "reset") == 0) {
            num_usuarios = 0;
            num_topicos = 0;

            // Limpa os usuários
            for (int i = 0; i < MAX_USERS; i++) {
                memset(usuarios[i].username, 0, sizeof(usuarios[i].username));
                memset(usuarios[i].subscribed_topics, 0, sizeof(usuarios[i].subscribed_topics));
                usuarios[i].num_topics = 0;
                usuarios[i].pid = 0;
            }

            // Limpa os tópicos
            for (int i = 0; i < MAX_TOPICS; i++) {
                memset(topicos[i].name, 0, sizeof(topicos[i].name));
                memset(topicos[i].mensagens, 0, sizeof(topicos[i].mensagens));
                topicos[i].num_mensagens = 0;
            }

            printf("[Manager] Todos os dados foram resetados.\n");
        }
 else {
            printf("[Manager] Comando '%s' não reconhecido.\n", comando);
        }
    }

    return NULL;
}


void *thread_timer(void *arg) {
    while (1) {
        sleep(2); // Verifica a cada segundo
        for (int i = 0; i < num_topicos; i++) {
            for (int j = 0; j < topicos[i].num_mensagens; j++) {
                Mensagem *m = &topicos[i].mensagens[j];
                if (m->time_to_live > 0) {
                    m->time_to_live--;
                    printf("[Timer] Mensagem '%s' no tópico '%s' - TTL restante: %d\n",
                           m->body, m->topic, m->time_to_live);

                    if (m->time_to_live == 0) {
                        printf("[Timer] Removendo mensagem expirada '%s' do tópico '%s'.\n",
                               m->body, m->topic);

                        // Remove a mensagem deslocando as outras
                        for (int k = j; k < topicos[i].num_mensagens - 1; k++) {
                            topicos[i].mensagens[k] = topicos[i].mensagens[k + 1];
                        }
                        topicos[i].num_mensagens--; // Atualiza o número de mensagens no tópico
                        j--; // Ajusta o índice para a próxima mensagem
                    }
                }
            }
        }
    }
    return NULL;
}



int main() {
    carregar_mensagens();
    mkfifo(FIFO_SRV, 0600); // Criação do FIFO principal




    int fd_manager = open(FIFO_SRV, O_RDONLY); // Abre o FIFO principal para leitura
    if (fd_manager == -1) {
        perror("[Manager] Erro ao abrir o pipe principal");
        return 1;
    }
    pthread_t tid_timer;
    pthread_create(&tid_timer, NULL, thread_timer, NULL);
    


    pthread_t tid_read; // Declara a thread para leitura
    pthread_create(&tid_read, NULL, thread_read, (void *)&fd_manager); // Cria a thread de leitura

    pthread_t tid_scanf; // Declara a thread para comandos administrativos
    pthread_create(&tid_scanf, NULL, thread_scanf, NULL); // Cria a thread de comandos

    printf("[Manager] Aguardando pedidos...\n");

    // Aguarda o término das threads (opcional, apenas para controle)
    pthread_join(tid_read, NULL);
    pthread_join(tid_scanf, NULL);

    salvar_mensagens();
    close(fd_manager); // Fecha o descritor do FIFO principal
    return 0;
}
