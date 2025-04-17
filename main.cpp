/*
 * Hodiny s budíkem
 *
 *  Popis funkcí:
 *   - Zobrazení aktuálních hodin a minut na 7‑segmentovém displeji
 *   - Nastavení času (režim hodin)
 *   - Nastavení času budíku (režim budíku)
 *   - Indikace režimů nastavování pomocí LED
 *       * PB2 – svítí při nastavování hodin
 *       * PB1 – svítí při nastavování budíku
 *   - Indikace uplynutí každé sekundy pomocí LED na PB3 (1 Hz)
 *   - Signalizace budíku blikáním LED na PB0, dokud není stisknuta libovolná klávesa
 *   - Po uložení hodin (ukončení režimu hodin) se sekundy vynulují
 *   - Běh hodin i během nastavování budíku – bez zpoždění
 *
 * Vstupy:
 *   - Matice 4×4 klávesnice připojená na PORTC (nižší čtyři bity jako řádky, 
 *     horní čtyři bity jako sloupce s pull‑up rezistory)
 *   - Klávesy:
 *       0–9   – číslice
 *       A (10)– inkrementace hodin v režimu nastavování
 *       B (11)– inkrementace minut v režimu nastavování
 *       C (12)– vstup/výstup do režimu nastavování hodin
 *       D (13)– vstup/výstup do režimu nastavování budíku
 *       * (14)– není použito
 *       # (15)– není použito
 *
 * Výstupy:
 *   - 7‑segmentový displej na PORTA (segmenty) a PORTD (výběr pozice)
 *   - 4 LED na PB0–PB3 (aktivní low):
 *       PB0 – signalizace budíku (bliká 1 Hz při vyzvánění)
 *       PB1 – indikace režimu nastavování budíku
 *       PB2 – indikace režimu nastavování hodin
 *       PB3 – sekundová indikace (1 Hz)
 *
 * Created: 17.04.2025
 * Author : Michal Vavřiňák
 */

 //#define F_CPU 16000000 // 16 MHz - není potřeba, protože je definováno v Makefile
 #include <avr/io.h>         // knihovna pro práci s I/O porty mikrokontroléru
 #include <util/delay.h>     // knihovna pro zpoždění
 #include <avr/interrupt.h>  // knihovna pro ovládání přerušení
 #include <stdint.h>         // knihovna pro celočíselné datové typy s pevnou délkou
 
 // Stavové konstanty pro režimy
 #define REZIM_NORMAL   0  // normální chod hodin
 #define REZIM_NAST_HOD 1  // nastavování hodin
 #define REZIM_NAST_BUD 2  // nastavování budíku
 
 // Příznaky a stavové proměnné
 volatile uint8_t sekunda_uplynula = 0;  // příznak uplynutí jedné sekundy
 volatile uint8_t stav_led        = 0;   // toggle bit pro blikání 1 Hz
 
 uint8_t rezim_nastaveni = REZIM_NORMAL; // aktuální režim (normál/hodiny/budík)
 uint8_t z_klavesa;                      // kód poslední stisknuté klávesy
 
 // Proměnné pro aktuální čas
 uint16_t hodiny  = 0;   // 0–23
 uint16_t minuty  = 0;   // 0–59
 uint16_t sekundy = 0;   // 0–59
 
 // Proměnné pro čas budíku
 uint16_t budik_hodiny = 0;  // 0–23
 uint16_t budik_minuty = 0;  // 0–59
 uint8_t  budik_aktivni = 0; // 0 = neaktivní, 1 = aktivní
 uint8_t  budik_signal  = 0; // 0 = nevzvoní, 1 = signalizuje
 
 // Mapa klávesnice 4×4: index podle aktivního
 // řádku (0–3) a detekovaného sloupce (0–3)
 const uint8_t mapa_klaves[4][4] = {
     {  1,  4,  7, 14 },  // |(S13) 1|(S14) 4|(S15) 7|(S16) *|
     {  2,  5,  8,  0 },  // |(S9)  2|(S10) 5|(S11) 8|(S12) 0|
     {  3,  6,  9, 15 },  // |(S5)  3|(S6)  6|(S7)  9|(S8)  #|
     { 10, 11, 12, 13 }   // |(S1)  A|(S2)  B|(S3)  C|(S4)  D|
 };
 
 // Bitové vzory pro znaky 0–9, A–F a mezera na 7‑segmentu
 const uint8_t znaky[] = {
     0b11000000,  // 0
     0b11111001,  // 1
     0b10100100,  // 2
     0b10110000,  // 3
     0b10011001,  // 4
     0b10010010,  // 5
     0b10000010,  // 6
     0b11011000,  // 7
     0b10000000,  // 8
     0b10011000,  // 9
     0b10001000,  // A
     0b10000011,  // b
     0b10100111,  // c
     0b10110001,  // d
     0b10000110,  // E
     0b10001110,  // F
     0b11111111   // mezera
 };
 
 // Masky pro výběr pozice 1.–4. číslice při multiplexování
 const uint8_t poz[] = { 1, 2, 4, 8 };
 
 /*
  * Funkce: cti_klavesu
  * -------------------
  * Skenuje maticovou klávesnici 4×4.
  * Postupně aktivuje jeden řádek (PORTC<0..3>=0), ostatní drží v log.1.
  * Čte vstupní sloupce (PINC<4..7>) a pokud některý spadne na 0,
  * vrátí odpovídající kód z mapy. Pokud není stisknuta žádná klávesa,
  * vrací 99.
  *
  * Návrat: 0–15 kód klávesy, 99 = žádná klávesa
  */
uint8_t cti_klavesu(void) {
    for (uint8_t r = 0; r < 4; r++) {          // cyklus přes všechny 4 řádky
        PORTC = ~(1 << r);                     // aktivuje jeden řádek nastavením na 0, ostatní na 1
        _delay_ms(1);                          // čekání 1ms na ustálení napětí
        for (uint8_t s = 0; s < 4; s++) {      // cyklus přes všechny 4 sloupce
            if ((~PINC) & (1 << (4 + s))) {    // testuje, zda je sloupec s aktivní (log.0)
                return mapa_klaves[r][s];       // pokud ano, vrátí odpovídající kód z mapy kláves
            }
        }
    }
    return 99;                                 // žádná klávesa není stisknuta
}
 
 /*
  * Funkce: cekej_na_pusteni
  * ------------------------
  * Zabraňuje vícenásobnému čtení jednoho stisku.
  * Blokuje smyčku dokud je stisknuta klávesa.
  */
 void cekej_na_pusteni(void) {
     while (cti_klavesu() != 99) ;
 }
 
 /*
  * Funkce: zobraz_znak
  * -------------------
  * Zobrazí znak s indexem 'z' na 7‑segmentu na pozici 'p'.
  * Data segmentů se zapíší na PORTA, výběr pozice na PORTD.
  */
 void zobraz_znak(uint8_t p, uint8_t z) {
     PORTA = znaky[z];    // nastavení segmentů
     PORTD = ~poz[p];     // výběr pozice (aktivní low)
 }
 
 /*
  * ISR(TIMER0_OVF_vect)
  * --------------------
  * Přerušení od přetečení časovače0 (cca 1 kHz multiplexu).
  * Obsluhuje multiplexování 4 číslic na 7‑segmentu.
  * Podle režimu (normál nebo nastavení budíku) volí hodnotu digit.
  */
 ISR(TIMER0_OVF_vect) {
     static uint8_t i = 0;
     uint8_t digit;
 
    if (rezim_nastaveni == REZIM_NAST_BUD) {
        // v režimu budíku zobrazujeme budik_minuty/hodiny
        if (i < 2) {
           if (i == 0) {
              digit = budik_minuty % 10;
           } else {
              digit = (budik_minuty / 10) % 10;
           }
        } else {
           if (i == 2) {
              digit = budik_hodiny % 10;
           } else {
              digit = (budik_hodiny / 10) % 10;
           }
        }
    } else {
        // v normálu nebo při nastavování hodin zobrazujeme minuty/hodiny
        if (i < 2) {
           if (i == 0) {
              digit = minuty % 10;
           } else {
              digit = (minuty / 10) % 10;
           }
        } else {
           if (i == 2) {
              digit = hodiny % 10;
           } else {
              digit = (hodiny / 10) % 10;
           }
        }
    }
 
     zobraz_znak(i, digit);
     i = (i + 1) & 3;  // cyklicky 0 → 1 → 2 → 3 → 0
 }
 
 /*
  * ISR(TIMER1_COMPA_vect)
  * ----------------------
  * Přerušení od Compare Match A časovače1 – generuje přesnou 1 Hz.
  * Na jede výstupní LED3 (PB3) toggluje stav_led a nastaví
  * příznak sekunda_uplynula pro hlavní smyčku.
  */
 ISR(TIMER1_COMPA_vect) {
     sekunda_uplynula = 1;      // signalizuj hlavní smyčce
     stav_led         = !stav_led;
 }
 
 int main(void) {
     // --- Inicializace portů ---
     DDRA = 0xFF;            // PORTA[0..7] = výstup pro segmenty
     DDRD = 0x0F;            // PORTD[0..3] = výstup pro pozice
     DDRC = 0x0F;            // PORTC[0..3] = řádky klávesnice
     PORTC = 0xFF;           // pull‑up na sloupcích klávesnice
     DDRB |= 0x0F;           // PB0–PB3 = výstupy pro LED (aktivní low)
     PORTB |= 0x0F;          // inicialně všechny LED zhasnuté (1)
 
     // --- Inicializace Timer0 pro multiplexování ---
     TCCR0  = (1 << CS02);   // prescaler = 256
     TIMSK |= (1 << TOIE0);  // povolit přerušení při přetečení
 
     // --- Inicializace Timer1 pro 1 Hz taktování ---
     TCCR1B = (1 << WGM12)   // CTC režim
            | (1 << CS12)    // prescaler = 1024
            | (1 << CS10);
     OCR1A = 15624;          // 16 MHz/1024/15625 ≈ 1 Hz
     TIMSK |= (1 << OCIE1A); // povolit Compare Match A
 
     sei(); // povolení globálních přerušení
 
     // --- Hlavní smyčka programu ---
     while (1) {
         // 1) Čtení klávesy a zrušení signalizace budíku
         z_klavesa = cti_klavesu();
         if (budik_signal && z_klavesa != 99) {
             // jakákoli klávesa během zvonění vypne alarm
             budik_signal = 0;
             cekej_na_pusteni();
         }
 
         // 2) Přepnutí/režim nastavení hodin (klávesa C = 12)
         if (z_klavesa == 12) {
             if (rezim_nastaveni == REZIM_NORMAL) {
                 rezim_nastaveni = REZIM_NAST_HOD;
             } else if (rezim_nastaveni == REZIM_NAST_HOD) {
                 // uložení hodin, návrat do normálu, vynulování sekund
                 rezim_nastaveni = REZIM_NORMAL;
                 sekundy = 0;
             }
             cekej_na_pusteni();
         }
 
         // 3) Přepnutí/režim nastavení budíku (klávesa D = 13)
         if (z_klavesa == 13) {
             if (rezim_nastaveni == REZIM_NORMAL) {
                 rezim_nastaveni = REZIM_NAST_BUD;
             } else if (rezim_nastaveni == REZIM_NAST_BUD) {
                 // uložení budíku, aktivace alarmu
                 rezim_nastaveni = REZIM_NORMAL;
                 budik_aktivni = 1;
             }
             cekej_na_pusteni();
         }
 
         // 4) Inkrementace hodin/minut dle aktivního režimu
         if (rezim_nastaveni == REZIM_NAST_HOD) {
             if (z_klavesa == 10) {        // A – hodiny
                 hodiny = (hodiny + 1) % 24;
                 cekej_na_pusteni();
             }
             if (z_klavesa == 11) {        // B – minuty
                 minuty = (minuty + 1) % 60;
                 cekej_na_pusteni();
             }
         }
         if (rezim_nastaveni == REZIM_NAST_BUD) {
             if (z_klavesa == 10) {        // A – hodiny budíku
                 budik_hodiny = (budik_hodiny + 1) % 24;
                 cekej_na_pusteni();
             }
             if (z_klavesa == 11) {        // B – minuty budíku
                 budik_minuty = (budik_minuty + 1) % 60;
                 cekej_na_pusteni();
             }
         }
 
         // 5) Běh času a kontrola budíku každou sekundu
         if (sekunda_uplynula) {
             // zvýšení sekund
             sekundy++;
             if (sekundy > 59) {
                 sekundy = 0;
                 // přetečení minut
                 if (++minuty > 59) {
                     minuty = 0;
                     hodiny = (hodiny + 1) % 24;
                 }
             }
             // spuštění alarmu v přesný čas (sekundy == 0)
             if (budik_aktivni && !budik_signal &&
                 hodiny == budik_hodiny &&
                 minuty == budik_minuty &&
                 sekundy == 0) {
                 budik_signal = 1;
             }
             sekunda_uplynula = 0;
         }
 
         // 6) Výpočet a zobrazení stavu LED (aktivní low)
         //    PB3: sekundová indikace (stav_led toggluje 1 Hz)
         //    PB2: režim nastavování hodin
         //    PB1: režim nastavování budíku
         //    PB0: signalizace budíku (bliká 1 Hz)
         // Výchozí hodnota pro LED - všechny LED jsou vypnuté (log.1, protože jsou aktivní v log.0)
         uint8_t led_out = 0x0F;  // 0b00001111

         // Pokud je stav_led=1 (každou druhou sekundu), rozsvítí LED na PB3 (sekundová indikace)
         if (stav_led) {
             led_out &= ~(1 << PB3);  // Vynuluje bit PB3, ostatní bity zachová
         }

         // Obsluha signalizace budíku a LED indikací režimů
         if (budik_signal) {
             // Pokud budík zvoní, bliká LED na PB0 v rytmu stav_led (1 Hz)
             if (stav_led) {
                 led_out &= ~(1 << PB0);  // Rozsvítí LED budíku v taktu 1 Hz
             }
         } else {
             // Pokud budík nezvoní, zobrazují se indikace režimů
             if (rezim_nastaveni == REZIM_NAST_HOD) {
                 led_out &= ~(1 << PB2);  // Rozsvítí LED pro režim nastavení hodin
             }
             if (rezim_nastaveni == REZIM_NAST_BUD) {
                 led_out &= ~(1 << PB1);  // Rozsvítí LED pro režim nastavení budíku
             }
         }

         // Aktualizuje pouze spodní 4 bity PORTB (LED), horní 4 bity zachová beze změny
         PORTB = (PORTB & ~0x0F) | led_out;  // (~0x0F = 0b11110000)
     }
 }
 