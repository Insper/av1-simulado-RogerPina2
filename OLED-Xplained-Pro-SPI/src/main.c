#include <asf.h>

#include "gfx_mono_ug_2832hsweg04.h"
#include "gfx_mono_text.h"
#include "sysfont.h"

/**
 *  Informacoes para o RTC
 *  poderia ser extraida do __DATE__ e __TIME__
 *  ou ser atualizado pelo PC.
 */
typedef struct  {
  uint32_t year;
  uint32_t month;
  uint32_t day;
  uint32_t week;
  uint32_t hour;
  uint32_t minute;
  uint32_t seccond;
} calendar;

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
volatile char tc7_flag_display;

volatile char but1_flag = 0;
volatile char but2_flag = 0;
volatile char but3_flag = 0;

volatile Bool f_rtt_alarme = true;
volatile Bool f_rtt_sem = true;

volatile Bool flag_rtc = false;

void io_init(void);

/**
* callback do botao1
*/
void but1_callback(void){
	if (but1_flag == 1) {
		but1_flag = 0;
	} else {
		but1_flag = 1;
	}
}

/**
* callback do botao2
*/
void but2_callback(void){
	if (but2_flag == 1) {
		but2_flag = 0;
	} else {
		but2_flag = 1;
	}
}

/**
* callback do botao3
*/
void but3_callback(void){
	if (but3_flag == 1) {
		but3_flag = 0;
	} else {
		but3_flag = 1;
	}
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
	* Devemos indicar ao TC que a interrup��o foi satisfeita.
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
	* Devemos indicar ao TC que a interrup��o foi satisfeita.
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
	* Devemos indicar ao TC que a interrup��o foi satisfeita.
	******************************************************************/
	ul_dummy = tc_get_status(TC2, 1);

	/* Avoid compiler warning */
	UNUSED(ul_dummy);

	/** Muda o estado do LED */
	tc7_flag = 1;
	tc7_flag_display = 1;
}

void RTT_Handler(void)
{
	uint32_t ul_status;

	/* Get RTT status - ACK */
	ul_status = rtt_get_status(RTT);
	
	/* IRQ due to Time has changed */
	if ((ul_status & RTT_SR_RTTINC) == RTT_SR_RTTINC) {
		f_rtt_alarme = false;
		//pin_toggle(LED_PIO, LED_IDX_MASK);    // BLINK Led
	}
	
	
	/* IRQ due to Alarm */
	if ((ul_status & RTT_SR_ALMS) == RTT_SR_ALMS) {
		// pin_toggle(LED_PIO, LED_IDX_MASK);    // BLINK Led
		f_rtt_alarme = true;                  // flag RTT alarme
		if (f_rtt_sem) {
			f_rtt_sem = false;
		} else {
			f_rtt_sem = true;
		}
	}
}

/**
* \brief Interrupt handler for the RTC. Refresh the display.
*/
void RTC_Handler(void)
{
	uint32_t ul_status = rtc_get_status(RTC);

	/*
	*  Verifica por qual motivo entrou
	*  na interrupcao, se foi por segundo
	*  ou Alarm
	*/
	if ((ul_status & RTC_SR_SEC) == RTC_SR_SEC) {
		rtc_clear_status(RTC, RTC_SCCR_SECCLR);
	}
	
	/* Time or date alarm */
	if ((ul_status & RTC_SR_ALARM) == RTC_SR_ALARM) {
		rtc_clear_status(RTC, RTC_SCCR_ALRCLR);
		flag_rtc = 1;
	}
	
	rtc_clear_status(RTC, RTC_SCCR_ACKCLR);
	rtc_clear_status(RTC, RTC_SCCR_TIMCLR);
	rtc_clear_status(RTC, RTC_SCCR_CALCLR);
	rtc_clear_status(RTC, RTC_SCCR_TDERRCLR);
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
	/* O TimerCounter � meio confuso
	o uC possui 3 TCs, cada TC possui 3 canais
	TC0 : ID_TC0, ID_TC1, ID_TC2
	TC1 : ID_TC3, ID_TC4, ID_TC5
	TC2 : ID_TC6, ID_TC7, ID_TC8
	*/
	pmc_enable_periph_clk(ID_TC);

	/** Configura o TC para operar em  4Mhz e interrup�c�o no RC compare */
	tc_find_mck_divisor(freq, ul_sysclk, &ul_div, &ul_tcclks, ul_sysclk);
	tc_init(TC, TC_CHANNEL, ul_tcclks | TC_CMR_CPCTRG);
	tc_write_rc(TC, TC_CHANNEL, (ul_sysclk / ul_div) / freq);

	/* Configura e ativa interrup�c�o no TC canal 0 */
	/* Interrup��o no C */
	NVIC_EnableIRQ((IRQn_Type) ID_TC);
	tc_enable_interrupt(TC, TC_CHANNEL, TC_IER_CPCS);

	/* Inicializa o canal 0 do TC */
	tc_start(TC, TC_CHANNEL);
}

static void RTT_init(uint16_t pllPreScale, uint32_t IrqNPulses)
{
  uint32_t ul_previous_time;

  /* Configure RTT for a 1 second tick interrupt */
  rtt_sel_source(RTT, false);
  rtt_init(RTT, pllPreScale);
  
  ul_previous_time = rtt_read_timer_value(RTT);
  while (ul_previous_time == rtt_read_timer_value(RTT));
  
  rtt_write_alarm_time(RTT, IrqNPulses+ul_previous_time);

  /* Enable RTT interrupt */
  NVIC_DisableIRQ(RTT_IRQn);
  NVIC_ClearPendingIRQ(RTT_IRQn);
  NVIC_SetPriority(RTT_IRQn, 0);
  NVIC_EnableIRQ(RTT_IRQn);
  rtt_enable_interrupt(RTT, RTT_MR_ALMIEN | RTT_MR_RTTINCIEN);
}

/**
* Configura o RTC para funcionar com interrupcao de alarme
*/
void RTC_init(Rtc *rtc, uint32_t id_rtc, calendar t, uint32_t irq_type){
	/* Configura o PMC */
	pmc_enable_periph_clk(ID_RTC);

	/* Default RTC configuration, 24-hour mode */
	rtc_set_hour_mode(rtc, 0);

	/* Configura data e hora manualmente */
	rtc_set_date(rtc, t.year, t.month, t.day, t.week);
	rtc_set_time(rtc, t.hour, t.minute, t.seccond);

	/* Configure RTC interrupts */
	NVIC_DisableIRQ(id_rtc);
	NVIC_ClearPendingIRQ(id_rtc);
	NVIC_SetPriority(id_rtc, 0);
	NVIC_EnableIRQ(id_rtc);

	/* Ativa interrupcao via alarme */
	rtc_enable_interrupt(rtc,  irq_type);
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
	
	// Inicializa clock do perif�rico PIO responsavel pelo botao
	pmc_enable_periph_clk(BUT1_PIO_ID);
	pmc_enable_periph_clk(BUT2_PIO_ID);
	pmc_enable_periph_clk(BUT3_PIO_ID);

	// Configura PIO para lidar com o pino do bot�o como entrada
	// com pull-up
	pio_configure(BUT1_PIO, PIO_INPUT, BUT1_PIO_IDX_MASK, PIO_PULLUP);
	pio_configure(BUT2_PIO, PIO_INPUT, BUT2_PIO_IDX_MASK, PIO_PULLUP);
	pio_configure(BUT3_PIO, PIO_INPUT, BUT3_PIO_IDX_MASK, PIO_PULLUP);

	// Configura interrup��o no pino referente ao botao e associa
	// fun��o de callback caso uma interrup��o for gerada
	// a fun��o de callback � a: but_callback()
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

	// Ativa interrup��o
	pio_enable_interrupt(BUT1_PIO, BUT1_PIO_IDX_MASK);
	pio_enable_interrupt(BUT2_PIO, BUT2_PIO_IDX_MASK);
	pio_enable_interrupt(BUT3_PIO, BUT3_PIO_IDX_MASK);

	// Configura NVIC para receber interrupcoes do PIO do botao
	// com prioridade 4 (quanto mais pr�ximo de 0 maior)
	NVIC_EnableIRQ(BUT1_PIO_ID);
	NVIC_SetPriority(BUT1_PIO_ID, 4); // Prioridade 4
	
	NVIC_EnableIRQ(BUT2_PIO_ID);
	NVIC_SetPriority(BUT2_PIO_ID, 4); // Prioridade 4
	
	NVIC_EnableIRQ(BUT3_PIO_ID);
	NVIC_SetPriority(BUT3_PIO_ID, 4); // Prioridade 4
}

void escreverhora(Rtc *rtc, calendar t){
	uint32_t h, m, s;
	rtc_get_time(rtc, &h, &m, &s);
	
	char hora[512];
	sprintf(hora, "%2d:%2d:%2d", h, m, s);
	
	gfx_mono_draw_string(hora, 20, 4, &sysfont);
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
		//gfx_mono_draw_string("5 10 1", 2,4, &sysfont);
		
	/** Configura RTC */
	calendar rtc_initial = {2019, 4, 06, 15, 20, 03, 00};
	RTC_init(RTC, ID_RTC, rtc_initial, RTC_IER_ALREN);

	//escreverhora(RTC, rtc_initial);
		
	/** Configura timer TC0, canal 1 com 4Hz */
		TC_init(TC0, ID_TC1, 1, 5);
		TC_init(TC1, ID_TC4, 1, 10);
		TC_init(TC2, ID_TC7, 1, 1);
		
	// Inicializa RTT com IRQ no alarme.
	f_rtt_alarme = true;

	/* Insert application code here, after the board has been initialized. */
	int px = 8;
	while(1) {
		
		/*			
		if (tc7_flag) {
			escreverhora(RTC, rtc_initial);
			tc7_flag = 0;
		}
		*/
			
		if (f_rtt_sem) {
			if (but1_flag) {
				if (tc1_flag) {
					pisca_led(LED1_PIO,LED1_IDX_MASK);
					tc1_flag = 0;
				}
			}
			
			if (but2_flag) {
				if (tc4_flag) {
					pisca_led(LED2_PIO, LED2_IDX_MASK);
					tc4_flag = 0;
				}
			}
			
			if (but3_flag) {
				if (tc7_flag) {
					pisca_led(LED3_PIO, LED3_IDX_MASK);
					tc7_flag = 0;
				}
			}
			
			if (tc7_flag_display) {
				gfx_mono_draw_filled_circle(px, 28, 3, GFX_PIXEL_SET, GFX_WHOLE);
				gfx_mono_draw_filled_circle(px + 12, 28, 3, GFX_PIXEL_SET, GFX_WHOLE);
				px += 24;
				tc7_flag_display = 0;
			}
		}
		
		if (f_rtt_alarme){	
			/*
			* IRQ apos 5s -> 
			*/
			uint16_t pllPreScale = (int) (((float) 32768) / 4.0);
			uint32_t irqRTTvalue = 20;
      
			// reinicia RTT para gerar um novo IRQ
			RTT_init(pllPreScale, irqRTTvalue);       
			
			px = 8;
			gfx_mono_draw_string("            ",0,24, &sysfont);
			gfx_mono_draw_string("            ",0,28, &sysfont);
			tc7_flag_display = false;
			
			f_rtt_alarme = false;
		}
		
		pmc_sleep(SAM_PM_SMODE_SLEEP_WFI);
	}
}
