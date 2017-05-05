#include "mbed.h"
#include "Adafruit_SSD1306.h"

//Switch input definition
#define SW_PIN0 p21
#define SW_PIN1 p22
#define SW_PIN2 p23
#define SW_PIN3 p24

//Sampling period for the switch oscillator (us)
#define SW_PERIOD 20000 

//Display interface pin definitions
#define D_MOSI_PIN p5
#define D_CLK_PIN p7
#define D_DC_PIN p8
#define D_RST_PIN p9
#define D_CS_PIN p10

//an SPI sub-class that sets up format and clock speed
class SPIPreInit : public SPI
{
public:
    SPIPreInit(PinName mosi, PinName miso, PinName clk) : SPI(mosi,miso,clk)
    {
        format(8,3);
        frequency(2000000);
    };
};

//Interrupt Service Routine prototypes (functions defined below)
void sedge0();
void sedge1();
void sedge2();
void sedge3();

void tout();

//Output for the alive LED
DigitalOut alive(LED1);

//External interrupt input from the switch oscillator
InterruptIn swin0(SW_PIN0);
InterruptIn swin1(SW_PIN1);
InterruptIn swin2(SW_PIN2);
InterruptIn swin3(SW_PIN3);

//Switch sampling timer
Ticker swtimer;

//Registers for the switch counter, switch counter latch register and update flag
volatile uint16_t scounter0=0;
volatile uint16_t scount0=0;

volatile uint16_t scounter1=0;
volatile uint16_t scount1=0;

volatile uint16_t scounter2=0;
volatile uint16_t scount2=0;

volatile uint16_t scounter3=0;
volatile uint16_t scount3=0;

volatile uint16_t update=0;

//Initialise SPI instance for communication with the display
SPIPreInit gSpi(D_MOSI_PIN,NC,D_CLK_PIN); //MOSI,MISO,CLK

//Initialise display driver instance
Adafruit_SSD1306_Spi gOled1(gSpi,D_DC_PIN,D_RST_PIN,D_CS_PIN,64,128); //SPI,DC,RST,CS,Height,Width

int main() { 
    //Initialisation
    gOled1.setRotation(0); //Set display rotation
    
    //Attach switch oscillator counter ISR to the switch input instance for a rising edge
    swin0.rise(&sedge0);
    swin1.rise(&sedge1);
    swin2.rise(&sedge2);
    swin3.rise(&sedge3);
    
    //Attach switch sampling timer ISR to the timer instance with the required period
    swtimer.attach_us(&tout, SW_PERIOD);
    
    //Write some sample text
    //gOled1.printf("%ux%u OLED Display\r\n", gOled1.width(), gOled1.height());
    
    //Main loop
    while(1)
    {
        //Has the update flag been set?       
        if (update) {
            //Clear the update flag
            update = 0;
            
            //Write the SW0 osciallor count as kHz
            gOled1.setTextCursor(0,0);
            gOled1.printf("\n%05u  ",(scount0/20));
            
            //Write the SW1 osciallor count as kHz
            //gOled1.setTextCursor(5,0);
            gOled1.printf("\n%05u  ",(scount1/20));
            
            //Write the SW2 osciallor count as kHz
            //gOled1.setTextCursor(10,0);
            gOled1.printf("\n%05u  ",(scount2/20));
            
            //Write the SW3 osciallor count as kHz
            //gOled1.setTextCursor(15,0);
            gOled1.printf("\n%05u  ",(scount3/20));
            
            //Copy the display buffer to the display
            gOled1.display();
            
            //Toggle the alive LED
            alive = !alive;
        }
        
        
    }
}


//Interrupt Service Routine for rising edge on the switch oscillator input
void sedge0() {
    //Increment the edge counter
    scounter0++;    
}

void sedge1() {
    //Increment the edge counter
    scounter1++;    
}

void sedge2() {
    //Increment the edge counter
    scounter2++;    
}

void sedge3() {
    //Increment the edge counter
    scounter3++;    
}

//Interrupt Service Routine for the switch sampling timer
void tout() {
    //Read the edge counter into the output register
    scount0 = scounter0;
    scount1 = scounter1;
    scount2 = scounter2;
    scount3 = scounter3;
    //Reset the edge counter
    scounter0 = 0;
    scounter1 = 0;
    scounter2 = 0;
    scounter3 = 0;
    //Trigger a display update in the main loop
    update = 1;
}

