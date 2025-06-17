#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "lib/ssd1306.h"
#include "lib/font.h"
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"

#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C
#define ADC_PIN 28  // GPIO para a leitura

//Trecho para modo BOOTSEL com botão B
#include "pico/bootrom.h"
#define botaoB 6
void gpio_irq_handler(uint gpio, uint32_t events)
{
  reset_usb_boot(0, 0);
}

volatile float adc_reading = 0;
ssd1306_t ssd; // Inicia a estrutura do display

void vADCReadTask(){

  float tensao;

  adc_select_input(2); // Seleciona o ADC para eixo X. O pino 28 como entrada analógica

  while(true){
    float soma = 0.0f;
    for (int i = 0; i < 16; i++) {
      soma += adc_read();
      sleep_ms(1);
    }
    float media = soma / 16.0f;
    adc_reading = (media * 3.3) / 4095;
    printf("Tensão: %.2f\n", adc_reading);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void vDisplayTask(){

  i2c_init(I2C_PORT, 400 * 1000);
  gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
  gpio_set_function(I2C_SCL, GPIO_FUNC_I2C); 
  gpio_pull_up(I2C_SDA); 
  gpio_pull_up(I2C_SCL); 
  
  ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT); // Inicializa o display
  ssd1306_config(&ssd); // Configura o display
  ssd1306_send_data(&ssd); // Envia os dados para o display
  ssd1306_fill(&ssd, false);
  ssd1306_send_data(&ssd);
  char str[5]; // Buffer para armazenar a string
  
  bool cor = true;

  while(true){
    sprintf(str, "%1.2f", adc_reading);  // Converte o float em string
      
    // Atualiza o conteúdo do display com animações
    ssd1306_fill(&ssd, !cor); // Limpa o display
    ssd1306_rect(&ssd, 3, 3, 122, 60, cor, !cor); // Desenha um retângulo
    ssd1306_line(&ssd, 3, 25, 123, 25, cor); // Desenha uma linha
    ssd1306_line(&ssd, 3, 37, 123, 37, cor); // Desenha uma linha   
    ssd1306_draw_string(&ssd, "CEPEDI   TIC37", 8, 6); // Desenha uma string
    ssd1306_draw_string(&ssd, "EMBARCATECH", 20, 16); // Desenha uma string
    ssd1306_draw_string(&ssd, "Projeto", 30, 28); // Desenha uma string
    ssd1306_draw_string(&ssd, "Tensao(V)", 30, 41); // Desenha uma string    
    ssd1306_draw_string(&ssd, str, 52, 52); // Desenha uma string   
    ssd1306_send_data(&ssd); // Atualiza o display
    vTaskDelay(pdMS_TO_TICKS(70));
  }
}

int main()
{
  // Para ser utilizado o modo BOOTSEL com botão B
  gpio_init(botaoB);
  gpio_set_dir(botaoB, GPIO_IN);
  gpio_pull_up(botaoB);
  gpio_set_irq_enabled_with_callback(botaoB, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
  stdio_init_all();
  //Aqui termina o trecho para modo BOOTSEL com botão B

  adc_init();
  adc_gpio_init(ADC_PIN);// GPIO 28 como entrada analógica
  gpio_init(11);
  gpio_set_dir(11, GPIO_OUT);
  gpio_put(11, true);

  xTaskCreate(vADCReadTask, "ADC Read Task", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);
  xTaskCreate(vDisplayTask, "Display Task", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);
  vTaskStartScheduler();

  panic_unsupported();
}