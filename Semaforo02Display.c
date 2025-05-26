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
#define MAX_PERSONS 10 // Número máximo de pessoas

ssd1306_t ssd;
SemaphoreHandle_t xSemaphorContadorIn;
SemaphoreHandle_t xSemaphorContadorOut;
SemaphoreHandle_t xMutexSem;
int totalPersons = 0;
char buffer[32];
int lastTime = 0; // Variável para armazenar o tempo do último evento

void vInTask(void *params) {
    while (true) {
        // Aguarda semáforo (um evento)
        if(xSemaphoreTake(xSemaphorContadorIn, portMAX_DELAY) == pdTRUE) {
            if (xSemaphoreTake(xMutexSem, portMAX_DELAY) == pdTRUE) {
                totalPersons++;
                ssd1306_draw_string(&ssd, "               ", 5, 44);
                ssd1306_send_data(&ssd);
                sprintf(buffer, "Eventos: %d", totalPersons);
                ssd1306_draw_string(&ssd, buffer, 5, 44);
                ssd1306_send_data(&ssd);
                vTaskDelay(pdMS_TO_TICKS(1000)); // Aguarda 1 tick
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
                ssd1306_draw_string(&ssd, "               ", 5, 44);
                ssd1306_send_data(&ssd);
                sprintf(buffer, "Eventos: %d", totalPersons);
                ssd1306_draw_string(&ssd, buffer, 5, 44);
                ssd1306_send_data(&ssd);
                vTaskDelay(pdMS_TO_TICKS(1000)); // Aguarda 1 tick
                xSemaphoreGive(xMutexSem); // Libera o mutex para outras tarefas
            }
        }
    }
}

// // void vResetTask(void *params) {
// //     while (true) {
// //         // Aguarda mutex
// //         if (xSemaphoreTake(xMutexSem, portMAX_DELAY) == pdTRUE) {
            
// //         }

// //         vTaskDelay(1); // Aguarda 1 tick
// //     }
// // }

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
        } else if(gpio == 22) {
            reset_usb_boot(0, 0); // BOOTSEL
        }
        lastTime = currentTime; // Atualiza o tempo do último evento
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
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

    gpio_set_irq_enabled_with_callback(BOTAO_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled(BOTAO_B, GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(22     , GPIO_IRQ_EDGE_FALL, true);

    // Cria semáforo de contagem (máximo 10, inicial 0)
    xSemaphorContadorIn = xSemaphoreCreateCounting(10, 0);
    xSemaphorContadorOut = xSemaphoreCreateCounting(10, 0);
    xMutexSem = xSemaphoreCreateMutex();

    // Cria tarefas
    xTaskCreate(vInTask, "InTask", configMINIMAL_STACK_SIZE + 128, NULL, 1, NULL);
    xTaskCreate(vOutTask, "OutTask", configMINIMAL_STACK_SIZE + 128, NULL, 1, NULL);
    // xTaskCreate(vResetTask, "ResetTask", configMINIMAL_STACK_SIZE + 128, NULL, 1, NULL);

    vTaskStartScheduler();
    panic_unsupported();
}
