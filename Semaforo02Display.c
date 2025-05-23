#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "lib/ssd1306.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "pico/bootrom.h"
#include "stdio.h"

#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define ENDERECO 0x3C

#define BOTAO_A 5 // Gera evento
#define BOTAO_B 6 // BOOTSEL

ssd1306_t ssd;
SemaphoreHandle_t xContadorSem;
uint16_t eventosProcessados = 0;

// ISR do botão A (incrementa o semáforo de contagem)
void gpio_callback(uint gpio, uint32_t events) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(xContadorSem, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// Tarefa que consome eventos e mostra no display
void vContadorTask(void *params) {
    char buffer[32];

    // Tela inicial
    ssd1306_fill(&ssd, 0);
    ssd1306_draw_string(&ssd, "Aguardando ", 5, 25);
    ssd1306_draw_string(&ssd, "  evento...", 5, 34);
    ssd1306_send_data(&ssd);

    while (true) {
        // Aguarda semáforo (um evento)
        if (xSemaphoreTake(xContadorSem, portMAX_DELAY) == pdTRUE) {
            eventosProcessados++;

            // Atualiza display com a nova contagem
            ssd1306_fill(&ssd, 0);
            sprintf(buffer, "Eventos: %d", eventosProcessados);
            ssd1306_draw_string(&ssd, "Evento ", 5, 10);
            ssd1306_draw_string(&ssd, "recebido!", 5, 19);
            ssd1306_draw_string(&ssd, buffer, 5, 44);
            ssd1306_send_data(&ssd);

            // Simula tempo de processamento
            vTaskDelay(pdMS_TO_TICKS(1500));

            // Retorna à tela de espera
            ssd1306_fill(&ssd, 0);
            ssd1306_draw_string(&ssd, "Aguardando ", 5, 25);
            ssd1306_draw_string(&ssd, "  evento...", 5, 34);
            ssd1306_send_data(&ssd);
        }
    }
}

// ISR para BOOTSEL e botão de evento
void gpio_irq_handler(uint gpio, uint32_t events) {
    if (gpio == BOTAO_B) {
        reset_usb_boot(0, 0);
    } else if (gpio == BOTAO_A) {
        gpio_callback(gpio, events);
    }
}

int main() {
    stdio_init_all();

    // Inicialização do display
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, ENDERECO, I2C_PORT);
    ssd1306_config(&ssd);
    ssd1306_send_data(&ssd);

    // Configura os botões
    gpio_init(BOTAO_A);
    gpio_set_dir(BOTAO_A, GPIO_IN);
    gpio_pull_up(BOTAO_A);

    gpio_init(BOTAO_B);
    gpio_set_dir(BOTAO_B, GPIO_IN);
    gpio_pull_up(BOTAO_B);

    gpio_set_irq_enabled_with_callback(BOTAO_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled(BOTAO_B, GPIO_IRQ_EDGE_FALL, true);

    // Cria semáforo de contagem (máximo 10, inicial 0)
    xContadorSem = xSemaphoreCreateCounting(10, 0);

    // Cria tarefa
    xTaskCreate(vContadorTask, "ContadorTask", configMINIMAL_STACK_SIZE + 128, NULL, 1, NULL);

    vTaskStartScheduler();
    panic_unsupported();
}
