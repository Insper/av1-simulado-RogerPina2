#include <asf.h>

#include "gfx_mono_ug_2832hsweg04.h"
#include "gfx_mono_text.h"
#include "sysfont.h"

#define LED1_PIO			PIOA
#define LED1_PIO_ID			ID_PIOA
#define LED1_IDX			0
#define LED1_IDX_MASK		(1 << LED1_IDX)

#define LED2_PIO			PIOC
#define LED2_PIO_ID			ID_PIOC
#define LED2_IDX			30
#define LED2_IDX_MASK		(1 << LED2_IDX)

#define LED3_PIO			PIOB
#define LED3_PIO_ID			ID_PIOB
#define LED3_IDX			2
#define LED3_IDX_MASK		(1 << LED3_IDX)

#define BUT1_PIO            PIOD
#define BUT1_PIO_ID         16
#define BUT1_PIO_IDX        28
#define BUT1_PIO_IDX_MASK   (1u << BUT1_PIO_IDX)

#define BUT2_PIO			PIOC
#define BUT2_PIO_ID			ID_PIOC
#define BUT2_PIO_IDX		31
#define BUT2_PIO_IDX_MASK	(1u << BUT2_PIO_IDX)

#define BUT3_PIO			PIOA
#define BUT3_PIO_ID			ID_PIOA
#define BUT3_PIO_IDX		19
#define BUT3_PIO_IDX_MASK	(1 << BUT3_PIO_IDX)

volatile char tc1_flag;
volatile char tc4_flag;
volatile char tc7_flag;

volatile char but1_flag;
volatile char but2_flag;
volatile char but3_flag;

void io_init(void);

/**
* callback do botao1
*/
void but1_callback(void){
	but1_flag = 0;
}

/**
* callback do botao2
*/
void but2_callback(void){
	but2_flag = 0;
}

/**
* callback do botao3
*/
void but3_callback(void){
	but3_flag = 0;
}

/*
 * @Brief Pisca LED placa
 */
void pisca_led (Pio *pio, uint32_t mask) {
	if(pio_get_output_data_status(pio, mask)) {
		pio_clear(pio, mask);
	} else {
		pio_set(pio,mask);
	}
}

void TC1_Handler(void){
	volatile uint32_t ul_dummy0;

	/****************************************************************
	* Devemos indicar ao TC que a interrupção foi satisfeita.
	******************************************************************/
	ul_dummy0 = tc_get_status(TC0, 1);

	/* Avoid compiler warning */
	UNUSED(ul_dummy0);

	/** Muda o estado do LED */
	tc1_flag = 1;
}

void TC4_Handler(void){
	volatile uint32_t ul_dummy;

	/****************************************************************
	* Devemos indicar ao TC que a interrupção foi satisfeita.
	******************************************************************/
	ul_dummy = tc_get_status(TC1, 1);

	/* Avoid compiler warning */
	UNUSED(ul_dummy);

	/** Muda o estado do LED */
	tc4_flag = 1;
}

void TC7_Handler(void){
	volatile uint32_t ul_dummy;

	/****************************************************************
	* Devemos indicar ao TC que a interrupção foi satisfeita.
	******************************************************************/
	ul_dummy = tc_get_status(TC2, 1);

	/* Avoid compiler warning */
	UNUSED(ul_dummy);

	/** Muda o estado do LED */
	tc7_flag = 1;
}

/**
* Configura TimerCounter (TC) para gerar uma interrupcao no canal (ID_TC e TC_CHANNEL)
* na taxa de especificada em freq.
*/
void TC_init(Tc * TC, int ID_TC, int TC_CHANNEL, int freq){
	uint32_t ul_div;
	uint32_t ul_tcclks;
	uint32_t ul_sysclk = sysclk_get_cpu_hz();

	/* Configura o PMC */
	/* O TimerCounter é meio confuso
	o uC possui 3 TCs, cada TC possui 3 canais
	TC0 : ID_TC0, ID_TC1, ID_TC2
	TC1 : ID_TC3, ID_TC4, ID_TC5
	TC2 : ID_TC6, ID_TC7, ID_TC8
	*/
	pmc_enable_periph_clk(ID_TC);

	/** Configura o TC para operar em  4Mhz e interrupçcão no RC compare */
	tc_find_mck_divisor(freq, ul_sysclk, &ul_div, &ul_tcclks, ul_sysclk);
	tc_init(TC, TC_CHANNEL, ul_tcclks | TC_CMR_CPCTRG);
	tc_write_rc(TC, TC_CHANNEL, (ul_sysclk / ul_div) / freq);

	/* Configura e ativa interrupçcão no TC canal 0 */
	/* Interrupção no C */
	NVIC_EnableIRQ((IRQn_Type) ID_TC);
	tc_enable_interrupt(TC, TC_CHANNEL, TC_IER_CPCS);

	/* Inicializa o canal 0 do TC */
	tc_start(TC, TC_CHANNEL);
}


void io_init(void)
{
	// Configura led
	pmc_enable_periph_clk(LED1_PIO_ID);
	pio_configure(LED1_PIO, PIO_OUTPUT_0, LED1_IDX_MASK, PIO_DEFAULT);
	
	pmc_enable_periph_clk(LED2_PIO_ID);
	pio_configure(LED2_PIO, PIO_OUTPUT_1, LED2_IDX_MASK, PIO_DEFAULT);
	
	pmc_enable_periph_clk(LED3_PIO_ID);
	pio_configure(LED3_PIO, PIO_OUTPUT_0, LED3_IDX_MASK, PIO_DEFAULT);
	
	// Inicializa clock do periférico PIO responsavel pelo botao
	pmc_enable_periph_clk(BUT1_PIO_ID);
	pmc_enable_periph_clk(BUT2_PIO_ID);
	pmc_enable_periph_clk(BUT3_PIO_ID);

	// Configura PIO para lidar com o pino do botão como entrada
	// com pull-up
	pio_configure(BUT1_PIO, PIO_INPUT, BUT1_PIO_IDX_MASK, PIO_PULLUP);
	pio_configure(BUT2_PIO, PIO_INPUT, BUT2_PIO_IDX_MASK, PIO_PULLUP);
	pio_configure(BUT3_PIO, PIO_INPUT, BUT3_PIO_IDX_MASK, PIO_PULLUP);

	// Configura interrupção no pino referente ao botao e associa
	// função de callback caso uma interrupção for gerada
	// a função de callback é a: but_callback()
	pio_handler_set(BUT1_PIO,
					BUT1_PIO_ID,
					BUT1_PIO_IDX_MASK,
					//PIO_IT_FALL_EDGE, //DESCIDA
					PIO_IT_RISE_EDGE, //SUBIDA
					but1_callback);

	pio_handler_set(BUT2_PIO,
					BUT2_PIO_ID,
					BUT2_PIO_IDX_MASK,
					PIO_IT_FALL_EDGE, //DESCIDA
					//PIO_IT_RISE_EDGE, //SUBIDA
					but2_callback);
	
	pio_handler_set(BUT3_PIO,
					BUT3_PIO_ID,
					BUT3_PIO_IDX_MASK,
					//PIO_IT_FALL_EDGE, //DESCIDA
					PIO_IT_RISE_EDGE, //SUBIDA
					but3_callback);

	// Ativa interrupção
	pio_enable_interrupt(BUT1_PIO, BUT1_PIO_IDX_MASK);
	pio_enable_interrupt(BUT2_PIO, BUT2_PIO_IDX_MASK);
	pio_enable_interrupt(BUT3_PIO, BUT3_PIO_IDX_MASK);

	// Configura NVIC para receber interrupcoes do PIO do botao
	// com prioridade 4 (quanto mais próximo de 0 maior)
	NVIC_EnableIRQ(BUT1_PIO_ID);
	NVIC_SetPriority(BUT1_PIO_ID, 4); // Prioridade 4
	
	NVIC_EnableIRQ(BUT2_PIO_ID);
	NVIC_SetPriority(BUT2_PIO_ID, 4); // Prioridade 4
	
	NVIC_EnableIRQ(BUT3_PIO_ID);
	NVIC_SetPriority(BUT3_PIO_ID, 4); // Prioridade 4
}


int main (void)
{
	
	io_init();
	
	board_init();
	sysclk_init();
	delay_init();

  // Init OLED
	gfx_mono_ssd1306_init();
  
  // Escreve na tela um circulo e um texto
	gfx_mono_draw_filled_circle(20, 16, 16, GFX_PIXEL_SET, GFX_WHOLE);
  gfx_mono_draw_string("mundo", 50,16, &sysfont);
  
	/** Configura timer TC0, canal 1 com 4Hz */
		TC_init(TC0, ID_TC1, 1, 5);
		TC_init(TC1, ID_TC4, 1, 10);
		TC_init(TC2, ID_TC7, 1, 1);

  /* Insert application code here, after the board has been initialized. */
	while(1) {
		if (but1_flag) {
			if (tc1_flag) {
			pisca_led(LED1_PIO,LED1_IDX_MASK);
			tc1_flag = 0;
			}
		}
		
		if (tc4_flag) {
			pisca_led(LED2_PIO, LED2_IDX_MASK);
			tc4_flag = 0;
		}
		
		if (tc7_flag) {
			pisca_led(LED3_PIO, LED3_IDX_MASK);
			tc7_flag = 0;
		}
				
		pmc_sleep(SAM_PM_SMODE_SLEEP_WFI);
	}
}
