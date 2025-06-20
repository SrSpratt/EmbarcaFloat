#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"     // Biblioteca para arquitetura Wi-Fi da Pico com CYW43  
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "lib/ssd1306.h"
#include "lib/font.h"
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include "lwip/pbuf.h"           // Lightweight IP stack - manipulação de buffers de pacotes de rede
#include "lwip/tcp.h"            // Lightweight IP stack - fornece funções e estruturas para trabalhar com o protocolo TCP
#include "lwip/netif.h"          // Lightweight IP stack - fornece funções e estruturas para trabalhar com interfaces de rede (netif)
// Credenciais WIFI - Tome cuidado se publicar no github!
#define WIFI_SSID "TEMPLATE"
#define WIFI_PASSWORD "TEMPLATE"

#define LED_PIN CYW43_WL_GPIO_LED_PIN 
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C
#define ADC_PIN 28  // GPIO para a leitura
#define CONTROL_PIN 9 // GPIO para o controle da bomba

//Trecho para modo BOOTSEL com botão B
#include "pico/bootrom.h"
#define botaoB 6
void gpio_irq_handler(uint gpio, uint32_t events)
{
  reset_usb_boot(0, 0);
}
//

volatile float adc_reading = 0; //variável que armazena a leitura do potenciômetro
volatile int pump_state = 0; // variável que armazena o estado do pino de controle
ssd1306_t ssd; // Inicia a estrutura do display

// WIFI

// Função de callback ao aceitar conexões TCP
static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err);

// Função de callback para processar requisições HTTP
static err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);

// Tratamento do request do usuário
void user_request(char **request);
// FIM WIFI


// TASKS
void vADCReadTask();
void vDisplayTask();
void vConnectTask();
//FIM TASKS

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

  //Testes
  gpio_init(11);
  gpio_set_dir(11, GPIO_OUT);
  gpio_init(12);
  gpio_set_dir(12, GPIO_OUT);
  gpio_init(13);
  gpio_set_dir(13, GPIO_OUT);
  gpio_init(CONTROL_PIN);
  gpio_set_dir(CONTROL_PIN, GPIO_OUT);
  //gpio_pull_up(CONTROL_PIN);

  xTaskCreate(vADCReadTask, "ADC Read Task", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);
  xTaskCreate(vDisplayTask, "Display Task", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);
  xTaskCreate(vConnectTask, "Connect Task", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);
  vTaskStartScheduler();

  panic_unsupported();
}


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
    if (adc_reading < 2){
      gpio_put(CONTROL_PIN, 1);
      printf("MENOR DO QUE 2: %d\n", gpio_get(CONTROL_PIN));
    } else {
      gpio_put(CONTROL_PIN, 0);
      printf("MAIOR DO QUE 2: %d\n", gpio_get(CONTROL_PIN));
    }
    pump_state = gpio_get(CONTROL_PIN);
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

void vConnectTask(){
  while (cyw43_arch_init())
  {
    printf("Falha ao inicializar Wi-Fi\n");
    gpio_put(13, true);
    sleep_ms(100);
    gpio_put(13, false);
  }

  // GPIO do CI CYW43 em nível baixo
  cyw43_arch_gpio_put(LED_PIN, 1);

  // Ativa o Wi-Fi no modo Station, de modo a que possam ser feitas ligações a outros pontos de acesso Wi-Fi.
  cyw43_arch_enable_sta_mode();

  // Conectar à rede WiFI - fazer um loop até que esteja conectado
  printf("Conectando ao Wi-Fi...\n");

  while (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 20000))
  {
    printf("Falha ao conectar ao Wi-Fi\n");
    sleep_ms(100);
  }

  printf("Conectado ao Wi-Fi\n");

  // Caso seja a interface de rede padrão - imprimir o IP do dispositivo.
  if (netif_default)
  {
    printf("IP do dispositivo: %s\n", ipaddr_ntoa(&netif_default->ip_addr));
  }

    // Configura o servidor TCP - cria novos PCBs TCP. É o primeiro passo para estabelecer uma conexão TCP.
  struct tcp_pcb *server = tcp_new();
  if (!server)
  {
    printf("Falha ao criar servidor TCP\n");
  }

  //vincula um PCB (Protocol Control Block) TCP a um endereço IP e porta específicos.
  if (tcp_bind(server, IP_ADDR_ANY, 80) != ERR_OK)
  {
    printf("Falha ao associar servidor TCP à porta 80\n");
  }

  // Coloca um PCB (Protocol Control Block) TCP em modo de escuta, permitindo que ele aceite conexões de entrada.
  server = tcp_listen(server);

  // Define uma função de callback para aceitar conexões TCP de entrada. É um passo importante na configuração de servidores TCP.
  tcp_accept(server, tcp_server_accept);
  printf("Servidor ouvindo na porta 80\n");

  while (true)
  {
    cyw43_arch_poll(); // Necessário para manter o Wi-Fi ativo
    vTaskDelay(pdMS_TO_TICKS(100));      // Reduz o uso da CPU
  }

  //Desligar a arquitetura CYW43.
  cyw43_arch_deinit();
}


// WIFI

// Tratamento do request do usuário - digite aqui
void user_request(char **request){

    //buffer auxiliar para não acabar corrompendo a requisição
    char aux[strlen(*request)];
    strcpy(aux, *request);

    //acende a luminária na maior intensidade
    if (strstr(aux, "GET /led_h") != NULL)
    {
        gpio_put(11, 1);
    } 

};

// Função de callback ao aceitar conexões TCP
static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    tcp_recv(newpcb, tcp_server_recv);
    return ERR_OK;
}

// Função de callback para processar requisições HTTP
static err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    if (!p)
    {
        tcp_close(tpcb);
        tcp_recv(tpcb, NULL);
        return ERR_OK;
    }

    // Alocação do request na memória dinámica
    char *request = (char *)malloc(p->len + 1);
    memcpy(request, p->payload, p->len);
    request[p->len] = '\0';

    printf("Request: %s\n", request);

    // Tratamento de request - Controle da página e painel
    
    if (strstr(request, "GET /level") != NULL) { // Responde a requisição de tensão medida
        char leitura[32];
        snprintf(leitura, sizeof(leitura), "%.2f", adc_reading);

        char resposta[128];
        snprintf(resposta, sizeof(resposta),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: %d\r\n"
            "Connection: close\r\n"
            "\r\n"
            "%s",
            strlen(leitura), leitura);

        tcp_write(tpcb, resposta, strlen(resposta), TCP_WRITE_FLAG_COPY);
        tcp_output(tpcb);
    } else if (strstr(request, "GET /state")) { // Retorna a requisição do pino de controle
        char leitura[32];
        if (pump_state == 1)
          snprintf(leitura, sizeof(leitura), "ligada!");
        else
          snprintf(leitura, sizeof(leitura), "desligada!");

        char resposta[128];
        snprintf(resposta, sizeof(resposta),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: %d\r\n"
            "Connection: close\r\n"
            "\r\n"
            "%s",
            strlen(leitura), leitura);

        tcp_write(tpcb, resposta, strlen(resposta), TCP_WRITE_FLAG_COPY);
        tcp_output(tpcb);
    } else { //retorna a página
        user_request(&request);

        // Cria a resposta HTML
        char html[3072];


        char state[11];
        if (pump_state == 1)
          snprintf(state, sizeof(state), "ligada!");
        else
          snprintf(state, sizeof(state), "desligada!");

        // Instruções html do webserver
        snprintf(html, sizeof(html), // Formatar uma string e armazená-la em um buffer de caracteres
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html\r\n"
                "Connection: close\r\n"
                "\r\n"
                "<!DOCTYPE html>"
                "<html>"
                    "<head>"
                        "<title>🏠Painel</title>"
                        "<meta charset=\"UTF-8\">"
                        "<style>"
                            "body{"
                                "background:#f8f9fa;"
                                "font-family:Arial;"
                                "margin:0;"
                                "min-height:100vh;"
                                "display:flex;"
                                "flex-direction:column;"
                                "align-items:center;}"
                            ".container{"
                                "max-width:800px;"
                                "margin:0 auto;"
                                "padding:20px;"
                                "display:flex;"
                                "flex-direction: row;}"
                            ".section{"
                                "display:flex;"
                                "flex-direction:column;"
                                "background:#f6f6f6;"
                                "border-radius:10px;"
                                "padding: 10px;"
                                "box-shadow:0 4px 6px rgba(0,0,0,0.1);"
                                "}"
                            ".card{background:#fff;"
                                "border-radius:10px;"
                                "box-shadow:0 4px 6px rgba(0,0,0,0.1);"
                                "padding:20px;"
                                "margin-bottom:20px;}"
                            ".content{display:flex;"
                                "flex-direction:row;"
                                "flex-wrap:wrap;}"
                            ".btn{"
                                "display:inline-flex;"
                                "align-items:center;"
                                "justify-content:center;"
                                "background:#6c757d;"
                                "color:white;"
                                "border:none;"
                                "border-radius:5px;"
                                "padding:12px 24px;"
                                "font-size:18px;"
                                "margin:8px;"
                                "cursor:pointer;"
                                "transition:all 0.3s;}"
                            ".btn:hover{"
                                "opacity:0.8;"
                                "transform:translateY(-2px);}"
                            ".btn-p{"
                                "background:#0d6efd;}"
                            ".btn-d{"
                                "background:#dc3545;}"
                            ".btn-s{"
                                "background:#198754;}"
                            ".btn-w{"
                                "background:#ffc107;color:#000;}"
                            ".form-group{"
                                "margin-bottom:1rem;"
                                "display:flex;"
                                "align-items:center;}"
                            "select{"
                                "padding:8px;"
                                "border-radius:4px;"
                                "border:1px solid #ced4da;"
                                "margin-left:10px;}"
                            "h1{"
                                "color:#212529;"
                                "margin-bottom:1.5rem;}"
                            ".sensor{"
                                "font-size:12px;"
                                "color:#495057;"
                                "margin-top:1rem;"
                                "flex:1 1 50%%}"
                            ".text{"
                                "display:flex;"
                                "flex:1 1 100%%;}"
                        "</style>"
                        "<script>"
                          "setInterval(() => {"
                              "fetch('/level')"
                                  ".then(res=> res.text())"
                                  ".then(data=>{"
                                      "document.getElementById(\"level\").innerText=data;"
                                  "});"
                          "},1000);" // atualiza a cada 1 segundo
                        "</script>"
                        "<script>"
                          "setInterval(() => {"
                              "fetch('/state')"
                                  ".then(res=> res.text())"
                                  ".then(data=>{"
                                      "document.getElementById(\"state\").innerText=data;"
                                  "});"
                          "},1000);" // atualiza a cada 1 segundo
                        "</script>"
                    "</head>"
                    "<body>"
                        "<h1>🏠 Painel</h1>"
                        "<div class=\"container\">"
                            "<div class=\"office section\">"
                                "<h4>Projeto</h4>"
                                "<div class=\"card\">"
                                    "<h4>💧Nível de água</h4>"
                                    "<div class=\"content\">"
                                      "<h4>Nível medido: <span id=\"level\">%.2f</span> V</h4>"
                                    "</div>"
                                "</div>"
                                "<div class=\"card\">"
                                    "<h4>🚿Estado da bomba</h4>"
                                    "<div class=\"content\">"
                                      "<span id=\"state\">%s</span>"
                                    "</div>"
                                "</div>"
                            "</div>"
                        "</div>"
                    "</body>"
                "</html>", adc_reading, state);

        tcp_write(tpcb, html, strlen(html), TCP_WRITE_FLAG_COPY);

        // Envia a mensagem
        tcp_output(tpcb);
    }

    //libera memória alocada dinamicamente
    free(request);
    
    //libera um buffer de pacote (pbuf) que foi alocado anteriormente
    pbuf_free(p);

    return ERR_OK;
}