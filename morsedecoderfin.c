
// // Morse code lookup table for A-Z [WITH LETTER REFERENCES]
// const char* morse_table[] = {
//     "A.-", "B-...", "C-.-.", "D-..", "E.", "F..-.", "G--.", "H....", "I..", "J.---",  // A-J
//     "K-.-", "L.-..", "M--", "N-.", "O---", "P.--.", "Q--.-", "R.-.", "S...", "T-",     // K-T
//     "U..-", "V...-", "W.--", "X-..-", "Y-.--", "Z--.."                              // U-Z
//     ".-.-.-",  // Period
//     "-....-"  // Hyphen 

// };


#define F_CPU 16000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <Arduino.h>



// Pin definitions
#define LED_PIN PB0
#define BUTTON_PIN PD2

// LCD Pin Mapping (4-bit mode)
#define LCD_RS PB1    // Pin 9 = PB1
#define LCD_E  PD3
#define LCD_D4 PD4
#define LCD_D5 PD5
#define LCD_D6 PD6
#define LCD_D7 PD7

// Morse timing thresholds (in ms)
#define DOT_THRESHOLD 500
#define LETTER_GAP 95 // originally 92 but to account for certain delays
#define WORD_GAP 262


// Global variables
volatile uint16_t press_start = 0;
volatile uint16_t press_duration = 0;
volatile uint16_t timer0_overflows = 0;
volatile bool new_press = false;
volatile bool button_down = false;
volatile bool timer1_overflowed = false;
volatile bool tracking_word_gap = false;
volatile bool pending_decode = false;
bool decoded_flag = false;
bool pending_word = false;
bool reset_hold = false;
uint32_t ms = 0;
uint16_t timer0_ovf = 0;
uint8_t consecutive_dits = 0;
uint8_t cursor_track = 0;
uint8_t row = 0;

String morse_buffer = "";




// Morse code lookup table for A-Z
const char* morse_table[] = {
    ".-", "-...", "-.-.", "-..", ".", "..-.", "--.", "....", "..", ".---",  // A-J
    "-.-", ".-..", "--", "-.", "---", ".--.", "--.-", ".-.", "...", "-",     // K-T
    "..-", "...-", ".--", "-..-", "-.--", "--.."                              // U-Z
    ".-.-.-",  // Period ('.')
    "-....-"   // Hyphen ('-')

};

// LCD FUNCTIONS (4-bit mode)
void send_nibble(uint8_t nibble);
void lcd_command(uint8_t cmd);

void lcd_data(uint8_t data);
void lcd_set_cursor(uint8_t col, uint8_t row);
void lcd_init(void);
void lcd_print(const char* str);
void lcd_print_char(char c);
void lcd_clear(void);
char decode_morse(const char* code);
void setup_timer1(void);
void setup_timer0();
void external_interrupt_init(void);
void advance_cursor(uint8_t steps);
void clearBuf();
void start_timer0_tracking();
void stop_timer0();
void reset_screen(void);

int main(void);
void send_nibble(uint8_t nibble) {
    // Clear data pins
    PORTD &= ~((1 << LCD_D4) | (1 << LCD_D5) | (1 << LCD_D6) | (1 << LCD_D7));
    if (nibble & 0x01) PORTD |= (1 << LCD_D4);
    if (nibble & 0x02) PORTD |= (1 << LCD_D5);
    if (nibble & 0x04) PORTD |= (1 << LCD_D6);
    if (nibble & 0x08) PORTD |= (1 << LCD_D7);

    // Pulse Enable
    PORTD |= (1 << LCD_E);
    PORTD &= ~(1 << LCD_E);
}

void lcd_command(uint8_t cmd) {
    PORTB &= ~(1 << LCD_RS);  // RS = 9 for command
    //PORTD &= ~(1 << LCD_RS);  // RS = 0 for command
    send_nibble(cmd >> 4);
    send_nibble(cmd & 0x0F);
    _delay_ms(2);
}

void lcd_data(uint8_t data) {
    PORTB |= (1 << LCD_RS);  // RS = 1 for data
    //PORTD |= (1 << LCD_RS);  // RS = 1 for data
    send_nibble(data >> 4);
    send_nibble(data & 0x0F);
    _delay_ms(2);
}
void lcd_set_cursor(uint8_t col, uint8_t row) {
    uint8_t row_offsets[] = {0x00, 0x40};
    lcd_command(0x80 | (col + row_offsets[row]));
}

void lcd_init(void) {
    DDRB |= (1 << LCD_RS); // Set RS (pin 9) as output
    DDRD |= (1 << LCD_E) | (1 << LCD_D4) |
            (1 << LCD_D5) | (1 << LCD_D6) | (1 << LCD_D7);

    _delay_ms(20);
    send_nibble(0x03); _delay_ms(5);
    send_nibble(0x03); _delay_us(200);
    send_nibble(0x03); _delay_us(200);
    send_nibble(0x02); // 4-bit mode

    lcd_command(0x04); //disable auto-increment mode
    lcd_command(0x28); // Function set: 4-bit, 2 lines
    //lcd_command(0x0C); // Display ON, Cursor OFF
    lcd_command(0x0E); // Display ON, Cursor ON, *EXPERIMENTAL*
    lcd_command(0x06); // Entry mode set
    lcd_command(0x01); // Clear display
    _delay_ms(2);
    lcd_set_cursor(0, 0);
}

void lcd_print(const char* str) {
    while (*str) {
        lcd_data(*str++);
    }
}

void lcd_print_char(char c) {
    lcd_data(c);
}

void lcd_clear(void) {
    lcd_command(0x01);
    _delay_ms(2);
}

// Decode Morse
char decode_morse(const char* code) {
    for (uint8_t i = 0; i < sizeof(morse_table); i++) {
        if (strcmp(code, morse_table[i]) == 0) {
            if (strcmp(code, "-....-") == 0) {
                return '-';
            } else if (strcmp(code, ".-.-.-") == 0) {
                return '.';
            } else {
                return 'A' + i; // assuming morse_table[0] = "A", [1] = "B", ...
            }
        }
    }
    return '?'; //unknown character
}

// Timer1: normal mode, prescaler 1024
void setup_timer1(void) {
    TCCR1B |= (1 << CS12) | (1 << CS10);
}

void setup_timer0() {
    TCCR0A = 0;               // Normal mode
    TCCR0B = 0;               // Don't start yet
    TIMSK0 |= (1 << TOIE0);   // Enable overflow interrupt
    TCNT0 = 0;                // set counter
}

// INT0 on any edge
void external_interrupt_init(void) {
    EICRA |= (1 << ISC00);
    EIMSK |= (1 << INT0);
    DDRD &= ~(1 << BUTTON_PIN);
    PORTD |= (1 << BUTTON_PIN);

}
void advance_cursor(uint8_t steps) {
    while (steps--) {
        cursor_track++;
        if (cursor_track >= 16) {
            cursor_track = 0;
            row++;
            if (row >= 2) {
                row = 0;  // Wrap to top
            }
        }
    }
    lcd_set_cursor(cursor_track, row);
}
ISR(INT0_vect) {
    _delay_ms(30);  // Hard debounce

    if (PIND & (1 << BUTTON_PIN)) {
        // Button released
        button_down = false;
        press_duration = (uint16_t)(TCNT1 - press_start);
        new_press = true;
        PORTB &= ~(1 << LED_PIN);

            start_timer0_tracking(); 
    } else {
        // Button pressed (falling edge)
        button_down = true;
        press_start = TCNT1;
        timer1_overflowed = false;
        PORTB |= (1 << LED_PIN);

    }
}

ISR(TIMER0_OVF_vect) {
    if (tracking_word_gap) {
        timer0_overflows++;
    }
}
void clearBuf() {
  morse_buffer = "";
}
void start_timer0_tracking() {
        TCCR0B = (1 << CS02) | (1 << CS00); // Prescaler 1024 → start timer
        tracking_word_gap = true;
    }
void stop_timer0() {
    TCCR0B = 0;              
    tracking_word_gap = false;
    timer0_overflows = 0;
} // Stop timer 

void reset_screen(void) {
    Serial.println("screen has been cleared");
    stop_timer0();
    lcd_clear();
    _delay_ms(50);             // Allow LCD time to stabilize

    clearBuf();

    cursor_track = 0;
    row = 0;
    consecutive_dits = 0;
    lcd_set_cursor(0, 0);      // Set to home position
    reset_hold = true;
    decoded_flag = false;
}

int main(void) {
    lcd_init();
    lcd_clear();
    lcd_set_cursor(0, 0);
    setup_timer0();
    setup_timer1();
    EIFR |= (1 << INTF0);
    external_interrupt_init();
    sei();  // Enable global interrupts
    Serial.begin(9600);
    DDRB |= (1 << LED_PIN);
    PORTB &= ~(1 << LED_PIN);
    _delay_ms(500);
    while (1) {

        if (new_press) {
            Serial.println(timer0_overflows);
            timer0_ovf = timer0_overflows;
            new_press = false;
            reset_hold = false;
            if(consecutive_dits > 4 && decoded_flag){             
                reset_screen();
                _delay_ms(100);
            }
             if (pending_decode) {
                if(pending_word){
                    pending_word = false;
                    advance_cursor(1);
                    _delay_ms(50);
                }
                pending_decode = false;
                stop_timer0();
                char decoded = decode_morse(morse_buffer.c_str());
                lcd_print_char(decoded);
                Serial.print("decoded letter "); 
                Serial.println(decoded);  
                advance_cursor(1);      
                decoded_flag = true;  
                consecutive_dits = 0;
                clearBuf();
                _delay_ms(50);  
                start_timer0_tracking();
                }

            //add DITS AND DAHS to buffer
            ms = ((uint32_t)press_duration * 1024UL) / (F_CPU / 1000UL);
            if(!reset_hold){//only add to buffer if there was no recent reset
                if (ms <= DOT_THRESHOLD) {
                    timer0_overflows = 0;
                    _delay_ms(30);
                    Serial.println("DIT");
                    morse_buffer += ".";
                    consecutive_dits++;
                    
                    Serial.println(morse_buffer);
                }else{             
                    timer0_overflows = 0;
                    _delay_ms(30);
                    Serial.println("DAH");
                    morse_buffer += "-";
                    Serial.println(morse_buffer);
                }
            }
        }//end of new press

        //check for letter & word periods
        cli();
        if(timer0_overflows >= WORD_GAP && !pending_word){
            Serial.print("word gap reached \n");
            pending_word = true;
        }sei();
        if(timer0_overflows >= LETTER_GAP && !pending_decode){
            Serial.print("letter gap reached \n");
            pending_decode = true;
        }
    }//end of while loop
}
