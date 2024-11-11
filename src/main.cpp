#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <semaphore>
#include <atomic>
#include <chrono>
#include <random>

// Global variables for synchronization
constexpr int NUM_JOGADORES = 4;
std::counting_semaphore<NUM_JOGADORES> cadeira_sem(NUM_JOGADORES - 1); // Inicia com n-1 cadeiras, capacidade máxima n
std::condition_variable music_cv;
std::mutex music_mutex;
std::atomic<bool> musica_parada{false};
std::atomic<bool> jogo_ativo{true};

/*
 * Uso básico de um counting_semaphore em C++:
 * 
 * O `std::counting_semaphore` é um mecanismo de sincronização que permite controlar o acesso a um recurso compartilhado 
 * com um número máximo de acessos simultâneos. Neste projeto, ele é usado para gerenciar o número de cadeiras disponíveis.
 * Inicializamos o semáforo com `n - 1` para representar as cadeiras disponíveis no início do jogo. 
 * Cada jogador que tenta se sentar precisa fazer um `acquire()`, e o semáforo permite que até `n - 1` jogadores 
 * ocupem as cadeiras. Quando todos os assentos estão ocupados, jogadores adicionais ficam bloqueados até que 
 * o coordenador libere o semáforo com `release()`, sinalizando a eliminação dos jogadores.
 * O método `release()` também pode ser usado para liberar múltiplas permissões de uma só vez, por exemplo: `cadeira_sem.release(3);`,
 * o que permite destravar várias threads de uma só vez, como é feito na função `liberar_threads_eliminadas()`.
 *
 * Métodos da classe `std::counting_semaphore`:
 * 
 * 1. `acquire()`: Decrementa o contador do semáforo. Bloqueia a thread se o valor for zero.
 *    - Exemplo de uso: `cadeira_sem.acquire();` // Jogador tenta ocupar uma cadeira.
 * 
 * 2. `release(int n = 1)`: Incrementa o contador do semáforo em `n`. Pode liberar múltiplas permissões.
 *    - Exemplo de uso: `cadeira_sem.release(2);` // Libera 2 permissões simultaneamente.
 */

// Classes
class JogoDasCadeiras {
public:
    JogoDasCadeiras(int num_jogadores)
        : num_jogadores(num_jogadores), cadeiras(num_jogadores - 1) {}

    void iniciar_rodada() {
        // TODO: Inicia uma nova rodada, removendo uma cadeira e ressincronizando o semáforo

        musica_parada = false;
        std::cout << "Nova rodada iniciada com " << cadeiras << " cadeiras" << " e " << cadeiras+1 << " jogadores" << std::endl;
        cadeiras--;
        std::counting_semaphore<NUM_JOGADORES> cadeira_sem(cadeiras);
    }

    void parar_musica() {
        // TODO: Simula o momento em que a música para e notifica os jogadores via variável de condição
        std::lock_guard<std::mutex> lock(music_mutex);
        musica_parada = true;
        music_cv.notify_all();
        std::cout << "Música parou!" << std::endl;
    }


    void eliminar_jogador(int jogador_id) {
        // TODO: Elimina um jogador que não conseguiu uma cadeira
        std::cout << "Jogador " << jogador_id << " foi eliminado!" << std::endl;
        num_jogadores--;
        if (num_jogadores <= 1) jogo_ativo = false;
    }

    void exibir_estado() {
        // TODO: Exibe o estado atual das cadeiras e dos jogadores
        std::cout << "Estado atual: " << num_jogadores << " jogadores, " << cadeiras << " cadeiras restantes." << std::endl;
    }

private:
    int num_jogadores;
    int cadeiras;
};

class Jogador {
public:
    Jogador(int id, JogoDasCadeiras& jogo)
        : id(id), jogo(jogo), eliminado(false) {}

    void tentar_ocupar_cadeira() {
        // TODO: Tenta ocupar uma cadeira utilizando o semáforo contador quando a música para (aguarda pela variável de condição)
        std::unique_lock<std::mutex> lock(music_mutex);
        music_cv.wait(lock, [] { return musica_parada.load(); });

        if (cadeira_sem.try_acquire()) {
            std::cout << "Jogador " << id << " conseguiu uma cadeira!" << std::endl;
        } else {
            eliminado = true;
        }
    }

    void verificar_eliminacao() {
        // TODO: Verifica se foi eliminado após ser destravado do semáforo
        if (eliminado) {
            jogo.eliminar_jogador(id);
        }
    }

    void joga() {
        // TODO: Aguarda a música parar usando a variavel de condicao
        
        // TODO: Tenta ocupar uma cadeira

        
        // TODO: Verifica se foi eliminado
        while (jogo_ativo.load()) {
            tentar_ocupar_cadeira();
            verificar_eliminacao();
            if (eliminado) break;
        }

    }

private:
    int id;
    JogoDasCadeiras& jogo;
    bool eliminado;
};

class Coordenador {
public:
    Coordenador(JogoDasCadeiras& jogo)
        : jogo(jogo) {}

    void iniciar_jogo() {
        // TODO: Começa o jogo, dorme por um período aleatório, e então para a música, sinalizando os jogadores 
        while (jogo_ativo) {
            jogo.iniciar_rodada();
            std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 3000 + 1000));
            jogo.parar_musica();

            std::this_thread::sleep_for(std::chrono::seconds(1));
            liberar_threads_eliminadas();
            if (NUM_JOGADORES <= 1) {
                jogo_ativo = false;
            }
        } 
    }

    void liberar_threads_eliminadas() {
        // Libera múltiplas permissões no semáforo para destravar todas as threads que não conseguiram se sentar
        cadeira_sem.release(NUM_JOGADORES - 1); // Libera o número de permissões igual ao número de jogadores que ficaram esperando
    }

private:
    JogoDasCadeiras& jogo;
};

// Main function
int main() {
    JogoDasCadeiras jogo(NUM_JOGADORES);
    Coordenador coordenador(jogo);
    std::vector<std::thread> jogadores;

    // Criação das threads dos jogadores
    std::vector<Jogador> jogadores_objs;
    for (int i = 1; i <= NUM_JOGADORES; ++i) {
        jogadores_objs.emplace_back(i, jogo);
    }

    for (int i = 0; i < NUM_JOGADORES; ++i) {
        jogadores.emplace_back(&Jogador::joga, &jogadores_objs[i]);
    }

    // Thread do coordenador
    std::thread coordenador_thread(&Coordenador::iniciar_jogo, &coordenador);

    // Esperar pelas threads dos jogadores
    for (auto& t : jogadores) {
        if (t.joinable()) {
            t.join();
        }
    }

    // Esperar pela thread do coordenador
    if (coordenador_thread.joinable()) {
        coordenador_thread.join();
    }

    std::cout << "Jogo das Cadeiras finalizado." << std::endl;
    return 0;
}

