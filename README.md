# Chipmatic DrillBoost Mod

Una piccola MOD per **Chipmatic** su Windows/Steam che rende il drill molto piu potente e riduce i costi delle stazioni principali.

Questa repository non contiene file originali del gioco. Include solo uno script che modifica i dati JSON della copia installata localmente.

## Cosa cambia

- Drill piu veloce e produttivo
- Capacita del drill aumentata
- Consumo energia del drill ridotto
- Costi ridotti per:
  - Battery
  - Furnace
  - Garage
  - Laboratory
  - Refiner
  - Solar
  - Crystal
  - Atom

## Valori principali

| Voce | Originale | Modificato |
| --- | ---: | ---: |
| Drill speedMultiplier | 1 | 3 |
| Drill capacity | 20 | 80 |
| Drill production amount | x1 | x3 |
| Drill extraction time | 60 | 30 |
| Drill energy cost | x1 | circa x0.5 |
| Station costs | x1 | circa x0.5 |

I costi non vengono mai portati sotto `1`, per evitare valori strani o oggetti gratuiti.

## Mod Manager GUI (consigliato)

Il progetto include **chipmatic_mod.exe**, un'interfaccia grafica Win32 che permette di modificare i parametri direttamente senza usare PowerShell.

### Utilizzo del Mod Manager

1. Chiudi Chipmatic.
2. Avvia `chipmatic_mod.exe`.
3. Imposta i valori desiderati nei campi.
4. Clicca **APPLICA TUTTO**.
5. Clicca **RESET SALVATAGGIO** (necessario ad ogni modifica).
6. Avvia Chipmatic normalmente con una nuova partita.

> **Nota:** Ad ogni modifica dei parametri, ricordatevi di cliccare su **APPLICA TUTTO** e **RESET SALVATAGGIO**.

Per compilare il sorgente (`src/chipmatic_mod.c`) su Windows con MinGW:

```bat
build.bat
```

Oppure con MSVC (Visual Studio 2019+):

```bat
_build_msvc.bat
```

---

## Requisiti

- Chipmatic installato da Steam su Windows
- PowerShell (per lo script alternativo) o solo Windows (per il Mod Manager .exe)
- Una copia legittima del gioco

Percorso predefinito supportato:

```text
C:\Program Files (x86)\Steam\steamapps\common\Chipmatic
```

## Installazione

1. Chiudi Chipmatic.
2. Apri PowerShell.
3. Vai nella cartella della repository.
4. Esegui:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\install-mod.ps1
```

Lo script crea automaticamente un backup di:

```text
www\buildingsStats.json
```

Il backup viene salvato nella stessa cartella del file originale con un nome simile a:

```text
buildingsStats.json.backup-YYYYMMDD-HHMMSS
```

## Ripristino

Per tornare ai dati originali, copia il backup sopra `buildingsStats.json`.

In alternativa, da Steam:

1. Clic destro su Chipmatic
2. Proprieta
3. File installati
4. Verifica integrita dei file del gioco

## Note

Alcuni valori potrebbero essere copiati dentro il salvataggio quando inizi una partita. Se non vedi subito gli effetti, prova una nuova partita.

Questa MOD modifica solo file dati leggibili. Non modifica eseguibili, DLL, DRM, Steamworks o sistemi anti-cheat.

