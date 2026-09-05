#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <iostream>
#include <cmath>

const unsigned char MODE1 = 0x00;
const unsigned char PRE_SCALE = 0xFE;
const unsigned char LED0_ON_L = 0x06; // registrador base do canal 0

int arquivo;

void escreverRegistrador(unsigned char registrador, unsigned char valor) {
    unsigned char buffer[2] = { registrador, valor };
    if(write(arquivo, buffer, 2 ) != 2){
        std::cerr << "Erro ao escrever" << std::endl;
    }
    
}

unsigned char lerRegistrador(unsigned char registrador) {
    write(arquivo, &registrador, 1);
    unsigned char valor;
    read(arquivo, &valor, 1);
    return valor;
}

// Configura a PCA9685 para 50Hz (mesma lógica do arquivo 02)
void configurarFrequencia(float frequencia_desejada) {
    int prescale = round(25000000.0 / (4096 * frequencia_desejada)) - 1;

    unsigned char mode1_atual = lerRegistrador(MODE1);
    unsigned char mode1_sleep = (mode1_atual & 0x7F) | 0x10;
    escreverRegistrador(MODE1, mode1_sleep);
    escreverRegistrador(PRE_SCALE, prescale);
    escreverRegistrador(MODE1, mode1_atual);
    usleep(5000);
    escreverRegistrador(MODE1, mode1_atual | 0xA0);
}

// Converte ângulo (0 a 180 graus) em valor de "OFF" (posição na régua de 4096)
int anguloParaPosicao(int angulo_graus) {
    // pulso mínimo (~1ms) e máximo (~2ms), em posições da régua de 4096
    const int posicao_minima = 136; // aproximadamente 0.66ms
    const int posicao_maxima = 568; // aproximadamente 2.77ms

    // regra de três: mapeia 0-180 graus para posicao_minima-posicao_maxima
    int posicao = posicao_minima + (angulo_graus * (posicao_maxima - posicao_minima)) / 180;
    return posicao;
}

// Move um servo em um canal específico para um ângulo específico
void moverServo(int canal, int angulo_graus) {
    int posicao_off = anguloParaPosicao(angulo_graus);

    unsigned char registrador_base = LED0_ON_L + (4 * canal);

    // ON sempre em 0 (liga no início do ciclo)
    escreverRegistrador(registrador_base, 0);       // ON_L
    escreverRegistrador(registrador_base + 1, 0);    // ON_H

    // OFF = posicao calculada, dividida em byte baixo e alto
    escreverRegistrador(registrador_base + 2, posicao_off & 0xFF);        // OFF_L
    escreverRegistrador(registrador_base + 3, (posicao_off >> 8) & 0x0F); // OFF_H
}

int main() {
    const char* caminho_i2c = "/dev/i2c-1";
    const int endereco_pca9685 = 0x40;

    arquivo = open(caminho_i2c, O_RDWR);
    if (arquivo < 0) {
        std::cerr << "Erro ao abrir o barramento I2C" << std::endl;
        return 1;
    }
    if (ioctl(arquivo, I2C_SLAVE, endereco_pca9685) < 0) {
        std::cerr << "Erro ao conectar com o dispositivo 0x40" << std::endl;
        close(arquivo);
        return 1;
    }

    configurarFrequencia(50.0);
    std::cout << "PCA9685 configurada. Movendo servo do canal 0..." << std::endl;

    // Teste: move o servo do canal 0 para 0, depois 90, depois 180 graus
    moverServo(0, 0);
    std::cout << "Servo em 0 graus. Aguardando 2 segundos..." << std::endl;
    sleep(2);

    moverServo(0, 90);
    std::cout << "Servo em 90 graus. Aguardando 2 segundos..." << std::endl;
    sleep(2);

    moverServo(0, 180);
    std::cout << "Servo em 180 graus. Aguardando 2 segundos..." << std::endl;
    sleep(2);

    close(arquivo);
    return 0;
}