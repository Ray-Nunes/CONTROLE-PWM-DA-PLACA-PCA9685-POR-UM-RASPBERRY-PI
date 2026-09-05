# CONTROLE-PWM-DA-PLACA-PCA9685-POR-UM-RASPBERRY-PI

# `03_servo_classe.cpp` — Controle de Ângulo de Servo via PCA9685

## Objetivo

Este módulo converte um ângulo desejado (0° a 180°) em um sinal PWM válido, 
escrevendo diretamente nos registradores de um canal da placa PCA9685 via I2C. 
É a camada que transforma "quero o servo em 90°" em bytes reais transmitidos 
ao hardware. Atenção!!! INICIAR O BARRAMENTO VIA TERMINAL |  sudo i2cset -y 1 0x40 0x00 0x00

## Contexto técnico

Servos motores hobby são controlados por um sinal PWM (Pulse Width Modulation) 
de 50Hz — ou seja, um pulso que se repete a cada 20ms. O ângulo do servo é 
determinado pela **largura** desse pulso dentro de cada ciclo: aproximadamente 
0.5ms corresponde a 0°, e aproximadamente 2.5ms corresponde a 180° (a faixa 
exata varia por fabricante/modelo, nesse projeto, os valores reais foram 
obtidos por teste, não por valor genérico de datasheet).

A PCA9685 não trabalha diretamente em milissegundos. Internamente, ela divide 
cada ciclo de 20ms em **4096 posições discretas** (resolução de 12 bits), e 
cada canal de servo é controlado por dois pares de registradores:

| Registrador       | Função                                  |
|                ---|                                      ---|
| `ON_L`  / `ON_H`  | Posição (0–4095) em que o pulso liga    |
| `OFF_L` / `OFF_H` | Posição (0–4095) em que o pulso desliga |

Neste projeto, `ON` é sempre fixado em `0` (o pulso liga no início do ciclo), 
e apenas `OFF` varia para controlar o ângulo.

## O problema central resolvido neste arquivo

Um registrador I2C armazena apenas 1 byte (0–255), mas a posição pode chegar 
a 4095, um valor que não cabe em um único byte. A solução, padrão em 
programação de hardware, é dividir o valor em duas partes:

- **Byte baixo (`OFF_L`)**: os 8 bits menos significativos
- **Byte alto (`OFF_H`)**: os bits restantes, deslocados para a posição correta

Isso é resolvido com duas operações de manipulação de bits:

```cpp
OFF_L = posicao & 0xFF;         // mantém apenas os últimos 8 bits
OFF_H = (posicao >> 8) & 0x0F;  // desloca 8 bits e mantém os 4 restantes
```

## Funções do arquivo

### `anguloParaPosicao(int angulo_graus)`

Converte um ângulo em graus para a posição correspondente na régua de 4096, 
usando uma regra de três linear:

```cpp
int anguloParaPosicao(int angulo_graus) {
    const int posicao_minima = 136;  // ~0.66ms, valor extremo testado empiricamente
    const int posicao_maxima = 562;  // ~2.74ms, valor extremo testado empiricamente

    int posicao = posicao_minima + (angulo_graus * (posicao_maxima - posicao_minima)) / 180;
    return posicao;
}
```

**Fórmula:**


**Exemplo de cálculo real (180°):**


**Nota de implementação:** a multiplicação é feita antes da divisão de propósito. 
Como C++ trunca a parte decimal em divisões entre inteiros, dividir primeiro 
resultaria em perda de precisão para ângulos intermediários.

**Nota metodológica:** os valores `136` e `562` não vêm de uma fórmula teórica 
de datasheet, foram obtidos testando os extremos físicos reais dos servos 
utilizados (MG996R e SG90) via terminal, com `i2cset`, até encontrar o curso 
mecânico completo sem forçar o motor.

### `moverServo(int canal, int angulo_graus)`

Localiza os registradores do canal desejado e escreve os quatro valores 
necessários (ON_L, ON_H, OFF_L, OFF_H):

```cpp
void moverServo(int canal, int angulo_graus) {
    int posicao_off = anguloParaPosicao(angulo_graus);
    unsigned char registrador_base = LED0_ON_L + (4 * canal);

    escreverRegistrador(registrador_base, 0);                              // ON_L
    escreverRegistrador(registrador_base + 1, 0);                          // ON_H
    escreverRegistrador(registrador_base + 2, posicao_off & 0xFF);         // OFF_L
    escreverRegistrador(registrador_base + 3, (posicao_off >> 8) & 0x0F);  // OFF_H
}
```

**Cálculo do endereço do registrador base:**

Cada canal (0 a 15) ocupa um bloco de 4 registradores consecutivos, calculado por:


| Canal | Registrador base |
|---|---|
| 0 | `0x06` |
| 1 | `0x0A` |
| 2 | `0x0E` |
| 15 | `0x42` |

**Exemplo de execução completa (canal 0, ângulo 180°):**

Entrada: canal = 0, angulo_graus = 180 │ anguloParaPosicao(180) │ → posicao_off = 562 │ registrador_base │ → 0x06 + (4×0) = 6 (0x06) │ OFF_L = 562 & 0xFF → 50 (0x32) OFF_H = (562>>8) & 0x0F → 2 (0x02) │ Escritas I2C realizadas: escreverRegistrador(0x06, 0) → ON_L escreverRegistrador(0x07, 0) → ON_H escreverRegistrador(0x08, 0x32) → OFF_L escreverRegistrador(0x09, 0x02) → OFF_H


## Detalhamento das operações bit a bit

### `posicao_off & 0xFF` — extraindo o byte baixo

0000 0010 0011 0010 (562 em binário, 16 bits) & 0000 0000 1111 1111 (0xFF)

0000 0000 0011 0010 = 50 = 0x32


A máscara `0xFF` "corta" tudo além dos 8 bits menos significativos, 
preservando apenas a parte que cabe em um byte.

### `(posicao_off >> 8) & 0x0F` — extraindo o byte alto

Passo 1 — shift à direita por 8 posições: 0000 0010 0011 0010 >>8→ 0000 0000 0000 0010 (= 2)

Passo 2 — máscara para manter apenas os 4 bits relevantes: 0000 0000 0000 0010 & 0000 0000 0000 1111 (0x0F)

0000 0000 0000 0010 (= 2 = 0x02)

O shift `>> 8` desloca os bits mais significativos para a posição das 
unidades (equivalente a dividir por 256). A máscara `0x0F` garante que 
apenas os 4 bits relevantes sejam mantidos, já que a posição máxima 
(4095) ocupa exatamente 12 bits no total — 8 no byte baixo, 4 no byte alto.


## Limitações conhecidas (para desenvolvimento futuro)

- `anguloParaPosicao()` não valida se `angulo_graus` está dentro de 0–180; 
  valores fora dessa faixa produzem posições fora do intervalo seguro do 
  servo sem aviso.
- `posicao_minima`/`posicao_maxima` estão fixados como constantes locais; 
  em uma versão orientada a objetos, esses valores deveriam ser parâmetros 
  configuráveis por instância de servo, já que servos diferentes têm 
  faixas reais diferentes (confirmado empiricamente neste projeto: SG90 
  e MG996R exigiram ajustes distintos da faixa genérica inicial).
- Não há verificação de erro no retorno de `read()`, apenas em `write()`.

## Dependências

- `<fcntl.h>`, `<unistd.h>`, `<sys/ioctl.h>`, `<linux/i2c-dev.h>` — chamadas 
  de sistema Linux para acesso ao barramento I2C
- `<cmath>` — função `round()`, usada no cálculo do prescale de frequência
