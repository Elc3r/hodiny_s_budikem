# Hodiny s budíkem – ATmega32A

Tento projekt implementuje **digitální hodiny s budíkem** na mikrokontroléru **ATmega32A**. Ovládání probíhá pomocí maticové klávesnice 4x4, čtyřmístného 7-segmentového displeje a čtyř LED diod.

## Funkce

- **Zobrazení aktuálního času** – na displeji jsou zobrazeny hodiny a minuty.
- **Nastavení času (režim hodin)** – možnost nastavit hodiny a minuty, sekundy se po nastavení vynulují.
- **Nastavení času budíku (režim budíku)** – možnost nastavit čas buzení.
- **Indikace režimů nastavování** – LED na PB2 svítí při nastavování hodin, LED na PB1 při nastavování budíku.
- **Indikace uplynutí sekundy** – LED na PB3 bliká s frekvencí 1 Hz.
- **Signalizace budíku** – LED na PB0 bliká 1 Hz při vyzvánění budíku, dokud není stisknuta libovolná klávesa.
- **Běh hodin i během nastavování budíku** – čas běží i při nastavování budíku, bez zpoždění.

## Ovládání

- **Klávesnice 4x4:**
  - `0–9` – číslice (nepoužívají se přímo, pouze pro rozšíření)
  - `A` (10) – inkrementace hodin v režimu nastavování
  - `B` (11) – inkrementace minut v režimu nastavování
  - `C` (12) – vstup/výstup do režimu nastavování hodin
  - `D` (13) – vstup/výstup do režimu nastavování budíku

## Hardware

- **Mikrokontrolér:** ATmega32A
- **Klávesnice:** 4x4 maticová (PORTC)
- **Displej:** 4-místný 7-segmentový (PORTA – segmenty, PORTD – pozice)
- **LED indikace:** PB0–PB3 (aktivní v log.0)
  - PB0 – signalizace budíku (bliká při vyzvánění)
  - PB1 – indikace režimu nastavování budíku
  - PB2 – indikace režimu nastavování hodin
  - PB3 – sekundová indikace (1 Hz)

## Kompilace a nahrání

### Požadavky

- **Intel macOS**
- **Homebrew**
- Nainstalované závislosti:
  - `avr-gcc`
  - `avrdude`

### Kompilace

Projekt obsahuje Makefile umístěný v **kořenové složce** repozitáře. Pro překlad spusťte v rootu repozitáře:

```sh
make
```

### Nahrání do mikrokontroléru

Nahrání HEX souboru do ATmega32A pomocí programátoru (např. USBasp):

```sh
avrdude -c usbasp -p m32a -U flash:w:Debug/main.hex:i
```

> **Poznámka:** Přiložené nástroje a konfigurace jsou určeny pouze pro **Intel macOS** s nainstalovanými závislostmi přes Homebrew. Na jiných systémech nebo architekturách (např. Apple Silicon) nemusí fungovat bez úprav.
