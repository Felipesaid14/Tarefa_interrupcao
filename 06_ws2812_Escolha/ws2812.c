#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/timer.h"
#include "ws2812.pio.h"

// Definições
#define IS_RGBW false
#define NUM_LEDS 25
#define WS2812_PIN 7
#define BOTAO_A_PIN 5
#define BOTAO_B_PIN 6
#define LED_VERMELHO_PIN 13
#define DEBOUNCE_MS 250

// Variáveis globais
int numero_atual = 0;
uint32_t buffer_leds[NUM_LEDS] = {0};
volatile bool led_red_state = false;
volatile bool atualizar_display = false;
volatile uint32_t ultimo_tempo_botao_a = 0;
volatile uint32_t ultimo_tempo_botao_b = 0;

// Função para enviar um pixel para a matriz de LEDs
static inline void enviar_pixel(uint32_t pixel_grb) {
    pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
    sleep_us(50); // Pequeno delay para estabilidade
}

// Função para converter RGB para GRB
static inline uint32_t converter_rgb_para_grb(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)(r) << 8) | ((uint32_t)(g) << 16) | (uint32_t)(b);
}

// Configura a matriz WS2812
void configurar_matriz_leds() {
    PIO pio = pio0;
    int sm = 0;
    uint offset = pio_add_program(pio, &ws2812_program);
    ws2812_program_init(pio, sm, offset, WS2812_PIN, 800000, IS_RGBW);
}

// Atualiza a matriz LED
void atualizar_matriz(int numero) {
    static const uint32_t padroes_numeros[10][NUM_LEDS] = {
        //0
        {1,1,1,1,1, 
         1,0,0,0,1, 
         1,0,0,0,1, 
         1,0,0,0,1, 
         1,1,1,1,1},

        //1
        {0,1,1,1,0, 
         0,0,1,0,0,  
         0,0,1,0,0,  
         0,1,1,0,0,  
         0,0,1,0,0},

        //2
        {1,1,1,1,1,
         1,0,0,0,0, 
         1,1,1,1,1, 
         0,0,0,0,1, 
         1,1,1,1,1},

        //3
        {1,1,1,1,1, 
         0,0,0,0,1, 
         1,1,1,1,1, 
         0,0,0,0,1, 
         1,1,1,1,1},

        //4
        {1,0,0,0,0, 
         0,0,0,0,1, 
         1,1,1,1,1, 
         1,0,0,0,1, 
         1,0,0,0,1},

        //5
        {1,1,1,1,1, 
         0,0,0,0,1,
         1,1,1,1,1, 
         1,0,0,0,0, 
         1,1,1,1,1},
        
        //6
        {1,1,1,1,1, 
         1,0,0,0,1, 
         1,1,1,1,1, 
         1,0,0,0,0, 
         1,1,1,1,1},   
        
        //7
        {0,0,0,1,0, 
         0,0,1,0,0, 
         0,1,0,0,0, 
         0,0,0,0,1, 
         1,1,1,1,1},

        //8
        {1,1,1,1,1, 
         1,0,0,0,1, 
         1,1,1,1,1, 
         1,0,0,0,1, 
         1,1,1,1,1},
         
         //9
        {1,1,1,1,1, 
         0,0,0,0,1, 
         1,1,1,1,1, 
         1,0,0,0,1, 
         1,1,1,1,1}
    };
    
    for (int i = 0; i < NUM_LEDS; i++) {
        buffer_leds[i] = padroes_numeros[numero][i] ? converter_rgb_para_grb(0, 0, 200) : 0;
    }
    
    for (int i = 0; i < NUM_LEDS; i++) {
        enviar_pixel(buffer_leds[i]);
    }
}

// Função de interrupção para os botões
void tratar_botao(uint gpio, uint32_t events) {
    uint32_t tempo_atual = to_ms_since_boot(get_absolute_time());
    if (gpio == BOTAO_A_PIN && (tempo_atual - ultimo_tempo_botao_a > DEBOUNCE_MS)) {
        ultimo_tempo_botao_a = tempo_atual;
        numero_atual = (numero_atual + 1) % 10;
        atualizar_display = true;
    } else if (gpio == BOTAO_B_PIN && (tempo_atual - ultimo_tempo_botao_b > DEBOUNCE_MS)) {
        ultimo_tempo_botao_b = tempo_atual;
        numero_atual = (numero_atual - 1 + 10) % 10;
        atualizar_display = true;
    }
}

// Callback da interrupção do timer para piscar o LED vermelho
bool piscar_led_vermelho(struct repeating_timer *t) {
    led_red_state = !led_red_state;
    gpio_put(LED_VERMELHO_PIN, led_red_state);
    return true;
}

int main() {
    stdio_init_all();
    sleep_ms(1000);
    configurar_matriz_leds();
    gpio_init(BOTAO_A_PIN);
    gpio_set_dir(BOTAO_A_PIN, GPIO_IN);
    gpio_pull_up(BOTAO_A_PIN);
    gpio_set_irq_enabled_with_callback(BOTAO_A_PIN, GPIO_IRQ_EDGE_FALL, true, tratar_botao);
    gpio_init(BOTAO_B_PIN);
    gpio_set_dir(BOTAO_B_PIN, GPIO_IN);
    gpio_pull_up(BOTAO_B_PIN);
    gpio_set_irq_enabled_with_callback(BOTAO_B_PIN, GPIO_IRQ_EDGE_FALL, true, tratar_botao);
    gpio_init(LED_VERMELHO_PIN);
    gpio_set_dir(LED_VERMELHO_PIN, GPIO_OUT);
    struct repeating_timer timer;
    add_repeating_timer_ms(-100, piscar_led_vermelho, NULL, &timer);
    atualizar_matriz(numero_atual);
    while (1) {
        if (atualizar_display) {
            atualizar_matriz(numero_atual);
            atualizar_display = false;
        }
        tight_loop_contents();
    }
    return 0;
}