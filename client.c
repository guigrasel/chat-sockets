// client.c - Cliente de chat TCP com prefixo de apelido
// Compilar: gcc -O2 -Wall -Wextra -o client client.c
// Uso: ./client <host> <porta> <apelido>
// Ex.: ./client 127.0.0.1 5000 nome

#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

static volatile sig_atomic_t em_execucao = 1;
static void ao_receber_sigint(int sinal){ (void)sinal; em_execucao = 0; }

static void erro_fatal(const char *mensagem){ perror(mensagem); exit(EXIT_FAILURE); }

static int enviar_tudo(int socket_fd, const char *buffer, size_t tamanho) {
  size_t enviado = 0;
  while (enviado < tamanho) {
    ssize_t bytes_enviados = send(socket_fd, buffer + enviado, tamanho - enviado, 0);
    if (bytes_enviados < 0) {
      if (errno == EINTR) continue;
      return -1;
    }
    enviado += (size_t)bytes_enviados;
  }
  return 0;
}

int main(int argc, char **argv) {
  if (argc != 4) {
    fprintf(stderr, "Uso: %s <host> <porta> <apelido>\n", argv[0]);
    return EXIT_FAILURE;
  }
  signal(SIGINT, ao_receber_sigint);

  const char *endereco_host = argv[1];
  const char *porta = argv[2];
  const char *apelido = argv[3];

  struct addrinfo dicas = {0}, *resultado = NULL;
  dicas.ai_family = AF_INET; 
  dicas.ai_socktype = SOCK_STREAM;

  int erro = getaddrinfo(endereco_host, porta, &dicas, &resultado);
  if (erro) { fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(erro)); return EXIT_FAILURE; }

  int socket_fd = -1;
  for (struct addrinfo *ptr = resultado; ptr; ptr = ptr->ai_next) {
    socket_fd = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
    if (socket_fd < 0) continue;
    if (connect(socket_fd, ptr->ai_addr, ptr->ai_addrlen) == 0) break;
    close(socket_fd); socket_fd = -1;
  }
  freeaddrinfo(resultado);
  if (socket_fd < 0) erro_fatal("connect");

  printf("Conectado como '%s'. Digite mensagens e pressione Enter. Ctrl+C para sair.\n", apelido);

  char prefixo[128];
  int tamanho_prefixo = snprintf(prefixo, sizeof(prefixo), "%s: ", apelido);

  while (em_execucao) {
    fd_set conjunto_leitura;
    FD_ZERO(&conjunto_leitura);
    FD_SET(socket_fd, &conjunto_leitura);
    FD_SET(STDIN_FILENO, &conjunto_leitura);
    int maior_fd = (socket_fd > STDIN_FILENO ? socket_fd : STDIN_FILENO);

    int prontos = select(maior_fd + 1, &conjunto_leitura, NULL, NULL, NULL);
    if (prontos < 0) {
      if (errno == EINTR) continue;
      erro_fatal("select");
    }

    if (FD_ISSET(STDIN_FILENO, &conjunto_leitura)) {
      char linha[1024];
      if (!fgets(linha, sizeof(linha), stdin)) {
        break;
      }

      size_t tamanho_linha = strlen(linha);
      bool tem_nova_linha = (tamanho_linha > 0 && linha[tamanho_linha-1] == '\n');
      char mensagem[128 + 1024 + 2];
      int tamanho_mensagem = snprintf(mensagem, sizeof(mensagem), "%s%s%s", prefixo, linha, tem_nova_linha ? "" : "\n");
      if (enviar_tudo(socket_fd, mensagem, (size_t)tamanho_mensagem) < 0) {
        perror("send");
        break;
      }
    }

    if (FD_ISSET(socket_fd, &conjunto_leitura)) {
      char buffer_recebido[2048];
      ssize_t bytes_recebidos = recv(socket_fd, buffer_recebido, sizeof(buffer_recebido)-1, 0);
      if (bytes_recebidos <= 0) {
        puts("Conexão encerrada pelo servidor.");
        break;
      }
      buffer_recebido[bytes_recebidos] = '\0';
      fputs(buffer_recebido, stdout);
      fflush(stdout);
    }
  }

  close(socket_fd);
  puts("Cliente finalizado.");
  return 0;
}
