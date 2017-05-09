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

// Print UI
void printUI(Adafruit_SSD1306_Spi& gOled, uint16_t state);

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

// Registers for the switch counter, switch counter latch register and update flag
volatile uint16_t scounter[4] = {0};
volatile uint16_t scount[4] = {0};
volatile uint16_t sfreq[4] = {0};
volatile bool son[4] = {0};
volatile uint8_t sstate[4] = {0};         // states = 0 (off), 1 (on), 2 (off - on)

// UI Section
volatile uint16_t update = 0;
volatile uint8_t ustate = 0;
volatile uint8_t dcount[7] = {0};
volatile int8_t digit = 6;
volatile int32_t freq = 0;

// Initialise SPI instance for communication with the display
SPIPreInit gSpi(D_MOSI_PIN,NC,D_CLK_PIN); //MOSI,MISO,CLK

// Initialise display driver instance
Adafruit_SSD1306_Spi gOled1(gSpi,D_DC_PIN,D_RST_PIN,D_CS_PIN,64,128); //SPI,DC,RST,CS,Height,Width

int main() { 
    // Initialisation
    gOled1.setRotation(2); //Set display rotation
    gOled1.clearDisplay();
    
    // Attach switch oscillator counter ISR to the switch input instance for a rising edge
    swin0.rise(&sedge0);
    swin1.rise(&sedge1);
    swin2.rise(&sedge2);
    swin3.rise(&sedge3);
    
    // Attach switch sampling timer ISR to the timer instance with the required period
    swtimer.attach_us(&tout, SW_PERIOD);
    
    // Write some sample text
    // gOled1.printf("%ux%u OLED Display\r\n", gOled1.width(), gOled1.height());
    
    // Main loop
    while(1) {
        
        // Has the update flag been set?       
        if (update) {
            
            // Clear the update flag
            update = 0;
            gOled1.setTextCursor(0,0);
            
            //for(int i = 0; i < 4; i++) {
//                // Write the SW0 osciallor count as kHz
//                gOled1.printf("SW%01u: %02ukHz", i, sfreq[i]);
//                if(son[i]){
//                    gOled1.printf(" - ON \n");
//                } else {
//                    gOled1.printf(" - OFF\n");
//                }
//            }
            
            // increment freq digit with SW1
            if (sstate[0] == 2) {
                dcount[digit]++;
                if(dcount[digit] > 9) {
                     dcount[digit] = 0;
                }  
            }
            // move freq digit pointer with SW0
            if (sstate[1] == 2) {
                digit--;
                if(digit < 0) {
                     digit = 6;
                }  
            }
            
            gOled1.printf("%01u,%01u%01u%01u,%01u%01u%01uHz\n", dcount[6], dcount[5], dcount[4], dcount[3], dcount[2], dcount[1], dcount[0]);
            gOled1.printf("         ");
            switch(digit) {
                case 0: gOled1.setTextCursor(48,8); break;
                case 1: gOled1.setTextCursor(42,8); break;
                case 2: gOled1.setTextCursor(36,8); break;
                case 3: gOled1.setTextCursor(24,8); break;
                case 4: gOled1.setTextCursor(18,8); break;
                case 5: gOled1.setTextCursor(12,8); break;
                case 6: gOled1.setTextCursor(0,8); break;
                default: gOled1.setTextCursor(48,8); break;
            }
            gOled1.printf("-\n");
            
            freq = 0;
            for (int i = 0; i < 7; i++) {
                freq += dcount[i]*(power(10, i));
            }
            gOled1.printf("%07uHz", freq);
            
            printUI(gOled1, 0);
            
            // Copy the display buffer to the display
            gOled1.display();
            
            // Toggle the alive LED
            alive = !alive;
        }
        
        
    }
}

void printUI(Adafruit_SSD1306_Spi& gOled, uint16_t state){
    gOled.setTextCursor(0,56);
    if (state){
        gOled.printf(">    ^    CNF    BCK");
    }
    else {
        gOled.printf("<    >    SEL");
    }
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
        // set button states
        if (sstate[i] == 2) {
            sstate[i] = 1;
        } else if ((sstate[i] == 0) && (son[i] == 1)) {
            sstate[i] = 2;
        } else if ((sstate[i] == 1) && (son[i] == 0)) {
            sstate[i] = 0;      
        }
    }
    
    // Trigger a display update in the main loop
    update = 1;
}

uint32_t power(uint32_t base, uint8_t exp){
    
    uint32_t res = 1;
    
    for (int i = 0; i < exp; i ++) {
        res *= base;
    }
    
    return res;
}
