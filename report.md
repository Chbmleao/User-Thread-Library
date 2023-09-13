# PAGINADOR DE MEMÓRIA - RELATÓRIO

1. Termo de compromisso

Os membros do grupo afirmam que todo o código desenvolvido para este
trabalho é de autoria própria. Exceto pelo material listado no item
3 deste relatório, os membros do grupo afirmam não ter copiado
material da Internet nem ter obtido código de terceiros.

2. Membros do grupo e alocação de esforço

Preencha as linhas abaixo com o nome e o e-mail dos integrantes do
grupo. Substitua marcadores `XX` pela contribuição de cada membro
do grupo no desenvolvimento do trabalho (os valores devem somar
100%).

- Carlos Henrique Brito Malta Leão <chbmleao@ufmg.br> 50%
- Vinicius Alves Faria Resende <viniciusfariaresende@gmail.com> 50%

3. Referências bibliográficas
   https://pubs.opengroup.org/onlinepubs/7908799/xsh/ucontext.h.html
   https://petbcc.ufscar.br/time/

4. Estruturas de dados
   Foi utilizada apenas uma lista duplamente encadeada para armazenamento das threads.

5. Descreva e justifique as estruturas de dados utilizadas para
   gerência das threads de espaço do usuário (partes 1, 2 e 5).
   Para a gerência das threads de espaço do usuário foram utilizadas duas listas duplamente encadeadas.
   A primeira foi uma lista que represeta a fila de prontos de threads, em que toda vez que a função dccthread_yield é chamada, é ativado o contexto da próxima thread da fila.
   Além disso, também foi necessário a criação de uma lista auxiliar, uma lista de threads que estão esperando outras threads terminarem. Dessa forma, toda vez que uma thread termina sua execução, realiza-se uma busca na fila de espera, caso alguma thread estivesse esperando esta, ela retorna para a fila de prontos.

6. Descreva o mecanismo utilizado para sincronizar chamadas de
   dccthread_yield e disparos do temporizador (parte 4).
   Para sincronizar chamadas de dccthread_yield e disparos do temporizador foram utilizadas duas bibliotecas, time.h e signals.h. Dessa forma, o programa possui um timer que transmite um sinal de 10 em 10 milissegundos, toda vez que esse sinal é transmitido, é chamada a função dccthread_yield e é realizada a troca de contexto de threads.
