#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include "comunicacao.h"

#define MAX_USERS 10
#define MAX_TOPICS 20
#define MAX_MESSAGES 5

pthread_mutex_t lock;

void processar_pedido(Pedido *p);

typedef struct {
    char username[20];
    char subscribed_topics[MAX_TOPICS][20];
    int num_topics;
    pid_t pid;
} Usuario;

typedef struct {
    char name[20];
    Mensagem mensagens[MAX_MESSAGES];
    int num_mensagens;
    int bloqueado; // 0 = desbloqueado, 1 = bloqueado
} Topico;

Usuario usuarios[MAX_USERS];
Topico topicos[MAX_TOPICS];
int num_usuarios = 0;
int num_topicos = 0;

// Funções auxiliares
void salvar_mensagens();
void carregar_mensagens();

void *thread_read(void *arg) {
    int fd_manager = *(int *)arg;
    Pedido p;

    while (1) {
        if (read(fd_manager, &p, sizeof(Pedido)) > 0) {
            processar_pedido(&p);
        }
    }
    return NULL;
}

void *thread_scanf(void *arg) {
    char comando[100];
    printf("[Manager] Digite um comando:\n");

    while (1) {
        printf("[Manager] CMD> ");
        fflush(stdout);

        if (!fgets(comando, sizeof(comando), stdin)) {
            printf("[Manager] Erro ao ler comando.\n");
            continue;
        }

        comando[strcspn(comando, "\n")] = 0; // Remove o '\n'

      if (strncmp(comando, "remove ", 7) == 0) {
    char *username_to_remove = comando + 7;
    int user_found = 0;

    for (int i = 0; i < num_usuarios; i++) {
        if (strcmp(usuarios[i].username, username_to_remove) == 0) {
            user_found = 1;

            // Notificar o feed do usuário a ser removido
            char fifo_cli[40];
            sprintf(fifo_cli, FIFO_CLI, usuarios[i].pid);
            printf("[Manager] Tentando notificar feed do usuário removido (%s)...\n", fifo_cli);

            int fd_cli = open(fifo_cli, O_WRONLY | O_NONBLOCK);
            if (fd_cli != -1) {
                Resposta r;
                strcpy(r.resposta, "Você foi removido da plataforma. O programa será encerrado.");
                write(fd_cli, &r, sizeof(Resposta));
                close(fd_cli);
                printf("[Manager] Feed do usuário '%s' notificado com sucesso.\n", username_to_remove);
            } else {
                perror("[Manager] Erro ao notificar feed do usuário removido");
            }

            // Remover o usuário da lista de usuários
            for (int k = i; k < num_usuarios - 1; k++) {
                usuarios[k] = usuarios[k + 1];
            }
            num_usuarios--;

            // Notificar todos os outros feeds
            for (int j = 0; j < num_usuarios; j++) {
                char fifo_feed[40];
                sprintf(fifo_feed, FIFO_CLI, usuarios[j].pid);
                printf("[Manager] Tentando notificar feed do usuário '%s'...\n", usuarios[j].username);

                int fd_feed = open(fifo_feed, O_WRONLY | O_NONBLOCK);
                if (fd_feed != -1) {
                    Resposta r;
                    sprintf(r.resposta, "Usuário '%s' foi removido da plataforma.", username_to_remove);
                    write(fd_feed, &r, sizeof(Resposta));
                    close(fd_feed);
                    printf("[Manager] Feed do usuário '%s' notificado com sucesso.\n", usuarios[j].username);
                } else {
                    perror("[Manager] Erro ao notificar outros feeds");
                }
            }

            printf("[Manager] Utilizador '%s' removido com sucesso.\n", username_to_remove);
            break;
        }
    }

    if (!user_found) {
        printf("[Manager] Utilizador '%s' não encontrado.\n", username_to_remove);
    }



        } else if (strcmp(comando, "users") == 0) {
            printf("[Manager] Listando utilizadores:\n");
            for (int i = 0; i < num_usuarios; i++) {
                printf("Usuário: %s (PID: %d)\n", usuarios[i].username, usuarios[i].pid);
            }
        } else if (strcmp(comando, "topics") == 0) {
            printf("[Manager] Listando tópicos:\n");
            for (int i = 0; i < num_topicos; i++) {
                printf("Tópico: %s (Mensagens: %d)\n", topicos[i].name, topicos[i].num_mensagens);
            }
        } else if (strcmp(comando, "reset") == 0) {
            num_usuarios = 0;
            num_topicos = 0;
            memset(usuarios, 0, sizeof(usuarios));
            memset(topicos, 0, sizeof(topicos));
            printf("[Manager] Todos os dados foram resetados.\n");
        } else if (strcmp(comando, "close") == 0) {
            printf("[Manager] Encerrando...\n");
            salvar_mensagens();
            exit(0);
        } else {
            printf("[Manager] Comando '%s' não reconhecido.\n", comando);
        }
    }
    return NULL;
}





void *thread_timer(void *arg) {
    while (1) {
        sleep(1);
        for (int i = 0; i < num_topicos; i++) {
            for (int j = 0; j < topicos[i].num_mensagens; j++) {
                Mensagem *m = &topicos[i].mensagens[j];
                if (m->time_to_live > 0) {
                    m->time_to_live--;
                    if (m->time_to_live == 0) {
                        printf("[Timer] Mensagem expirada no tópico '%s': %s\n", m->topic, m->body);
                        for (int k = j; k < topicos[i].num_mensagens - 1; k++) {
                            topicos[i].mensagens[k] = topicos[i].mensagens[k + 1];
                        }
                        topicos[i].num_mensagens--;
                        j--;
                    }
                }
            }
        }
    }
    return NULL;
}

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
        usuarios[num_usuarios].pid = pid;
        num_usuarios++;
        strcpy(r->resposta, "OK");
        printf("[Manager] Usuário '%s' registrado com sucesso. (PID: %d)\n", username, pid);
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
        registrar_usuario(p->username, p->pid, &r);
    } else if (strcmp(p->comando, "topics") == 0) {
        pthread_mutex_lock(&lock);
        if (num_topicos == 0) {
            strcpy(r.resposta, "Nenhum tópico disponível.");
        } else {
            char buffer[512] = "Tópicos:\n";
            for (int i = 0; i < num_topicos; i++) {
                char linha[128];
                sprintf(linha, "- %s (Mensagens: %d)\n", topicos[i].name, topicos[i].num_mensagens);
                strcat(buffer, linha);
            }
            strncpy(r.resposta, buffer, sizeof(r.resposta) - 1);
            r.resposta[sizeof(r.resposta) - 1] = '\0'; // Garante que a resposta é terminada corretamente
        }
        pthread_mutex_unlock(&lock);
    } else if (strcmp(p->comando, "subscribe") == 0) {
        pthread_mutex_lock(&lock);
        int found = 0;
        for (int i = 0; i < num_topicos; i++) {
            if (strcmp(topicos[i].name, p->topic) == 0) {
                found = 1;
                break;
            }
        }

        if (!found) {
            if (num_topicos < MAX_TOPICS) {
                strcpy(topicos[num_topicos].name, p->topic);
                topicos[num_topicos].num_mensagens = 0;
                num_topicos++;
                strcpy(r.resposta, "Subscrito com sucesso!");
                printf("[Manager] Novo tópico '%s' criado automaticamente.\n", p->topic);
            } else {
                strcpy(r.resposta, "Erro: Limite de tópicos atingido!");
                pthread_mutex_unlock(&lock);
                write(fd_cli, &r, sizeof(Resposta));
                close(fd_cli);
                return;
            }
        }

        for (int i = 0; i < num_usuarios; i++) {
            if (strcmp(usuarios[i].username, p->username) == 0) {
                strcpy(usuarios[i].subscribed_topics[usuarios[i].num_topics], p->topic);
                usuarios[i].num_topics++;
                strcpy(r.resposta, "Subscrito com sucesso!");
                break;
            }
        }
        pthread_mutex_unlock(&lock);
    } else if (strcmp(p->comando, "unsubscribe") == 0) {
        pthread_mutex_lock(&lock);
        int user_found = 0;
        for (int i = 0; i < num_usuarios; i++) {
            if (strcmp(usuarios[i].username, p->username) == 0) {
                user_found = 1;
                int topic_found = 0;
                for (int j = 0; j < usuarios[i].num_topics; j++) {
                    if (strcmp(usuarios[i].subscribed_topics[j], p->topic) == 0) {
                        topic_found = 1;
                        // Remove o tópico da lista de subscrições
                        for (int k = j; k < usuarios[i].num_topics - 1; k++) {
                            strcpy(usuarios[i].subscribed_topics[k], usuarios[i].subscribed_topics[k + 1]);
                        }
                        usuarios[i].num_topics--;
                        break;
                    }
                }
                if (topic_found) {
                    strcpy(r.resposta, "Desinscrito com sucesso!");
                } else {
                    strcpy(r.resposta, "Erro: Você não está subscrito neste tópico!");
                }
                break;
            }
        }
        if (!user_found) {
            strcpy(r.resposta, "Erro: Usuário não encontrado!");
        }

        // Verificar se o tópico deve ser removido do manager
        int topic_still_used = 0;
        for (int i = 0; i < num_usuarios; i++) {
            for (int j = 0; j < usuarios[i].num_topics; j++) {
                if (strcmp(usuarios[i].subscribed_topics[j], p->topic) == 0) {
                    topic_still_used = 1;
                    break;
                }
            }
            if (topic_still_used) break;
        }

        if (!topic_still_used) {
            for (int i = 0; i < num_topicos; i++) {
                if (strcmp(topicos[i].name, p->topic) == 0) {
                    for (int j = i; j < num_topicos - 1; j++) {
                        topicos[j] = topicos[j + 1];
                    }
                    num_topicos--;
                    printf("[Manager] Tópico '%s' removido por não ter mais assinantes.\n", p->topic);
                    break;
                }
            }
        }

        pthread_mutex_unlock(&lock);
    } else if (strcmp(p->comando, "msg") == 0) {
        pthread_mutex_lock(&lock);
        for (int i = 0; i < num_topicos; i++) {
            if (strcmp(topicos[i].name, p->topic) == 0) {
                if (topicos[i].num_mensagens < MAX_MESSAGES) {
                    Mensagem *m = &topicos[i].mensagens[topicos[i].num_mensagens++];
                    strcpy(m->username, p->username);
                    strcpy(m->topic, p->topic);
                    strcpy(m->body, p->mensagem);
                    m->time_to_live = p->time_to_live;

                    printf("[Manager] Mensagem adicionada ao tópico '%s' por '%s'.\n", p->topic, p->username);

                    for (int j = 0; j < num_usuarios; j++) {
                        Usuario *u = &usuarios[j];
                        for (int k = 0; k < u->num_topics; k++) {
                            if (strcmp(u->subscribed_topics[k], p->topic) == 0) {
                                char fifo_feed[40];
                                sprintf(fifo_feed, FIFO_CLI, u->pid);
                                int fd_feed = open(fifo_feed, O_WRONLY);
                                if (fd_feed != -1) {
                                    write(fd_feed, m, sizeof(Mensagem));
                                    close(fd_feed);
                                    printf("[Manager] Mensagem enviada para '%s' (usuário '%s').\n", fifo_feed, u->username);
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
                pthread_mutex_unlock(&lock);
                write(fd_cli, &r, sizeof(Resposta));
                close(fd_cli);
                return;
            }
        }
        strcpy(r.resposta, "Erro: Tópico não encontrado!");
        pthread_mutex_unlock(&lock);
    } else {
        strcpy(r.resposta, "Comando desconhecido!");
    }

    write(fd_cli, &r, sizeof(Resposta));
    close(fd_cli);
}


// Funções para salvar e carregar mensagens
void salvar_mensagens() {
    const char *filename = getenv("MSG_FICH") ? getenv("MSG_FICH") : "mensagens_persistentes.txt";
    FILE *file = fopen(filename, "w");
    if (!file) return;

    for (int i = 0; i < num_topicos; i++) {
        for (int j = 0; j < topicos[i].num_mensagens; j++) {
            Mensagem *m = &topicos[i].mensagens[j];
            if (m->time_to_live > 0) {
                fprintf(file, "%s %s %d %s\n", m->topic, m->username, m->time_to_live, m->body);
            }
        }
    }
    fclose(file);
}

void carregar_mensagens() {
    const char *filename = getenv("MSG_FICH") ? getenv("MSG_FICH") : "mensagens_persistentes.txt";
    FILE *file = fopen(filename, "r");
    if (!file) return;

    while (!feof(file)) {
        Mensagem m;
        if (fscanf(file, "%s %s %d %[^\n]", m.topic, m.username, &m.time_to_live, m.body) == 4) {
            int i;
            for (i = 0; i < num_topicos; i++) {
                if (strcmp(topicos[i].name, m.topic) == 0) break;
            }
            if (i == num_topicos && num_topicos < MAX_TOPICS) {
                strcpy(topicos[num_topicos].name, m.topic);
                topicos[num_topicos].num_mensagens = 0;
                num_topicos++;
            }
            if (i < num_topicos) {
                topicos[i].mensagens[topicos[i].num_mensagens++] = m;
            }
        }
    }
    fclose(file);
}


int main() {
    // Inicializa o mutex
    pthread_mutex_init(&lock, NULL);

    carregar_mensagens();
    mkfifo(FIFO_SRV, 0600);
    int fd_manager = open(FIFO_SRV, O_RDONLY);
    if (fd_manager == -1) {
        perror("[Manager] Erro ao abrir FIFO principal");
        return 1;
    }

    pthread_t tid_timer, tid_scanf, tid_read;
    pthread_create(&tid_timer, NULL, thread_timer, NULL);
    pthread_create(&tid_scanf, NULL, thread_scanf, NULL);
    pthread_create(&tid_read, NULL, thread_read, &fd_manager);

    pthread_join(tid_read, NULL);
    pthread_join(tid_scanf, NULL);

    // Salva mensagens persistentes antes de encerrar
    salvar_mensagens();

    // Fecha o FIFO principal
    close(fd_manager);

    // Destrói o mutex
    pthread_mutex_destroy(&lock);

    return 0;
}
