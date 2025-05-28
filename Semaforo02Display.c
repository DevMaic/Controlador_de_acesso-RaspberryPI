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
#define BOTAO_B 6 // Gera evento
#define MAX_PERSONS 10 // Número máximo de pessoas

SemaphoreHandle_t xSemaphorContadorIn; // Semáforo para entrada de pessoas
SemaphoreHandle_t xSemaphorContadorOut; // Semáforo para saída de pessoas
SemaphoreHandle_t xSemaphorBinaryReset; // Semáforo binário para reset do contador

ssd1306_t ssd;
SemaphoreHandle_t xMutexSem;

int totalPersons = 0; // Variável para controlar o número de pessoas ativas
char buffer[32]; // Variável utilizada para armazenar strings temporárias
int lastTime = 0; // Variável para armazenar o tempo do último evento
bool buzzerState = false; // Estado do buzzer para o limite mínimo de pessoas
bool buzzerState1 = false; // Estado do buzzer para o limite máximo de pessoas
bool maxLimitBeeped = false; // Variável para impedir que o buzzer toque repetidamente quando o limite máximo é atingido
bool zeroBeeped = false; // Variável para impedir que o buzzer toque repetidamente quando o número de pessoas chega a zero

 // Task de entrada
void vInTask(void *params) {
    while (true) {
        // Entra somente quando há uma task de entrada na fila e ao mesmo tempo o número de usuários é menor que o máximo permitido
        if(xSemaphoreTake(xSemaphorContadorIn, portMAX_DELAY) == pdTRUE && totalPersons < MAX_PERSONS) {
            // Tenta adquirir o mutex para garantir acesso exclusivo à variável totalPersons e display
            if (xSemaphoreTake(xMutexSem, portMAX_DELAY) == pdTRUE) {
                totalPersons++;
                ssd1306_fill(&ssd, false); // Limpa o display
                ssd1306_rect(&ssd, 3, 3, 122, 60, true, false);      // Desenha um retângulo
                ssd1306_line(&ssd, 3, 37, 123, 37, true);           // Desenha uma linha
                ssd1306_draw_string(&ssd, "", 8, 6); // Desenha uma string
                ssd1306_draw_string(&ssd, "", 20, 16);  // Desenha uma string
                sprintf(buffer, "Usuarios: %d", totalPersons); // Formata a string com o número de usuários
                ssd1306_draw_string(&ssd, buffer, 10, 18);  // Desenha uma string
                sprintf(buffer, "Vagas: %d", (MAX_PERSONS-totalPersons));
                ssd1306_draw_string(&ssd, buffer, 10, 45);    // Desenha uma string
                ssd1306_send_data(&ssd); // Envia os dados para o display OLED
                vTaskDelay(pdMS_TO_TICKS(1000)); // Aguarda 1 tick

                xSemaphoreGive(xMutexSem); // Libera o mutex para outras tarefas
            }
        }
    }
}

// Task de saída
void vOutTask(void *params) {
    while(true) {
        // Sai somente quando há uma task de saída na fila e ao mesmo tempo o número de usuários é maior que zero
        if(xSemaphoreTake(xSemaphorContadorOut, portMAX_DELAY) == pdTRUE && totalPersons > 0) {
            // Tenta adquirir o mutex para garantir acesso exclusivo à variável totalPersons e display
            if(xSemaphoreTake(xMutexSem, portMAX_DELAY) == pdTRUE) {
                totalPersons--;
                ssd1306_fill(&ssd, false); // Limpa o display
                ssd1306_rect(&ssd, 3, 3, 122, 60, true, false);      // Desenha um retângulo
                ssd1306_line(&ssd, 3, 37, 123, 37, true);           // Desenha uma linha
                ssd1306_draw_string(&ssd, "", 8, 6); // Desenha uma string
                ssd1306_draw_string(&ssd, "", 20, 16);  // Desenha uma string
                sprintf(buffer, "Usuarios: %d", totalPersons); // Formata a string com o número de usuários
                ssd1306_draw_string(&ssd, buffer, 10, 18);  // Desenha uma string
                sprintf(buffer, "Vagas: %d", (MAX_PERSONS-totalPersons));
                ssd1306_draw_string(&ssd, buffer, 10, 45);    // Desenha uma string
                ssd1306_send_data(&ssd); // Envia os dados para o display OLED
                vTaskDelay(pdMS_TO_TICKS(1000)); // Aguarda 1 tick

                xSemaphoreGive(xMutexSem); // Libera o mutex para outras tarefas
            }
        }
    }
}

// Task de reset do contador
void vResetTask(void *params) {
    while (true) {
        // Aguarda o semáforo binário de reset ser liberado
        if(xSemaphoreTake(xSemaphorBinaryReset, portMAX_DELAY) == pdTRUE) {
            // Zera os semáforos manualmente rodando e dando take enquanto existirem tarefas na fila
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
            ssd1306_send_data(&ssd); // Envia os dados para o display OLED

            printf("Reset executado com sucesso\n");
        }
    }
}

// Task que controla o estado do buzzer e verifica os limites de pessoas
void vStateControllerTask(void *params) {
    while(true) {
        if (totalPersons < MAX_PERSONS) {
            maxLimitBeeped = false; // libera para tocar de novo futuramente
        }
        if (totalPersons > 0) {
            zeroBeeped = false; // libera para tocar de novo futuramente
        }
        if (totalPersons == MAX_PERSONS && !maxLimitBeeped) {
            buzzerState1 = true; // Ativa o buzzer para o limite máximo de pessoas
            maxLimitBeeped = true; // Impede que o buzzer toque repetidamente quando o limite máximo é atingido
        }
        if (totalPersons == 0 && !zeroBeeped) {
            buzzerState = true; // Ativa o buzzer para o limite mínimo de pessoas
            zeroBeeped = true; // Impede que o buzzer toque repetidamente quando o número de pessoas chega a zero
        }
        vTaskDelay(pdMS_TO_TICKS(10)); // Aguarda 10 ms antes de verificar novamente
    }
}

// Função de interrupção para os botões
void gpio_irq_handler(uint gpio, uint32_t events) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint32_t currentTime = to_us_since_boot(get_absolute_time());

    if(currentTime - lastTime > 200000) { // Debounce
        if (gpio == BOTAO_A) { // Se o botão A for pressionado aumenta o contador
            xSemaphoreGiveFromISR(xSemaphorContadorIn, &xHigherPriorityTaskWoken);
        } else if (gpio == BOTAO_B) { // Se o botão B for pressionado diminui o contador
            xSemaphoreGiveFromISR(xSemaphorContadorOut, &xHigherPriorityTaskWoken);
        } else if(gpio == 22) { // Se o botão 22 for pressionado reseta o contador
            xSemaphoreGiveFromISR(xSemaphorBinaryReset, &xHigherPriorityTaskWoken); // Libera o semáforo de reset
        }
        lastTime = currentTime; // Atualiza o tempo do último evento
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken); // Troca o contexto da tarefa se necessário
}

// Task que controla os LEDs de acordo com o número de pessoas
void vLedTask() {
    while (true) {
        if(totalPersons == 0) {
            gpio_put(11, false);
            gpio_put(12, true);
            gpio_put(13, false);
        } else if(totalPersons > 0 && totalPersons <= MAX_PERSONS-2) {
            gpio_put(11, true);
            gpio_put(12, false);
            gpio_put(13, false); 
        } else if(totalPersons == MAX_PERSONS-1) {
            gpio_put(11, true);
            gpio_put(12, false);
            gpio_put(13, true);
        } else {
            gpio_put(11, false);
            gpio_put(12, false);
            gpio_put(13, true);
        }
        vTaskDelay(1); // Aguarda 1 tick antes de verificar novamente
    }
}

// Função para tocar o buzzer
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

// Task que controla o buzzer
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
        // Verifica o estado dos buzzers e toca se necessário
        if (buzzerState) {
            tocar_buzzer(slice_num, wrap, 2, 50, 50);
            buzzerState = false; // Reseta o estado do buzzer para não tocar infinitamente
        }
        if (buzzerState1) {
            tocar_buzzer(slice_num, wrap, 1, 50, 0);
            buzzerState1 = false; // Reseta o estado do buzzer para não tocar infinitamente
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// Função de inicialização dos LEDs
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

    xSemaphorContadorIn = xSemaphoreCreateCounting(10, 0); // Semáforo para entrada de pessoas
    xSemaphorContadorOut = xSemaphoreCreateCounting(10, 0); // Semáforo para saída de pessoas
    xSemaphorBinaryReset = xSemaphoreCreateBinary(); // Semáforo binário para reset do contador
    xMutexSem = xSemaphoreCreateMutex(); // Mutex para proteger o acesso à variável totalPersons e ao display

    // Iniciliza o display pela primeira vez, independente de qualquer evento
    // Esse trecho existe pra que o displlay não comece desligado
    ssd1306_fill(&ssd, false);                          // Limpa o display
    ssd1306_rect(&ssd, 3, 3, 122, 60, true, false);      // Desenha um retângulo
    ssd1306_line(&ssd, 3, 37, 123, 37, true);           // Desenha uma linha
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
    xTaskCreate(vStateControllerTask, "Task", configMINIMAL_STACK_SIZE + 128, NULL, 1, NULL);

    vTaskStartScheduler();
    panic_unsupported();
}
