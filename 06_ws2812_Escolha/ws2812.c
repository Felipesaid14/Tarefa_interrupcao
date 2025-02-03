#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/timer.h"
#include "ws2812.pio.h"

#define UTILIZA_RGBW false
#define TOTAL_LEDS 25
#define PINO_WS2812 7
#define PINO_BOTAO_CIMA 5
#define PINO_BOTAO_BAIXO 6
#define PINO_LED_ALERTA 13
#define PINO_LED_A 12
#define PINO_LED_B 11
#define TEMPO_DEBOUNCE 200

int valor_atual = 0;
uint32_t buffer_pixels[TOTAL_LEDS] = {0};
volatile bool estado_led_alerta = false;
volatile bool atualizar_tela = false;
volatile uint32_t ultimo_pressionamento_cima = 0;
volatile uint32_t ultimo_pressionamento_baixo = 0;

static inline uint32_t converter_cor(uint8_t vermelho, uint8_t verde, uint8_t azul) {
    return ((uint32_t)(vermelho) << 8) | ((uint32_t)(verde) << 16) | (uint32_t)(azul);
}

static inline void enviar_cor(uint32_t cor) {
    pio_sm_put_blocking(pio0, 0, cor << 8u);
    sleep_us(50);
}

void inicializar_painel_led() {
    PIO controlador = pio0;
    int estado = 0;
    uint deslocamento = pio_add_program(controlador, &ws2812_program);
    ws2812_program_init(controlador, estado, deslocamento, PINO_WS2812, 800000, UTILIZA_RGBW);
}

void alternar_led_alerta(struct repeating_timer *temporizador) {
    estado_led_alerta = !estado_led_alerta;
    gpio_put(PINO_LED_ALERTA, estado_led_alerta);
    return true;
}

void manipular_botoes(uint pino, uint32_t eventos) {
    uint32_t tempo_atual = to_ms_since_boot(get_absolute_time());
    if (pino == PINO_BOTAO_CIMA && (tempo_atual - ultimo_pressionamento_cima > TEMPO_DEBOUNCE)) {
        ultimo_pressionamento_cima = tempo_atual;
        valor_atual = (valor_atual + 1) % 10;
        atualizar_tela = true;
    } else if (pino == PINO_BOTAO_BAIXO && (tempo_atual - ultimo_pressionamento_baixo > TEMPO_DEBOUNCE)) {
        ultimo_pressionamento_baixo = tempo_atual;
        valor_atual = (valor_atual - 1 + 10) % 10;
        atualizar_tela = true;
    }
}

void exibir_numero(int numero) {
    static const uint32_t padrao_digitos[10][TOTAL_LEDS] = { ... };
    
    for (int i = 0; i < TOTAL_LEDS; i++) {
        buffer_pixels[i] = padrao_digitos[numero][i] ? converter_cor(0, 0, 200) : 0;
    }
    
    for (int i = 0; i < TOTAL_LEDS; i++) {
        enviar_cor(buffer_pixels[i]);
    }
}

int main() {
    stdio_init_all();
    sleep_ms(1000);
    
    inicializar_painel_led();
    
    gpio_init(PINO_BOTAO_CIMA);
    gpio_set_dir(PINO_BOTAO_CIMA, GPIO_IN);
    gpio_pull_up(PINO_BOTAO_CIMA);
    gpio_set_irq_enabled_with_callback(PINO_BOTAO_CIMA, GPIO_IRQ_EDGE_FALL, true, manipular_botoes);
    
    gpio_init(PINO_BOTAO_BAIXO);
    gpio_set_dir(PINO_BOTAO_BAIXO, GPIO_IN);
    gpio_pull_up(PINO_BOTAO_BAIXO);
    gpio_set_irq_enabled_with_callback(PINO_BOTAO_BAIXO, GPIO_IRQ_EDGE_FALL, true, manipular_botoes);
    
    gpio_init(PINO_LED_ALERTA);
    gpio_set_dir(PINO_LED_ALERTA, GPIO_OUT);
    
    struct repeating_timer temporizador;
    add_repeating_timer_ms(-100, alternar_led_alerta, NULL, &temporizador);
    
    exibir_numero(valor_atual);
    
    while (1) {
        if (atualizar_tela) {
            exibir_numero(valor_atual);
            atualizar_tela = false;
        }
        tight_loop_contents();
    }
    return 0;
}
