#include <stdio.h>
#include "stm32f7xx.h"
#include <stdlib.h>



void GPIO_Config(void);
void TIM3_PWM_Config(void);
void EXTI_Config(void);
void TIM7_Config(void);
void SysTick_Init(void);
void UART4_Init(void);
void Tx_UART4(char v[]); 
void Rx_UART4(void);

int numero_rx = 0;

long Pul_A = 0;
double RPM = 0;
int temp = 0;
float pwm = 0;
char envio = 0x31;
char rData=0, msg[20];
int count_char;
char buffer[10];
char dir;

extern "C"{
	
	/* Interrupción cada 10 ms */
	void SysTick_Handler(void){
				
		if(RPM == 0){
			dir = ' ';
		}
		
		/* Envio por UART : sentido de giro y RPM */
		sprintf(buffer,"%c %d\n",dir,(int)RPM); 
		Tx_UART4(buffer);
	}
}

extern "C"{
	
	void EXTI2_IRQHandler(void){
		EXTI->PR |= (1UL<<2); // Limpiar bandera de interrupción
		Pul_A++; // Contador de pulsos del encoder por interrupción
	}
}

extern "C"{
	void EXTI3_IRQHandler(void){
		
		EXTI->PR |= (1UL<<3); // Limpiar bandera de interrupción
		
		/* Determinar sentido de giro con interrupciones del Encoder */
		if( ((GPIOE->IDR &= 0x4)==0x4) == ((GPIOE->IDR &= 0x8)==0x8) ){
			dir = '+';
		}else{
			dir = '-';
		}
		
	}
}

extern "C"{

	void TIM7_IRQHandler(void){
		
		TIM7->SR &=~ TIM_SR_UIF; // Limpiar bandera de Interrupción
		
		temp++;
		
		if(temp > 999){  // 1000 ms -> 1 seg.
		temp = 0;
		GPIOB->ODR ^= (1UL<<7); // LED_Testigo
		/* Capruta de pulsos cada 1 segundo */
		RPM = Pul_A * 60 / 442; // 442 pulsos por vuelta en el eje
		Pul_A = 0;
			
		}
		
   }
}

extern "C"{

		void UART4_IRQHandler(void){

			if(UART4->ISR & 0x20){ // Los datos recibidos están listos para ser leídos.
				Rx_UART4();
			}

		}	// end UART4
}

int main(void){

	GPIO_Config();
	EXTI_Config();
	TIM3_PWM_Config();
	TIM7_Config();
	SysTick_Init(); // 10ms
	UART4_Init(); 
			
	while(true){
	
		/* 
		PWM -> TIM3_CH1 PB4
		PWM con T = 20ms, numero_rx : [3 a 99] porcentaje
		del ancho de pulso proveniente de UART (NetBeans) */
		pwm = 200*numero_rx; 
		TIM3->CCR1 = pwm; // Caargar el nuevo valor del PWM
		
	}
}

void GPIO_Config(void){

	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOEEN;
	
	/* PWM TIM3_CHI*/
	GPIOA->MODER &=~ (3UL<<2*6);
	GPIOA->MODER |= (2UL<<2*6);
	GPIOA->OTYPER =0;
	GPIOA->OSPEEDR &=~ (3UL<<2*6);
	GPIOA->OSPEEDR |= (1UL<<2*6);
	GPIOA->PUPDR &=~ (3UL<<2*6);
	GPIOA->PUPDR |= (1UL<<2*6);
	GPIOA->AFR[0] |= 0x2000000;
	
	/* OUT A ENCODER */
	GPIOE->MODER &=~ (3UL<<2*2);
	GPIOE->OTYPER = 0;
	GPIOE->OSPEEDR |= (2UL<<2*2);
	GPIOE->PUPDR |= (2UL<<2*2);
	
	/* OUT B ENCODER */
	GPIOE->MODER &=~ (3UL<<2*3);
	GPIOE->OTYPER = 0;
	GPIOE->OSPEEDR |= (2UL<<2*3);
	GPIOE->PUPDR |= (2UL<<2*3);
	
	GPIOB->MODER |= (1UL<<2*7);
	GPIOB->OTYPER = 0;
	GPIOB->OSPEEDR |= (3UL<<2*7);
	GPIOB->PUPDR |= (1UL<<2*7);
				
}

void EXTI_Config(void){

	RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
	
	//EXTI->IMR |= (1UL<<0);
	EXTI->IMR |= EXTI_IMR_IM2;
	EXTI->RTSR |= (1UL<<2);
	EXTI->PR |= EXTI_PR_PR2;

	
	EXTI->IMR |= EXTI_IMR_IM3;
	EXTI->RTSR |= (1UL<<3);

	
	SYSCFG->EXTICR[0] |= (SYSCFG_EXTICR1_EXTI2_PE | SYSCFG_EXTICR1_EXTI3_PE);
	
	NVIC_EnableIRQ(EXTI2_IRQn);
	NVIC_EnableIRQ(EXTI3_IRQn);

}

void TIM3_PWM_Config(void){
	
	RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
	
	TIM3->EGR |= (1UL<<0); 
	TIM3->PSC = 15; // 1 Mhz
	TIM3->ARR = 20000; // 20 ms
	TIM3->DIER |= (1UL<<0); // Habilitar Interrupción
	
	TIM3->CR1 |= (1UL<<0); // Habilitar Contador
	
	/* PWM TIM3_CH_1 */
	TIM3->CCER |= (1UL<<0);
	TIM3->CCMR1 |= 0x60; // PWM modo 1
	NVIC_EnableIRQ(TIM3_IRQn);
}

void TIM7_Config(void){
	
  RCC->APB1ENR |= RCC_APB1ENR_TIM7EN;
	
  TIM7->CR1 &=~ TIM_CR1_CEN;
  TIM7->PSC = 15;
  TIM7->ARR = 1000; // 1ms
  NVIC_EnableIRQ(TIM7_IRQn);
  TIM7->DIER |= TIM_DIER_UIE;
  TIM7->CR1 |= TIM_CR1_CEN;
	
}

void SysTick_Init(void){
	SystemCoreClockUpdate();
	SysTick_Config(SystemCoreClock/10);  //SysTick configurado a 10ms, 100Hz
}

void UART4_Init(void){

	RCC->APB1ENR |= (1UL<<19); // Habilitar el Reloj del UART4

	//UART4
	//UART4->BRR  = 0x683; // Velocidad : 9600 Baudios
	UART4->BRR  = 0x8B; //115200
	
  UART4->CR1 |= 0x2D; // UART4 Habilitado, RE y TE Transmisión y recepción Habilitada, RXNEIE Interrupción por Recepción Habilitada
  GPIOC->MODER   |= (2UL<<2*11); // Rx PTC11
	GPIOC->AFR[1]  |= 0x8000;
	GPIOC->MODER   |= (2UL<<2*10); // Tx PTC10
	GPIOC->AFR[1]  |= 0x800;
	NVIC_EnableIRQ(UART4_IRQn); // Venctor Interrupciones para UART4
	
}
void Tx_UART4(char v[]){

	short i=0;
	for (i=0; envio !=0; i++){
		
	    envio = v[i];
	    UART4->TDR = envio;
			while ((UART4->ISR &= 0x80)==0);   //esperar hasta que TXE sea 1, lo que indica que se envio el dato
		
	}
	envio='1';
}

void Rx_UART4(void){
			
	rData = UART4->RDR;

	if(rData == '*' | rData == 10){ // 10 = LF = Nueva Línea, 13 = CR = Retorno de Carro, *

		msg[count_char] = '\0'; // Finalizar cadena de caracteres con 0
		count_char=0;
		rData=0;
		numero_rx=0;
		numero_rx = atoi(msg); // convierte la cadena de caracteres a int

		for(int x=0;x<20;x++){msg[x]=0;}

	}	else {
		
		msg[count_char] = rData; // Formacion de la cadena de caracteres
		count_char++;
		}
}
