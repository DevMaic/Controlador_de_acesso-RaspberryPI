#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/pwm.h"
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
#define MAX_PERSONS 10 // Número máximo de pessoas

SemaphoreHandle_t xSemaphorContadorIn;
SemaphoreHandle_t xSemaphorContadorOut;
SemaphoreHandle_t xSemaphorBinaryReset;

ssd1306_t ssd;
SemaphoreHandle_t xMutexSem;

int totalPersons = 0;
char buffer[32];
int lastTime = 0; // Variável para armazenar o tempo do último evento
bool buzzerState = false; // Estado do buzzer
bool buzzerState1 = false; // Estado do buzzer para o limite máximo de pessoas
bool maxLimitBeeped = false;
bool zeroBeeped = false;

void vInTask(void *params) {
    while (true) {
        // Aguarda semáforo (um evento)
        if(xSemaphoreTake(xSemaphorContadorIn, portMAX_DELAY) == pdTRUE) {
            if (xSemaphoreTake(xMutexSem, portMAX_DELAY) == pdTRUE) {
                totalPersons++;
                ssd1306_fill(&ssd, false); // Limpa o display
                ssd1306_rect(&ssd, 3, 3, 122, 60, true, false);      // Desenha um retângulo
                ssd1306_line(&ssd, 3, 37, 123, 37, true);           // Desenha uma linha
                ssd1306_draw_string(&ssd, "", 8, 6); // Desenha uma string
                ssd1306_draw_string(&ssd, "", 20, 16);  // Desenha uma string
                sprintf(buffer, "Usuarios: %d", totalPersons);
                ssd1306_draw_string(&ssd, buffer, 10, 18);  // Desenha uma string
                sprintf(buffer, "Vagas: %d", (MAX_PERSONS-totalPersons));
                ssd1306_draw_string(&ssd, buffer, 10, 45);    // Desenha uma string
                ssd1306_send_data(&ssd);
                vTaskDelay(pdMS_TO_TICKS(1000)); // Aguarda 1 tick
                if (totalPersons == MAX_PERSONS && !maxLimitBeeped) {
                    buzzerState1 = true;
                    maxLimitBeeped = true;
                }
                if (totalPersons < MAX_PERSONS) {
                    maxLimitBeeped = false; // libera para tocar de novo futuramente
                }

                xSemaphoreGive(xMutexSem); // Libera o mutex para outras tarefas
            }
        }
    }
}

void vOutTask(void *params) {
    while(true) {
        // Aguarda semáforo (um evento)
        if(xSemaphoreTake(xSemaphorContadorOut, portMAX_DELAY) == pdTRUE) {
            if(xSemaphoreTake(xMutexSem, portMAX_DELAY) == pdTRUE) {
                totalPersons--;
                ssd1306_fill(&ssd, false); // Limpa o display
                ssd1306_rect(&ssd, 3, 3, 122, 60, true, false);      // Desenha um retângulo
                ssd1306_line(&ssd, 3, 37, 123, 37, true);           // Desenha uma linha
                ssd1306_draw_string(&ssd, "", 8, 6); // Desenha uma string
                ssd1306_draw_string(&ssd, "", 20, 16);  // Desenha uma string
                sprintf(buffer, "Usuarios: %d", totalPersons);
                ssd1306_draw_string(&ssd, buffer, 10, 18);  // Desenha uma string
                sprintf(buffer, "Vagas: %d", (MAX_PERSONS-totalPersons));
                ssd1306_draw_string(&ssd, buffer, 10, 45);    // Desenha uma string
                ssd1306_send_data(&ssd);
                vTaskDelay(pdMS_TO_TICKS(1000)); // Aguarda 1 tick

                if (totalPersons == 0 && !zeroBeeped) {
                    buzzerState = true;
                    zeroBeeped = true;
                }
                if (totalPersons > 0) {
                    zeroBeeped = false; // libera para tocar de novo futuramente
                }

                xSemaphoreGive(xMutexSem); // Libera o mutex para outras tarefas
            }
        }
    }
}

void vResetTask(void *params) {
    while (true) {
        if(xSemaphoreTake(xSemaphorBinaryReset, portMAX_DELAY) == pdTRUE) {
            // Zera os semáforos manualmente
            while(xSemaphoreTake(xSemaphorContadorIn, 0) == pdTRUE);
            while(xSemaphoreTake(xSemaphorContadorOut, 0) == pdTRUE);

            // Reseta o contador
            totalPersons = 0;

            ssd1306_fill(&ssd, false); // Limpa o display
            ssd1306_rect(&ssd, 3, 3, 122, 60, true, false);      // Desenha um retângulo
            ssd1306_line(&ssd, 3, 37, 123, 37, true);           // Desenha uma linha
            ssd1306_draw_string(&ssd, "", 8, 6); // Desenha uma string
            ssd1306_draw_string(&ssd, "", 20, 16);  // Desenha uma string
            sprintf(buffer, "Usuarios: %d", totalPersons);
            ssd1306_draw_string(&ssd, buffer, 10, 18);  // Desenha uma string
            sprintf(buffer, "Vagas: %d", (MAX_PERSONS-totalPersons));
            ssd1306_draw_string(&ssd, buffer, 10, 45);    // Desenha uma string
            ssd1306_send_data(&ssd);   

            printf("Reset executado com sucesso\n");
        }
    }
}

void gpio_irq_handler(uint gpio, uint32_t events) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint32_t currentTime = to_us_since_boot(get_absolute_time());

    if(currentTime - lastTime > 200000) { // Debounce
        if (gpio == BOTAO_A) {
            if(totalPersons<MAX_PERSONS) {
                xSemaphoreGiveFromISR(xSemaphorContadorIn, &xHigherPriorityTaskWoken);
            }
            printf("%d\n", uxSemaphoreGetCount(xSemaphorContadorIn)); // Exibe a contagem do semáforo
        } else if (gpio == BOTAO_B) {
            if(totalPersons>0) {
                xSemaphoreGiveFromISR(xSemaphorContadorOut, &xHigherPriorityTaskWoken);
            }
            printf("%d\n", uxSemaphoreGetCount(xSemaphorContadorOut)); // Exibe a contagem do semáforo
        } else if(gpio == 22) {                  // Incializa com pdFALSE
            xSemaphoreGiveFromISR(xSemaphorBinaryReset, &xHigherPriorityTaskWoken); // Libera o semáforo de reset
        }
        lastTime = currentTime; // Atualiza o tempo do último evento
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken); // Troca o contexto da tarefa se necessário
}

void vLedTask() {
    while (true) {
        if(totalPersons == 0) {
            gpio_put(11, false);
            gpio_put(12, true);
            gpio_put(13, false);
        } else if(totalPersons > 0 && totalPersons <= MAX_PERSONS-2) {
            gpio_put(11, true); // Desliga o LED se não houver pessoas
            gpio_put(12, false); // Desliga o LED azul
            gpio_put(13, false); // Liga o LED vermelho se houver pessoas
        } else if(totalPersons == MAX_PERSONS-1) {
            gpio_put(11, true); // Desliga o LED se não houver pessoas
            gpio_put(12, false); // Desliga o LED azul
            gpio_put(13, true); // Liga o LED se houver pessoas
        } else {
            gpio_put(11, false); // Desliga o LED verde se houver pessoas
            gpio_put(12, false); // Desliga o LED azul se houver pessoas
            gpio_put(13, true); // Liga o LED se houver pessoas
        }
        vTaskDelay(1); // Aguarda 100 ms antes de verificar novamente
    }
}

void tocar_buzzer(uint slice, uint wrap, int repeticoes, int duracao_ms, int pausa_ms) {
    for (int i = 0; i < repeticoes; i++) {
        gpio_set_function(21, GPIO_FUNC_PWM);
        pwm_set_wrap(slice, wrap);
        pwm_set_chan_level(slice, pwm_gpio_to_channel(21), wrap / 8); // 12,5%
        pwm_set_enabled(slice, true);
        vTaskDelay(pdMS_TO_TICKS(duracao_ms));

        pwm_set_enabled(slice, false);
        gpio_set_function(21, GPIO_FUNC_SIO);
        gpio_set_dir(21, GPIO_OUT);
        gpio_put(21, 0);
        vTaskDelay(pdMS_TO_TICKS(pausa_ms));
    }
}

void vBuzzerTask(void *params) {
    const uint slice_num = pwm_gpio_to_slice_num(21);
    const uint32_t clock = clock_get_hz(clk_sys);
    const uint32_t freq = 2000;
    const uint32_t divider16 = (clock << 4) / freq;
    const uint32_t wrap = (divider16 + (1 << 4) - 1) >> 4;

    gpio_set_function(21, GPIO_FUNC_PWM);
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 4.0f);
    pwm_init(slice_num, &config, false);

    while (true) {
        if (buzzerState) {
            tocar_buzzer(slice_num, wrap, 2, 50, 50);
            buzzerState = false;
        }

        if (buzzerState1) {
            tocar_buzzer(slice_num, wrap, 1, 50, 0);
            buzzerState1 = false;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void gpioInit(int led) {
    gpio_init(led);
    gpio_set_dir(led, GPIO_OUT);
    gpio_put(led, false); // Desliga o LED inicialmente
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

    gpio_init(22);
    gpio_set_dir(22, GPIO_IN);
    gpio_pull_up(22);

    gpioInit(11); // LED verde
    gpioInit(12); // LED azul
    gpioInit(13); // LED vermelho

    gpio_set_irq_enabled_with_callback(BOTAO_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled(BOTAO_B, GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(22     , GPIO_IRQ_EDGE_FALL, true);

    // Cria semáforo de contagem (máximo 10, inicial 0)
    xSemaphorContadorIn = xSemaphoreCreateCounting(10, 0);
    xSemaphorContadorOut = xSemaphoreCreateCounting(10, 0);
    xSemaphorBinaryReset = xSemaphoreCreateBinary();
    xMutexSem = xSemaphoreCreateMutex();

    bool cor = true;

    ssd1306_fill(&ssd, !cor);                          // Limpa o display

    ssd1306_rect(&ssd, 3, 3, 122, 60, cor, !cor);      // Desenha um retângulo
    ssd1306_line(&ssd, 3, 37, 123, 37, cor);           // Desenha uma linha
    ssd1306_draw_string(&ssd, "", 8, 6); // Desenha uma string
    ssd1306_draw_string(&ssd, "", 20, 16);  // Desenha uma string
    ssd1306_draw_string(&ssd, "Usuarios: 0", 10, 18);  // Desenha uma string
    ssd1306_draw_string(&ssd, "Vagas: 10", 10, 45);    // Desenha uma string
    ssd1306_send_data(&ssd);                           // Atualiza o display

    // Cria tarefas
    xTaskCreate(vInTask, "InTask", configMINIMAL_STACK_SIZE + 128, NULL, 1, NULL);
    xTaskCreate(vOutTask, "OutTask", configMINIMAL_STACK_SIZE + 128, NULL, 1, NULL);
    xTaskCreate(vResetTask, "ResetTask", configMINIMAL_STACK_SIZE + 128, NULL, 1, NULL);
    xTaskCreate(vLedTask, "LedTask", configMINIMAL_STACK_SIZE + 128, NULL, 1, NULL);
    xTaskCreate(vBuzzerTask, "BuzzerTask", configMINIMAL_STACK_SIZE + 128, NULL, 1, NULL);

    vTaskStartScheduler();
    panic_unsupported();
}
