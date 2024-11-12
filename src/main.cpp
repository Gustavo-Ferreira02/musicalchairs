#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <semaphore>
#include <atomic>
#include <chrono>
#include <random>
#include <memory>

// Variáveis globais de sincronização
constexpr int NUM_JOGADORES = 4;
std::unique_ptr<std::counting_semaphore<NUM_JOGADORES>> cadeira_sem = std::make_unique<std::counting_semaphore<NUM_JOGADORES>>(NUM_JOGADORES - 1);
std::condition_variable music_cv;
std::mutex music_mutex;
std::atomic<bool> musica_parada{false};
std::atomic<bool> jogo_ativo{true};  // Variável para controle do loop de cada jogador

int tempoVariavel(int min, int max) {
    std::random_device numero;
    std::mt19937 gerar(numero());
    std::uniform_int_distribution<> distrubuicao(min, max);
    return distrubuicao(gerar);
}

class JogoDasCadeiras {
public:
    int num_jogadores;
    int cadeiras;

    JogoDasCadeiras(int num_jogadores)
        : num_jogadores(num_jogadores), cadeiras(num_jogadores - 1) {}

    void primeira_rodada() {
        std::cout << "Bem-vindo ao Jogo das Cadeiras Concorrente!" << std::endl;
        exibir_estado();
    }

    void iniciar_rodada() {
        // Inicia uma nova rodada, liberando permissões e atualizando o semáforo
        cadeira_sem->release(cadeiras);  // Libera permissões iguais ao número de cadeiras
        musica_parada = false;
        num_jogadores--;
        cadeiras--;
        exibir_estado();

        // Recria o semáforo com o novo número de permissões
        cadeira_sem.reset(new std::counting_semaphore<NUM_JOGADORES>(cadeiras));
    }

    void parar_musica() {
        // Para a música e notifica os jogadores para tentar se sentar
        musica_parada = true;
        music_cv.notify_all();
        std::cout << "Musica parou!" << std::endl;
    }

    void exibir_estado() {
        std::cout << "Iniciando rodada com " << num_jogadores << " jogadores, " << cadeiras << " cadeiras restantes." << std::endl;
        std::cout << std::endl;
    }

    void eliminar_jogador(int id) {
        if (!jogo_ativo)
        {
            std::cout << "Jogador " << id << " venceu!\n";
            return;
        }
        std::cout << "Jogador " << id << " foi eliminado!" << std::endl;
        std::cout << std::endl;
    }
};

class Jogador {
public:
    int id;
    JogoDasCadeiras& jogo;
    bool eliminado;

    Jogador(int id, JogoDasCadeiras& jogo)
        : id(id), jogo(jogo), eliminado(false) {}

    void tentar_ocupar_cadeira() {
        //std::unique_lock<std::mutex> lock(music_mutex);
        //music_cv.wait(lock, [] { return musica_parada.load(); });

        std::unique_lock<std::mutex> lock(music_mutex);
            music_cv.wait(lock);;

        if (eliminado) return;

        if (cadeira_sem->try_acquire()) {
            std::cout << "Jogador " << id << " pegou uma cadeira!" << std::endl;
        } else {
            eliminado = true;
            this->jogo.eliminar_jogador(id);
        }
    }

    void joga() {
        // Os jogadores tentarão jogar enquanto o jogo estiver ativo
        if (this->jogo.cadeiras == 0)
        {
            std::cout << "Jogador " << id << " ganhou!";
        }
        
        while (this->jogo.cadeiras > 0 && !eliminado) {
            //std::unique_lock<std::mutex> lock(music_mutex);
           // music_cv.wait(lock);;
            tentar_ocupar_cadeira();
        }
    }
};

class Coordenador {
public:
    JogoDasCadeiras& jogo;

    Coordenador(JogoDasCadeiras& jogo)
        : jogo(jogo) {}

    void iniciar_jogo() {
        int tempo = tempoVariavel(100, 150);
        this->jogo.primeira_rodada();

        while (jogo.cadeiras > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(tempo));
            this->jogo.parar_musica();
            tempo = tempoVariavel(1000, 2000);
            std::this_thread::sleep_for(std::chrono::milliseconds(tempo));
            this->jogo.iniciar_rodada();
        }

        // Finaliza o jogo ao término de todas as rodadas
        jogo_ativo = false;
        music_cv.notify_all();  // Notifica todos para terminar
    }
};

// Função principal
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

    // Espera pelas threads dos jogadores
    for (auto& t : jogadores) {
        if (t.joinable()) {
            t.join();
        }
    }

    // Espera pela thread do coordenador
    if (coordenador_thread.joinable()) {
        coordenador_thread.join();
    }

    std::cout << "Jogo das Cadeiras finalizado." << std::endl;
    return 0;
}
