Espaço de armazenamento distribuído. Com serviço de chekpoint.

<div style="text-align:center"><img src=" Architecture.jpeg" /></div>

Passos para rodar o sistema:

### SERVIDOR DE CONFIGURAÇÃO DO SISTEMA

```bash
gcc -g -pthread ServerConfig.c -o config; 
./config
```

* Editar arquivo de configuração "config.txt" de acordo com os endereços de ip e porta dos servidores
* Roda na porta 9740

### SERVIDORES QUE OFERECEM O SERVIÇO DE MEMÓRIA

```bash
gcc -g -pthread Server.c MemoryManager.c -o server
```

```bash
./server ?(int port int shmkey int semkey)
```

* Três parâmetros opicionais, útil quando testado mais de um servidor na mesma máquina (nesse caso, esses valores devem ser obrigatoriamente diferentes):

	port: porta onde o servidor atenderá as conexões
	shmkey: key da memória compartilhada
	semkey: key do semáforo

* Se não for passado nada, os valores defaults são
(só pode ser usado quando os servidores estão em máquinas diferentes):

	int port = 9734;
	int shmkey = 1238;
	int semkey = 1234;

### LOGGERS

```bash
gcc -g -pthread Logger.c MemoryManager.c -o logger
```

```bash
./logger int is_master_logger ?(int port int shmkey int semkey)
```

* Um parâmetro obrigatório: int is_master_logger:
	0: é um logger comum;
	1: é, além de local, o logger global.

* Os parâmetros opcionais são os mesmos dos servidores de memória: port, shmkey e semkey; Porém o valor default para port é 9735;
* O logger de cada servidor deve possuir porta distinta do servidor ao qual ele é responsável localmente;
* TODOS os loggers devem rodar em server_port + 1 (Por exemplo: se o server 1 roda em 9734 e o 2 em 8673, o logger local 1 vai rodar
em 9735 e o 2 em 8674);
* shmkey e semkey devem, obrigatoriamente, ser iguais aos valores do servidor ao qual ele é responsável localmente (pois são recursos compartilhados).

### CLIENTES

* configurar ip do servidor de configuração nos clientes

```bash
gcc -g -pthread Client.c MemoryManager.c -o client
./client & ./client  & ./client & ./client & ./client
```
