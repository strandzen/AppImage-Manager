# System Analysis & Architectural Critique V2

Dette er en dypere og mer nådeløs teknisk kritikk av den nåværende tilstanden til AppImage Manager, med fokus på manglende funksjoner, flaskehalser, dårlig arkitektur og farlige fallgruver.

## 1. Fallgruven med oversatte systemmapper (Applikasjoner vs Applications)
> [!WARNING]
> **Farlig Arkitektur:** Å oversette den fysiske mappen på disken.
Teksten i selve vinduet (UI-et) er allerede klargjort for oversettelse (via `qsTr("Applications")`), så på norsk *vil* det stå "Applikasjoner" i grensesnittet. 
Men å faktisk opprette den *fysiske* mappen `~/Applikasjoner` på harddisken er en **massiv fallgruve**. 
- Linux XDG-standarden (`xdg-user-dirs`) definerer ikke en standardmappe for applikasjoner. Det de facto fellesskapsstandarden for AppImages er `~/Applications`.
- Hvis vi oversetter mappen, og du installerer tre programmer mens maskinen er på norsk, havner de i `~/Applikasjoner`. Hvis du bytter systemspråk til engelsk neste dag, vil appen plutselig opprette og lete i `~/Applications`. De gamle programmene vil da forsvinne fra systemets oversikt, og KIO vil bli forvirret.
**Konklusjon:** UI-teksten kan oversettes, men den fysiske stien i `core.py` MÅ være hardkodet til `~/Applications` (eventuelt `~/.local/bin`) uavhengig av språk.

## 2. Dårlig kode / Treg løsning: Frysning av brukergrensesnittet (Blocking I/O)
> [!CAUTION]
> **Flaskehals:** Installasjonen fryser appen!
I `core.py` bruker vi `subprocess.run(['kioclient', 'move', path, new_path], check=True)`. Dette er en *synkron* operasjon. Det betyr at Python-tråden stopper helt opp og venter til filen er ferdig flyttet. Hvis du drar en 3 GB stor AppImage fra en treg minnepenn over til harddisken, vil hele QML-vinduet ditt fryse ("Not responding") i flere minutter mens `kioclient` jobber i bakgrunnen. 
**Løsning:** KIO-kommandoen må kjøres asynkront via `subprocess.Popen` (eventuelt en QThread/QProcess) slik at UI-et kan lukkes eller vise en snurrende indikator.

## 3. Treg løsning: Midlertidige ikon-filer på disk
> [!NOTE]
> **Ineffektivitet:** Ikon-håndtering i `gui.py`.
Når vi leser ikonet til en AppImage, henter vi det ut som binærdata (bytes). QML kan ikke vise rå bytes direkte uten en spesiell C++ bilde-leverandør (`QQuickImageProvider`). Vår nåværende "late" løsning er å skrive disse bytene til en ekte, midlertidig fil på disken (via `tempfile`), og be QML laste filen. Dette krever unødvendig skriving til disk (I/O) hver eneste gang man klikker på en AppImage.
**Løsning:** Skriv en tilpasset `QQuickImageProvider` i Python som konverterer bytene direkte til et `QPixmap` i minnet (RAM), uten å berøre harddisken.

## 4. Sikkerhet: Blind tillit til `Exec`-argumenter
> [!CAUTION]
> **Sikkerhetsrisiko:** `.desktop` fil injeksjon.
Når appen oppretter en snarvei, leser den `Exec=` linjen fra AppImage'en sin interne desktop-fil, og kopierer argumentene direkte (f.eks. `--no-sandbox`).
Hvis en ondsinnet AppImage legger til `Exec=AppRun; rm -rf /` i sin interne desktop-fil, vil vår manager blindt opprette en system-snarvei som kjører denne skadelige koden. 
**Løsning:** Vi må parse og sanitere (rense) `Exec`-argumentene med Regex, og nekte å opprette snarveier som inneholder semikolon (`;`), pipes (`|`) eller shell-operatører (`&&`).

## 5. Manglende Feature: Oppdatering av AppImages (zsync)
> [!TIP]
> **Mangler:** Automatiske oppdateringer.
AppImages har et innebygd system for delta-oppdateringer via `AppImageUpdate` / `zsync`. Akkurat nå fungerer appen vår utelukkende som en dørvakt for installasjon/sletting. Den har ingen anelse om AppImage'en er utdatert. Et offisielt KDE-verktøy ville inkludert en "Check for Updates"-knapp som leser zsync-headerne og laster ned kun de endrede bytene av filen.

## 6. Manglende Feature: Opprydding av Foreldreløse Filer (Orphans)
> [!NOTE]
> **Mangler:** Bakgrunnsdaemon for synkronisering.
Hvis brukeren ignorerer verktøyet vårt og simpelthen sletter en fil fra `~/Applications` manuelt i Dolphin, vil `.desktop`-snarveien i startmenyen ligge igjen som en død lenke (orphan). 
**Løsning:** Profesjonelle verktøy (som AppImageLauncher) har en bakgrunnstjeneste som lytter etter endringer (via `inotify`) i `~/Applications`-mappen, og automatisk sletter snarveier hvis selve programfilen forsvinner.
