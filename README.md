# O que é um Socket?

Um **socket** é uma **porta de comunicação** que permite que dois programas troquem dados através de uma rede.
Ele é como uma “tomada virtual”: um lado conecta (cliente) e o outro lado espera conexões (servidor).

Na prática:

* Cada socket é identificado por **endereço IP + porta**.
* O servidor **escuta** em uma porta específica.
* O cliente **conecta** nessa porta e os dois passam a trocar dados como se fosse um “fio virtual”.

Há dois tipos principais:

* **SOCK\_STREAM (TCP)** – comunicação confiável, orientada a conexão (entrega garantida, ordem preservada).
* **SOCK\_DGRAM (UDP)** – comunicação rápida, sem conexão, sem garantia de entrega.

O código do servidor usa **TCP**, garantindo que todos os bytes enviados cheguem na ordem certa.

---

# Como funciona o servidor do projeto

O servidor implementa um **chat simples** usando sockets TCP e multiplexação com `select()`.
Aqui está o passo a passo:

## 1. Criação do socket do servidor

Ele cria um socket com:

```c
socket(AF_INET, SOCK_STREAM, 0);
```

* `AF_INET`: IPv4
* `SOCK_STREAM`: usa TCP
* O socket passa a ser um “ponto de escuta”.

Depois, usa `bind()` para associar esse socket a uma **porta** e `listen()` para colocá-lo em modo servidor.

## 2. Aceitação de clientes

Sempre que chega uma conexão nova, o servidor chama `accept()`:

* Cria um **novo socket** para aquele cliente específico.
* Guarda esse socket em uma lista (`clients[]`).
* Continua escutando para aceitar mais conexões.

Isso significa que há:

* **Um socket principal** (escutando a porta).
* **Um socket por cliente** (para trocar mensagens).

## 3. Laço principal com `select()`

O coração do servidor é o laço infinito que:

* Cria um conjunto (`fd_set`) com o socket do servidor e os sockets de todos os clientes.
* Chama `select()`, que **bloqueia** até:

  * Haver uma nova conexão.
  * Ou um cliente enviar dados.

Quando algo acontece:

* Se for uma nova conexão → usa `accept()` e adiciona o cliente.
* Se for mensagem de cliente → usa `recv()` para ler os bytes.

## 4. Broadcast das mensagens

Sempre que um cliente envia algo, o servidor:

* Lê os bytes recebidos.
* Chama `broadcast()`, que envia esses bytes para todos os outros clientes conectados.
* Se o cliente desconectar (ou enviar 0 bytes), o servidor fecha o socket dele e libera o slot.

## 5. Encerramento limpo

Quando você pressiona `Ctrl+C`, o sinal `SIGINT` é capturado:

* `running` é marcado como `0`.
* O laço principal termina.
* O servidor fecha todos os sockets abertos e imprime “Servidor finalizado.”

## Resumo em uma frase

Este servidor cria uma **sala de bate-papo via TCP**, aceita vários clientes, recebe dados de cada um e retransmite para todos os outros, tudo usando **um único processo** e a função `select()` para monitorar várias conexões ao mesmo tempo.

---

# Como funciona o cliente do projeto

O cliente implementa a ponta que **conecta ao servidor**, lê do teclado e do socket simultaneamente com `select()`, e envia mensagens **prefixadas com o apelido**.

## 1. Resolução de endereço e conexão

* Usa `getaddrinfo(host, porta, ...)` para obter os parâmetros de conexão (IPv4/TCP).
* Itera os resultados tentando `socket()` + `connect()` até conseguir abrir a conexão com o servidor.
* Em caso de erro fatal de conexão, aborta com mensagem apropriada.

## 2. Preparação do prefixo (apelido)

* Monta uma string `"<apelido>: "` usando `snprintf` e guarda o tamanho do prefixo.
* Todas as mensagens enviadas ao servidor serão precedidas por esse prefixo, para que os demais clientes vejam quem falou.

## 3. Laço principal com `select()` (stdin + socket)

O cliente precisa reagir a **duas fontes** de dados: o teclado (stdin) e o socket. Para isso:

* Cria um `fd_set` e adiciona `STDIN_FILENO` e o `socket_fd`.
* Chama `select()` e aguarda até um deles ficar legível.

### Quando o teclado (stdin) está pronto

* Lê uma linha com `fgets` para o buffer `linha`.
* Garante que a mensagem termine com `\n` (se o usuário não apertou Enter no final, adiciona).
* Concatena `prefixo + linha` em `mensagem`.
* Envia usando `enviar_tudo(socket_fd, mensagem, tamanho)`.

  * **`enviar_tudo`** faz um loop em torno de `send()` para lidar com **envio parcial** ou interrupções por sinal (`EINTR`), garantindo que **todos os bytes** sejam realmente enviados.
* Imprime localmente `"você: ..."` para feedback ao usuário.

### Quando o socket está pronto

* Lê dados com `recv()` para `buffer_recebido` e adiciona `\0` ao final para imprimir como string.
* Se `recv()` retornar `<= 0`, considera que o servidor fechou a conexão e encerra.
* **Evita ecoar a própria mensagem**: se o texto recebido começar com o mesmo `prefixo` do cliente, ele não imprime (o servidor retransmite para todos, então o cliente suprime sua própria cópia local).

## 4. Tratamento de sinais e encerramento

* Captura `SIGINT` (Ctrl+C) e marca `em_execucao = 0`, permitindo sair do laço com segurança.
* Fecha o `socket_fd` e imprime “Cliente finalizado.”.

## Resumo em uma frase

O cliente **resolve o endereço**, **conecta** ao servidor, e usa `select()` para **multiplexar** entrada do usuário e mensagens da rede; ao enviar, prefixa cada linha com o **apelido** e garante a entrega completa com `enviar_tudo`, enquanto no recebimento **omite** a própria mensagem para evitar eco duplicado.
