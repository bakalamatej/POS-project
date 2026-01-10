# Používateľská dokumentácia

Projekt obsahuje **server** a **klienta** pre simuláciu náhodnej prechádzky (random walker) v mriežke.
Komunikácia server–klient používa **POSIX** mechanizmy: zdieľanú pamäť a UNIX doménový socket.

> Dôležité: server aj klient musia bežať na **tom istom Linux/WSL prostredí**, pretože zdieľaná pamäť aj socket sú lokálne.

## Požiadavky na systém

Tento projekt je určený pre Linuxové prostredie. Spúšťanie je podporené:

- cez SSH na školský server: **ssh frios2.fri.uniza.sk**
- cez **WSL**
- na ľubovoľnom Linuxe v príkazovom riadku

Nepodporuje sa natívne spúšťanie mimo Linux/WSL, keďže projekt používa POSIX funkcie (napr. `shm_open`, UNIX sockety, `termios`, linkovanie `-lrt`).

### Čo treba doinštalovať ak to ešte nemáte (WSL / Linux)

Na kompiláciu potrebuješ C kompilátor a `make`. Na Ubuntu/WSL nainštaluješ všetko naraz cez balík `build-essential`:

```bash
sudo apt update
sudo apt install build-essential
```

Týmto získaš `gcc`, `make` a štandardné vývojové knižnice potrebné pre POSIX programy.

## Kompilácia

V priečinku `sem/` spusti:

```bash
make
```

Vytvoria sa dve binárky:

- `server`
- `client`

## Spustenie (odporúčaný postup)

Najjednoduchší spôsob je spustiť **len klienta** – klient vie server spustiť sám na pozadí.

### Variant A: klient spustí server automaticky

```bash
./client
```

V menu zvoľ:

- **[1] New simulation** – klient sa spýta na parametre a spustí `./server ...` automaticky.

### Variant B: server spustíš ručne a potom sa pripojíš klientom

V jednom termináli:

```bash
./server [parametre]
```

V druhom termináli:

```bash
./client
```

V klientovi zvoľ **[2] Connect to server** a vyber PID servera.

**Poznámka:** server po spustení **čaká, kým sa pripojí prvý klient**, a až potom začne simuláciu.

## Parametre servera (podľa kódu)

Server podporuje tieto prepínače:

- `-s <world_size>` veľkosť sveta (ak nepoužívaš prekážky)
- `-r <replications>` počet replikácií (iterácií celej simulácie)
- `-k <max_steps>` maximálny počet krokov (limit pre prechádzku)
- `-p <up> <down> <left> <right>` pravdepodobnosti pohybu (4 čísla)
- `-f <obstacles_file>` súbor s prekážkami (vtedy sa veľkosť sveta berie zo súboru)
- `-o <output_file>` názov výstupného súboru s výsledkami
- `-l <resume_file>` obnovenie zo súboru v `saved/` (resume)

### Príklady

Svet bez prekážok (10×10), 100 replikácií, max 200 krokov:

```bash
./server -s 10 -r 100 -k 200 -p 0.25 0.25 0.25 0.25 -o out.txt
```

Svet s prekážkami (veľkosť sa číta zo súboru):

```bash
./server -f obstacles.txt -r 500 -k 200 -p 0.25 0.25 0.25 0.25 -o out.txt
```

Obnovenie (resume) existujúcej simulácie zo `saved/`:

```bash
./server -l nejaky_subor.txt -r 1000 -o nove_vysledky.txt
```
# Pripojenie viacerých klientov k tej istej simulácii

Ak už beží server (simulácia), môžeš sa k nej pripojiť z viacerých terminálov.

**Ako na to:**

1. Spusti server podľa návodu vyššie (alebo nech ho spustí prvý klient cez "New simulation").
2. V ďalšom termináli spusti príkaz:

  ```bash
  ./client
  ```

3. V menu zvoľ **[2] Connect to server** a vyber PID bežiaceho servera .

Takto môžeš mať otvorených viac klientov naraz, všetci uvidia rovnaký stav simulácie v reálnom čase.

> Poznámka: Všetci klienti majú len "read-only" pohľad na simuláciu, môžu prepínať zobrazenie (mód/view) nezávisle, ale samotná simulácia beží na serveri.
> 
## Formát súboru `obstacles.txt`

Súbor s prekážkami má formát:

1. prvé číslo = veľkosť sveta `N`
2. potom nasleduje `N × N` čísiel (0/1), kde `1` znamená prekážku

Príklad (pre `N=3`):

```text
3
0 1 0
0 0 0
0 0 1
```

Stredová pozícia (cieľ a štart) nesmie byť zablokovaná – ak je, server ju automaticky odblokuje.

## Ovládanie klienta počas behu

Klient beží v termináli a pravidelne prekresľuje obraz.

Klávesy:

- `1` prepne mód na **interactive**
- `2` prepne mód na **summary** (štatistiky)
- `3` prepne „view“ v summary móde:
  - **average steps** (priemerné kroky pri úspechu)
  - **probability (%)** (úspešnosť v %)
- `ESC` ukončí klienta

## Ukladanie výsledkov a resume

- Server ukladá výsledky do priečinka `saved/`.
- Ak v klientovi zadáš `Output file: out.txt`, reálny súbor bude `saved/out.txt`.
- Voľba **[3] Resume simulation** v klientovi ponúkne `.txt` súbory zo `saved/`.

## Kontakt

V prípade problémov:

- kovalcik3@stud.uniza.sk
- bakala@stud.uniza.sk
