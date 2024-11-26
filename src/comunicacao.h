#ifndef COMUNICACAO_H
#define COMUNICACAO_H

#include <unistd.h>

#define FIFO_SRV "/tmp/manager_pipe"      // Pipe principal do manager
#define FIFO_CLI "/tmp/feed_pipe_%d"      // Pipe exclusivo para o feed

#define MAX_USERS 10                      // Número máximo de usuários
#define MAX_TOPICS 20                     // Número máximo de tópicos
#define MAX_MSG_LENGTH 300                // Tamanho máximo do corpo da mensagem
#define MAX_MESSAGES 5                    // Número máximo de mensagens por tópico

// Estrutura para uma mensagem commit feito 
typedef struct {
    char username[20];            // Nome do usuário que enviou
    char topic[20];               // Nome do tópico
    char body[MAX_MSG_LENGTH];    // Conteúdo da mensagem
    int time_to_live;             // Tempo de vida da mensagem (em segundos)
} Mensagem;

// Estrutura para um pedido vindo do feed
typedef struct {
    char username[20];            // Nome do usuário que fez o pedido
    char comando[20];             // Comando (login, subscribe, msg, etc.)
    char topic[20];               // Nome do tópico (se aplicável)
    char mensagem[MAX_MSG_LENGTH];// Corpo da mensagem (se aplicável)
    int time_to_live;             // Tempo de vida (se aplicável)
    pid_t pid;                    // PID do cliente
} Pedido;

// Estrutura para resposta enviada ao feed
typedef struct {
    char resposta[100];           // Resposta ao feed
    Mensagem mensagens[5];        // Mensagens enviadas ao feed (se aplicável)
    int num_mensagens;            // Quantidade de mensagens enviadas
} Resposta;

#endif
