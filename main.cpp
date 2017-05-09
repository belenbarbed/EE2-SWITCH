#include "mbed.h"
#include "Adafruit_SSD1306.h"

// Switch input definition
#define SW_PIN0 p21
#define SW_PIN1 p22
#define SW_PIN2 p23
#define SW_PIN3 p24

// Sampling period for the switch oscillator (us)
#define SW_PERIOD 20000 

// Freq threshold for buttons
#define THOLD_ON 66
#define THOLD_OFF 68

// Display interface pin definitions
#define D_MOSI_PIN p5
#define D_CLK_PIN p7
#define D_DC_PIN p8
#define D_RST_PIN p9
#define D_CS_PIN p10

// Wave output pins
#define OUT_PIN p25
#define AOUT_PIN p18

// PWMOUT to LED2
PwmOut pout(OUT_PIN);

// sine wave and triangle wave outputs through DAC
AnalogOut DAC(AOUT_PIN);
volatile int sin_count = 0;
float sine_wave[128];
volatile int tri_count = 0;
float tri_wave[128];

// an SPI sub-class that sets up format and clock speed
class SPIPreInit : public SPI
{
public:
    SPIPreInit(PinName mosi, PinName miso, PinName clk) : SPI(mosi,miso,clk)
    {
        format(8,3);
        frequency(2000000);
    };
};

// Interrupt Service Routine prototypes (functions defined below)
void sedge0();
void sedge1();
void sedge2();
void sedge3();

void tout();

// Interrupt service for sine outputting
void sine_interrupt();

// Interrupt service for triangle wave outputting
void tri_interrupt();

// Print UI
void printUI(Adafruit_SSD1306_Spi& gOled, volatile uint8_t state[4]);

// some math 4 u
uint32_t power(uint32_t base, uint8_t exp);

// Output for the alive LED
DigitalOut alive(LED1);

// External interrupt input from the switch oscillator
InterruptIn swin0(SW_PIN0);
InterruptIn swin1(SW_PIN1);
InterruptIn swin2(SW_PIN2);
InterruptIn swin3(SW_PIN3);

// Switch sampling timer
Ticker swtimer;

// Sine wave timer
Ticker sine_ticker;

// triangle wave timer
Ticker tri_ticker;

// Registers for the switch counter, switch counter latch register and update flag
volatile uint16_t scounter[4] = {0};
volatile uint16_t scount[4] = {0};
volatile uint16_t sfreq[4] = {0};
volatile bool son[4] = {0};
volatile uint8_t sstate[4] = {0};         // states = 0 (off), 1 (on), 2 (off - on)

// UI Section
volatile uint16_t update = 0;
volatile uint8_t ustate = 0;

volatile int8_t wtype = 0;               // 0 is square, 1 is sine, 2 is triangle 
volatile int8_t ctype = 0;

volatile uint8_t dcount[4] = {0};
volatile int8_t digit = 3;

volatile int32_t freq = 0;
volatile int32_t cfreq = 0;

// Initialise SPI instance for communication with the display
SPIPreInit gSpi(D_MOSI_PIN,NC,D_CLK_PIN); //MOSI,MISO,CLK

// Initialise display driver instance
Adafruit_SSD1306_Spi gOled1(gSpi,D_DC_PIN,D_RST_PIN,D_CS_PIN,64,128); //SPI,DC,RST,CS,Height,Width

// DEBUGGING 
//for(int i = 0; i < 4; i++) {
//    // Write the SW0 osciallor count as kHz
//    gOled1.printf("SW%01u: %02ukHz", i, sfreq[i]);
//    if(son[i]){
//        gOled1.printf(" - ON \n");
//    } else {
//        gOled1.printf(" - OFF\n");
//    }
//}
            

int main() { 
    // Initialisation
    gOled1.setRotation(2); //Set display rotation
    gOled1.clearDisplay();
    
    // Precalculate sine wave values
    for(int i = 0; i < 128; i++) {
        sine_wave[i]=((1.0 + sin((float(i)/128.0*6.28318530717959)))/2.0);
    }
    
    // TODO: Precalculate triangle wave values
    for(int i = 0; i < 128; i++) {
        tri_wave[i]=((1.0 + sin((float(i)/128.0*6.28318530717959)))/2.0);
    }
    
    // Attach switch oscillator counter ISR to the switch input instance for a rising edge
    swin0.rise(&sedge0);
    swin1.rise(&sedge1);
    swin2.rise(&sedge2);
    swin3.rise(&sedge3);
    
    // Attach switch sampling timer ISR to the timer instance with the required period
    swtimer.attach_us(&tout, SW_PERIOD);
    
    // Main loop
    while(1) {
        //
        // sinewave output
        //if (ctype==2){
//            sin_out(aout, cfreq);
//        }
        
        
        // Has the update flag been set?       
        if (update) {
            // Clear the update flag
            update = 0;
            
            for (int i = 0; i<4; i++){
                // set button states
                if (sstate[i] == 2) {
                    sstate[i] = 1;
                } else if ((sstate[i] == 0) && (son[i] == 1)) {
                    sstate[i] = 2;
                } else if ((sstate[i] == 1) && (son[i] == 0)) {
                    sstate[i] = 0;      
                }
            }
            
            // increment freq digit with SW0
            if (sstate[0] == 2) {
                dcount[digit]++;
                if(dcount[digit] > 9) {
                     dcount[digit] = 0;
                }
            }
            
            // move freq digit pointer with SW1
            else if (sstate[1] == 2) {
                digit--;
                if(digit < 0) {
                     digit = 3;
                }  
            }
            
            // select type of wave with SW2
            else if (sstate[2] == 2) {
                wtype++;
                if (wtype > 2) {
                    wtype = 0;
                }     
            }
            
            // SW3 confirms values and makes wave
            else if (sstate[3] == 2) {
                cfreq = freq;
                ctype = wtype;
                
                if (cfreq != 0) {
                    if (ctype == 0){
                        // SQUARE WAVE
                        pout.period_us(1000000/freq); 
                        pout.write(0.5);
                        
                    } else if (ctype == 1) {
                        // SINE WAVE
                        sine_ticker.attach(&sine_interrupt, 1/(cfreq*128));
                    } else if (ctype == 2) {
                        // TRIANGLE WAVE
                        tri_ticker.attach(&tri_interrupt, 1/(cfreq*128));
                    }
                }
            }
            
            // calculate the frequency
            freq = 0;
            for (int i = 0; i < 4; i++) {
                freq += dcount[i]*(power(10, i));
            }
            
            // PRINTING
            printUI(gOled1, sstate);
            // Print choice of wave type
            gOled1.setTextCursor(0,0);
            gOled1.printf("SQR      SIN      TRI");
            gOled1.printf("                     ");
            switch(wtype) {
                case 0: gOled1.setTextCursor(0,8); break;
                case 1: gOled1.setTextCursor(54,8); break;
                case 2: gOled1.setTextCursor(108,8); break; 
                default: gOled1.setTextCursor(0,8); break;
            }
            gOled1.printf("---");
            
            // Print choice of digits
            gOled1.setTextCursor(0,16);
            gOled1.printf("%01u,%01u%01u%01uHz\n", dcount[3], dcount[2], dcount[1], dcount[0]);
            gOled1.printf("         ");
            switch(digit) {
                case 0: gOled1.setTextCursor(24,24); break;
                case 1: gOled1.setTextCursor(18,24); break;
                case 2: gOled1.setTextCursor(12,24); break;
                case 3: gOled1.setTextCursor(0,24);  break;
                default: gOled1.setTextCursor(48,24); break;
            }
            gOled1.printf("-\n");
            
            // Print set values of wave type
            gOled1.setTextCursor(0,32);
            gOled1.printf("wave type: ");
            switch(ctype) {
                case 0: gOled1.printf("SQR"); break;
                case 1: gOled1.printf("SIN"); break;
                case 2: gOled1.printf("TRI"); break; 
                default: gOled1.printf("SQR"); break;
            }
            
            // Print set values of frequency and
            gOled1.setTextCursor(0,40);
            gOled1.printf("frequency: %04uHz", cfreq);
            
            // Copy the display buffer to the display
            gOled1.display();
            
            // Toggle the alive LED
            alive = !alive;
        }
        
        
    }
}

void printUI(Adafruit_SSD1306_Spi& gOled, volatile uint8_t state[4]){
    gOled.setTextCursor(0,56);
    if (state[3]){
        gOled.printf("^(f)  >(f)  >(t)     ");
    }
    else if (state[2]){
        gOled.printf("^(f)  >(f)        CNF");
    }
    else if (state[1]){
        gOled.printf("^(f)        >(t)  CNF");
    }
    else if (state[0]){
        gOled.printf("      >(f)  >(t)  CNF");
    }
    else {
        gOled.printf("^(f)  >(f)  >(t)  CNF");
    }
    gOled.printf("^(f)  >(f)  >(t)  CNF");
}

// Interrupt Service Routine for rising edge on the switch oscillator input
void sedge0() {
    // Increment the edge counter
    scounter[0]++;    
}

void sedge1() {
    // Increment the edge counter
    scounter[1]++;    
}

void sedge2() {
    // Increment the edge counter
    scounter[2]++;    
}

void sedge3() {
    // Increment the edge counter
    scounter[3]++;    
}

// Interrupt Service Routine for the switch sampling timer
void tout() {
    
    for(int i = 0; i < 4; i++) {
        // Read the edge counter into the output register
        scount[i] = scounter[i];
        // Reset the edge counter
        scounter[i] = 0;
        // calculate the frequency
        sfreq[i] = (scount[i]*1000)/SW_PERIOD;
        // check if buttons are pressed
        if((son[i]==0) && (sfreq[i] < THOLD_ON)) {
            son[i] = 1;
        } else if ((son[i]==1) && (sfreq[i] > THOLD_OFF)) {
            son[i] = 0;
        }
    }
    
    // Trigger a display update in the main loop
    update = 1;
}

void sine_interrupt() {
    // send next analog sample out to D to A
    DAC = sine_wave[sin_count];
    // increment pointer and wrap around back to 0 at 128
    sin_count = (sin_count + 1) & 0x07F;
}

void tri_interrupt() {
    // send next analog sample out to D to A
    DAC = tri_wave[tri_count];
    // increment pointer and wrap around back to 0 at 128
    tri_count = (tri_count + 1) & 0x07F;
}

uint32_t power(uint32_t base, uint8_t exp){
    
    uint32_t res = 1;
    
    for (int i = 0; i < exp; i ++) {
        res *= base;
    }
    
    return res;
}
