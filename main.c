/** ***************************************************************************
 * @file    main.c
 * @brief   Simple UART Demo for EFM32GG_STK3700
 * @version 1.0
******************************************************************************/

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
/*
 * Including this file, it is possible to define which processor using comando line
 * E.g. -DEFM32GG995F1024
 * The alternative is to include the processor specific file directly
 * #include "efm32gg995f1024.h"
 */
#include "em_device.h"
#include "clock_efm32gg2.h"

#include "gpio.h"
#include "led.h"
#include "uart.h"
#include "pwm.h"
#include "adc.h"
#include "lcd.h"

#define DELAYVAL 2

#define SYSTICKDIVIDER 1000
#define SOFTDIVIDER 1000
        
int state = 0;
int valor_adc0 = 0; //valor do adc0 (controla a roda direita)
int valor_adc1 = 0; //valor do adc1 (controla a roda esquerda)
int valor_adc2 = 0; //valor do adc2 (usado para verificar se o carrinho chegou no cruzamento e se virou completamente)
char adc0[50], adc1[50], adc2[50]; // strings para mostrar os valores no display
static uint32_t ticks = 0; 
int statusDistancia[4][4] = {{0, 1, 1, 1},
                            {0, 0, 1, 0}, 
                            {1, 0, 1, 1}, 
                            {0, 0, 1, 1}}; // exemplo de caminho do labirinto
int contaStatus = 0; // conta qual linha do labirinto foi utilizada para o exemplo
int checaCruzamento = 0; //começa no 0 para indicar que o sensor 2 está fora da linha preta e indica 1 caso esteja em cima da linha. É mudado no cruzamento e na hora de virar para auxiliar o entendimento do código.
int segundaVolta = 0; //serve para fazer virar para esquerda 2x, 0 significa que não girou 1x ainda, 1 significa que girou 1x e precisa virar de novo

void SysTick_Handler(void) {
    static int counter = 0;             // must be static

    ticks++;
    
    if( counter != 0 ) {
        counter--;
    } else {
        counter = SOFTDIVIDER-1;
        LED_Toggle(LED1);
    }
}

void Delay(uint32_t v) {
    uint32_t lim = ticks+v;       // Missing processing of overflow here

    while ( ticks < lim ) {}

}




void andar (){
    valor_adc0 = ADC_Read(ADC_CH0);
    valor_adc1 = ADC_Read(ADC_CH1);
    itoa(valor_adc0, adc0, 10);
    itoa(valor_adc1, adc1, 10);
    strcat(adc0, "-");
    //LCD_WriteString(strcat(adc0, adc1));

    if(valor_adc0 < 600 && valor_adc1 < 600){
        PWM_Write(TIMER1, 1, 0);
        PWM_Write(TIMER0, 1, 0);
        state = 1;
        return;
    }
    else if(valor_adc0 == valor_adc1){
        PWM_Write(TIMER1, 1, 65535);
        PWM_Write(TIMER0, 1, 65535);
    }
    else if(valor_adc0 > valor_adc1){
        PWM_Write(TIMER1, 1, 16383);
        PWM_Write(TIMER0, 1, 65535);
    }
    else if(valor_adc0 < valor_adc1){
        PWM_Write(TIMER1, 1, 65535);
        PWM_Write(TIMER0, 1, 16383);
    }
    state = 0;
    return;
    
}


    //verificar ultrassom
    //Sensor de distância verificaria todos os caminhos livres para o carrinho passar e depois escolheria um lado dependendo da sua prioridade
    // a Ordem de preferência é: Direita - Frente - Esquerda - Meia volta
void verificacao(){
    LCD_WriteString("foi");
    int contador = 0;
    int caminhos[4];
    int ladosDisponiveis[4];

    for(int i=0; i<4; i++){
        ladosDisponiveis[i] = statusDistancia[contaStatus][i];
    }


    for (int i=0; i< 30 ; i++){
        char bufer[30];
        //verifica se tem uma parede na frente do sensor
        LCD_WriteString(itoa(i, bufer, 10));

        if(i==0){
            for (int j=0; j< 4 ; j++){
                if(ladosDisponiveis[j]){ // aqui seria se o sensor detectasse um caminho livre
                    caminhos[j] = 1;
                }else{
                    caminhos[j] = 0;
                }
                //gira 90º para um lado fixo
                PWM_Write(TIMER2, 0, 16383);
            }
            
        }
        
        Delay(200*DELAYVAL);
    }

    if(caminhos[1]){//supondo que o sensor gira para direita
        state = 2; // virar para direita
        LCD_WriteString("DIREITA");

        Delay(3*DELAYVAL);
    }
    else if (caminhos[0]){
        LCD_WriteString("FRENTE");

        state = 0; // continuar em frente
        Delay(3*DELAYVAL);

    }
    else if (caminhos[3]){
        LCD_WriteString("ESQUERDA");

        state = 4; // virar para esquerda     
        Delay(3*DELAYVAL);

    }
    else if (caminhos[2]){

        LCD_WriteString("COSTAS");
        state = 6; // dar meia volta
        Delay(3*DELAYVAL);

    }

    Delay(500*DELAYVAL);
    contaStatus++;

}


void ChangeState(){
    char buf[30];

    

    switch(state){
        case 0://andando em linha reta, passa para o caso 1 caso o sensor 3 detectar uma linha preta. 
            LCD_WriteString(itoa(contaStatus,buf, 10));

            andar();

            break;

        case 1://parado para analisar pra onde ir, verifica os lados para onde o carro pode virar, vira para direita (estado 2) como padrão, mas se não tiver como virar verifica se o caminho à frente está disponível, caso não esteja, verifica se o da esquerda está disponível (estado 4), e caso contrário o carro deve retornar pelo caminho que chegou(estado 6).
            valor_adc2 = ADC_Read(ADC_CH2);
            //LCD_WriteString("STATE 1");
            if(valor_adc2 > 600){
                PWM_Write(TIMER1, 1, 65535);
                PWM_Write(TIMER0, 1, 65535);
                LCD_WriteString("ESPERANDO");
            }
            else{
                PWM_Write(TIMER1, 1, 0);
                PWM_Write(TIMER0, 1, 0);
                checaCruzamento = 1;
                verificacao();
            }


            break;
        case 2://virando para direita
            LCD_WriteString("STATE 2");

            valor_adc2 = ADC_Read(ADC_CH2);
            if(checaCruzamento == 1 && valor_adc2 < 600){ // sensor na linha començando a virar
                PWM_Write(TIMER1, 1, 65535);
                PWM_Write(TIMER0, 1, 0);
            }else if (checaCruzamento == 1 && valor_adc2 > 600){ // sensor saiu da linha mas continuou girando
                PWM_Write(TIMER1, 1, 65535);
                PWM_Write(TIMER0, 1, 0);
                checaCruzamento = 0;
            }else if (checaCruzamento == 0 && valor_adc2 > 600){ // sensor fora da linha, continua girando, para se encontrar uma linha preta
                PWM_Write(TIMER1, 1, 65535);
                PWM_Write(TIMER0, 1, 0);
            }else if(checaCruzamento == 0 && valor_adc2 < 600){
                PWM_Write(TIMER1, 1, 0);
                PWM_Write(TIMER0, 1, 0);
                Delay(DELAYVAL*100);
                checaCruzamento = 1;
                state = 3;
            }

            break;

        case 3://verificação se virou completamente para a direita
            LCD_WriteString("STATE 3");

            valor_adc2 = ADC_Read(ADC_CH2);
            if(valor_adc2 < 600){
                PWM_Write(TIMER1, 1, 65535);
                PWM_Write(TIMER0, 1, 65535);
            }else{
                state = 0;
                checaCruzamento = 0;    
            }
            break;


        case 4://virando para a esquerda
            LCD_WriteString("STATE 4");

            valor_adc2 = ADC_Read(ADC_CH2);
            if(checaCruzamento == 1 && valor_adc2 < 600){ // sensor na linha començando a virar
                PWM_Write(TIMER0, 1, 65535);
                PWM_Write(TIMER1, 1, 0);
            }else if (checaCruzamento == 1 && valor_adc2 > 600){ // sensor saiu da linha mas continuou girando
                PWM_Write(TIMER0, 1, 65535);
                PWM_Write(TIMER1, 1, 0);
                checaCruzamento = 0;
            }else if (checaCruzamento == 0 && valor_adc2 > 600){ // sensor fora da linha, continua girando, para se encontrar uma linha preta
                PWM_Write(TIMER0, 1, 65535);
                PWM_Write(TIMER1, 1, 0);
            }else if (checaCruzamento == 0 && valor_adc2 < 600){
                PWM_Write(TIMER0, 1, 0);
                PWM_Write(TIMER1, 1, 0);
                Delay(DELAYVAL);
                checaCruzamento = 1;
                state = 5;
            }
            break;

        case 5://verificação se virou completamente para esquerda
            LCD_WriteString("STATE 5");

            valor_adc2 = ADC_Read(ADC_CH2);
            if(valor_adc2 < 600){
                PWM_Write(TIMER1, 1, 65535);
                PWM_Write(TIMER0, 1, 65535);
            }else{
                state = 0;
                checaCruzamento = 0;    
            }
            break;
        case 6://virando de costas (180º)
            LCD_WriteString("STATE 6");

            valor_adc2 = ADC_Read(ADC_CH2);
            if(checaCruzamento == 1 && valor_adc2 < 600 && segundaVolta == 0 ){ // sensor na linha començando a virar
                PWM_Write(TIMER0, 1, 65535);
                PWM_Write(TIMER1, 1, 0);
            }else if (checaCruzamento == 1 && valor_adc2 > 600 && segundaVolta == 0){ // sensor saiu da linha mas continuou girando
                PWM_Write(TIMER0, 1, 65535);
                PWM_Write(TIMER1, 1, 0);
                checaCruzamento = 0;
            }else if (checaCruzamento == 0 && valor_adc2 > 600 && segundaVolta == 0){ // sensor fora da linha, continua girando, para se encontrar uma linha preta
                PWM_Write(TIMER0, 1, 65535);
                PWM_Write(TIMER1, 1, 0);
            }else if(checaCruzamento == 0 && valor_adc2 < 600 && segundaVolta == 0){
                PWM_Write(TIMER0, 1, 0);
                PWM_Write(TIMER1, 1, 0);
                Delay(DELAYVAL);
                checaCruzamento = 1;
                segundaVolta = 1;
            }
            else if(checaCruzamento == 1 && valor_adc2 < 600 && segundaVolta == 1){ // sensor na linha començando a virar
                PWM_Write(TIMER0, 1, 65535);
                PWM_Write(TIMER1, 1, 0);
            }else if (checaCruzamento == 1 && valor_adc2 > 600 && segundaVolta == 1){ // sensor saiu da linha mas continuou girando
                PWM_Write(TIMER0, 1, 65535);
                PWM_Write(TIMER1, 1, 0);
                checaCruzamento = 0;
            }else if (checaCruzamento == 0 && valor_adc2 > 600 && segundaVolta == 1){ // sensor fora da linha, continua girando, para se encontrar uma linha preta
                PWM_Write(TIMER0, 1, 65535);
                PWM_Write(TIMER1, 1, 0);
            }else if(checaCruzamento == 0 && valor_adc2 < 600 && segundaVolta == 1){
                PWM_Write(TIMER0, 1, 0);
                PWM_Write(TIMER1, 1, 0);
                Delay(DELAYVAL);
                checaCruzamento = 1;
                segundaVolta = 0;
                state = 7;
            }
            break;
        case 7://verificação se virou completamente para trás
            LCD_WriteString("STATE 7");

            valor_adc2 = ADC_Read(ADC_CH2);
            if(valor_adc2 < 600){
                PWM_Write(TIMER1, 1, 65535);
                PWM_Write(TIMER0, 1, 65535);
            }else{
                state = 0;
                checaCruzamento = 0;    
            }
            break;
        default://qualquer coisa
        return;
    }

}



int main(void){

    //Configure inputs
    GPIO_Init(GPIOD, BIT(0), 0);
    GPIO_Init(GPIOD, BIT(1), 0);
    GPIO_Init(GPIOD, BIT(2), 0);
    //Configure Outputs
    GPIO_Init(GPIOD, 0, BIT(7));
    GPIO_Init(GPIOC, 0, BIT(0));

    //ClockConfiguration_t clockconf;
    
    /* Configure LEDs */
    //LED_Init(LED1|LED2);

    // Set clock source to external crystal: 48 MHz
    (void) SystemCoreClockSet(CLOCK_HFXO,1,1);
    SysTick_Config(SystemCoreClock/SYSTICKDIVIDER);


    // #if 1
    // ClockGetConfiguration(&clockconf);
    // #endif
    /* Turn on LEDs */
    //LED_Write(0,LED2);

    /* Configure UART */
    //UART_Init();

    // Configure LED PWM
    //PWM_Init(TIMER3,PWM_LOC1,PWM_PARAMS_ENABLECHANNEL2);
    //PWM_Write(TIMER3, 2, 0);

    //Configure ADC read
    // Configure ADC
    ADC_Init(500000);
    ADC_ConfigChannel(ADC_CH0, 0); //ADC_SINGLECTRL_REF_VDD
    ADC_ConfigChannel(ADC_CH1, 0); //ADC_SINGLECTRL_REF_
    ADC_ConfigChannel(ADC_CH2, 0); //ADC_SINGLECTRL_REF

    
     /* Configure LCD */
    LCD_Init();

    LCD_SetAll();
    Delay(DELAYVAL);

    LCD_ClearAll();
    Delay(DELAYVAL);

    // Configure LED PWM
    PWM_Init(TIMER0,PWM_LOC4,PWM_PARAMS_ENABLECHANNEL1);
    PWM_Init(TIMER1,PWM_LOC4,PWM_PARAMS_ENABLECHANNEL1);
    PWM_Init(TIMER2,PWM_LOC1,PWM_PARAMS_ENABLECHANNEL0);

    // ! NAO SEI Q Q ISSO: Enable IRQs
    //__enable_irq();


    char buffer[100];

    //ch = '*';
    while(1){
        ChangeState();
        
    }
    return 0;
}

