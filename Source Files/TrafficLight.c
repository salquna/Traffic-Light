#include "TExaS.h"
#include "inc\tm4c123gh6pm.h"

// Label input/output ports
#define WalkLIGHT			    (*((volatile uint32_t *)0x40025028))	// PF3 and PF1
#define LIGHT                   (*((volatile uint32_t *)0x400050FC))   // PB0-5
#define SENSOR                  (*((volatile uint32_t *)0x4002401C))	// PE0, PE1, PE2,

void DisableInterrupts(void); // Disable interrupts
void EnableInterrupts(void);  // Enable interrupts
void Pll_Init(void);
void Init_GPIO_PortsEBF(void);
void SysTick_Init(void);
void SysTick_Wait10ms(unsigned long);

struct State 
{
  uint32_t Out;      // 6-bit output
  uint32_t Time;     // 10 ms
  uint8_t Next[10];
}; // depends on 3-bit input
typedef const struct State STyp;
#define goN      0
#define waitN    1
#define goE      2
#define waitE    3
#define WalkON1  4
#define WalkOFF1 5
#define WalkON2  6
#define WalkOFF2 7
#define WalkON3	 8
#define WalkOFF3 9


// Define FSM as global	
STyp FSM[10]=
{
	          //000  001  010 011  100    101   110   111                                                    R     G
 {0xA1,300,{goN,waitN,goN,waitN,waitN,waitN,waitN,waitN}},																		  // goN  10 100001
                                                                                                //            R   y
 {0xA2, 50,{goE,goE,goE,goE,WalkON1,WalkON1,WalkON1,WalkON1}},																	// waitN  10 100010
                                                                                                //            GR 
 {0x8C,300,{goE,goE,waitE,waitE,waitE,waitE,waitE,waitE}},																	    // goE 10   001100
                                                                                                //           y R
 {0x94, 50,{goN,goN,goN,goN,WalkON1,WalkON1,WalkON1,WalkON1}},																	// waitE 10 010100
 
 {0x224,150,{WalkOFF1,WalkOFF1,WalkOFF1,WalkOFF1,WalkOFF1,WalkOFF1,WalkOFF1,WalkOFF1}},	            // walkON1  001000 100100
 {0x024,10,{WalkON2,WalkON2,WalkON2,WalkON2,WalkON2,WalkON2,WalkON2,WalkON2}},						// walkOFF1  00 100100
 {0xA4,10,{WalkOFF2,WalkOFF2,WalkOFF2,WalkOFF2,WalkOFF2,WalkOFF2,WalkOFF2,WalkOFF2}},				// walkON2  10 100100
 {0x24, 10,{WalkON3,WalkON3,WalkON3,WalkON3,WalkON3,WalkON3,WalkON3,WalkON3}},						// walkOFF2 on  00 100100
 {0xA4,10,{WalkOFF3,WalkOFF3,WalkOFF3,WalkOFF3,WalkOFF3,WalkOFF3,WalkOFF3,WalkOFF3}},		        // walkON3 10 100100
 {0x24, 10,{goN,goE,goN,goN,goE,goE,goN,goN}}                                                       // walkOFF off 00 100100
};
																							

int n; // state number
uint32_t Input;

int main(void){
	// activate grader and set system clock to 80 MHz
   TExaS_Init(SW_PIN_PE210, LED_PIN_PB543210,ScopeOff);
  
	// Call various initialization functions
  Pll_Init();               // initialize 80 MHz system clock
  SysTick_Init();                   // initialize SysTick timer
  Init_GPIO_PortsEBF();
	// Establish initial state of traffic lights
  n = goN;                          // initial state: Green north; Red east; Walk red
  
	while(1){
    LIGHT = FSM[n].Out&0x03F;       		// set street lights to current state's 5 bit Out value
		WalkLIGHT = (FSM[n].Out&0x3C0)>>6;	// set walk light to current state's 4 bit Out value
    SysTick_Wait10ms(FSM[n].Time);  		// wait 10 ms * current state's Time value
		
		
		
    Input = SENSOR;                 		// get new input from car detectors
    n = FSM[n].Next[Input];         		// transition to next state
  }
}
void Init_GPIO_PortsEBF(void) {
	volatile unsigned long delay;
  // allow time for clock to stabilize
  SYSCTL_RCGC2_R |= 0x32; // 1) enable clock for ports F, E, and B
	delay = SYSCTL_RCGC2_R; // and wait for a while 
  GPIO_PORTB_DIR_R |= 0x3F;         // make PB5-0 out
  GPIO_PORTB_AFSEL_R &= ~0x3F;      // disable alt funct on PB5-0
  GPIO_PORTB_DEN_R |= 0x3F;         // enable digital I/O on PB5-0
                                    // configure PB5-0 as GPIO
  GPIO_PORTB_PCTL_R = (GPIO_PORTB_PCTL_R&0xFF000000)+0x00000000;
  GPIO_PORTB_AMSEL_R &= ~0x3F;      // disable analog functionality on PB5-0
  GPIO_PORTE_DIR_R &= ~0x07;        // make PE2-0 in
  GPIO_PORTE_AFSEL_R &= ~0x07;      // disable alt funct on PE2-0
  GPIO_PORTE_DEN_R |= 0x07;         // enable digital I/O on PE2-0
                                    // configure PE2-0 as GPIO
  GPIO_PORTE_PCTL_R = (GPIO_PORTE_PCTL_R&0xFFFFFF00)+0x00000000;
  GPIO_PORTE_AMSEL_R &= ~0x07;      // disable analog functionality on PE2-0
  GPIO_PORTF_DIR_R |= 0x0A;					// make PF1 and PF3
	GPIO_PORTF_AFSEL_R &= ~0x0A;			// disable alt function for PF1 and PF3
	GPIO_PORTF_DEN_R |= 0x0A;					// enable digital I/O in PF1 and PF3
																		// configure PF1 and PF3 as GPIO
	GPIO_PORTF_PCTL_R = (GPIO_PORTF_PCTL_R&0xFFFFFF00)+0x00000000;
	GPIO_PORTF_AMSEL_R |= ~0x0A;			// disable analog functionality on PF1 and PF3
	}
	void Pll_Init(void){
 // 0) Use RCC2
 SYSCTL_RCC2_R |= 0x80000000; // USERCC2
 // 1) bypass PLL while initializing
 SYSCTL_RCC2_R |= 0x00000800; // BYPASS2, PLL bypass
 // 2) select the crystal value and oscillator source
 SYSCTL_RCC_R = (SYSCTL_RCC_R &~0x000007C0) // clear XTAL field, bits 10-6
 + 0x00000540; // 10101, configure for 16 MHz crystal
 SYSCTL_RCC2_R &= ~0x00000070; // configure for main oscillator source
 // 3) activate PLL by clearing PWRDN
 SYSCTL_RCC2_R &= ~0x00002000;
 // 4) set the desired system divider
 SYSCTL_RCC2_R |= 0x40000000; // use 400 MHz PLL
 SYSCTL_RCC2_R = (SYSCTL_RCC2_R&~ 0x1FC00000) // clear system clock divider
 + (4<<22); // configure for 80 MHz clock
 // 5) wait for the PLL to lock by polling PLLLRIS
 while((SYSCTL_RIS_R&0x00000040)==0){}; // wait for PLLRIS bit
 // 6) enable use of PLL by clearing BYPASS
 SYSCTL_RCC2_R &= ~0x00000800;
}



void SysTick_Init(void){
	NVIC_ST_CTRL_R = 0;              // 1) disable SysTick during setup
  NVIC_ST_RELOAD_R = 0x00FFFFFF;   // 2) maximum reload value
  NVIC_ST_CURRENT_R = 0;           // 3) any write to current clears it
  NVIC_ST_CTRL_R = 0x00000005;     // 4) enable SysTick with core clock

}
// The delay parameter is in units of the 80 MHz core clock. (12.5 ns)
void SysTick_Wait(unsigned long delay){
 NVIC_ST_RELOAD_R = delay-1; // number of counts to wait
 NVIC_ST_CURRENT_R = 0; // any value written to CURRENT clears
                                                                           while((NVIC_ST_CTRL_R&0x00010000)==0){ // wait for count flag
 }
}
// 800000*12.5ns equals 10ms
void SysTick_Wait10ms(unsigned long delay){
 unsigned long i;
 for( i=0; i<delay; i++){
 SysTick_Wait(800000); // wait 10ms
 }
}
