# Optimaliseringsplan for AppImage Manager

Dette dokumentet skisserer en detaljert plan for å løse ytelsesflaskehalsene og feilene vi avdekket i forrige analyse, direkte basert på dine tilbakemeldinger. 

## Brukergjennomgang Nødvendig
Vennligst se over løsningsforslagene (spesielt hvordan vi flytter filer til papirkurven og håndterer `Exec`-sikkerhet). Gi beskjed om du vil endre noe før vi starter kodingen.

## Åpne Spørsmål
> [!NOTE]
> For å flytte filer til papirkurven (Trash), tenker jeg å bruke KDEs innebygde `kioclient move <fil> trash:/`. Dette er den mest native måten i Plasma. Er dette greit for deg?

---

## Foreslåtte Endringer

### 1. Lynrask Oppstart (Metadata-caching & Smart Navn)
For å forhindre at AppImage-en pakkes ut to ganger (en gang for info, en gang for snarvei-sjekk):
- **Caching i minnet:** Jeg vil legge til en `lru_cache` (minne-buffer) i `core.py`. Når `get_appimage_info` kjøres for første gang, lagres resultatet. Neste gang den kalles, returnerer den umiddelbart.
- **Smart Navne-uthenting:** I praksis betyr dette at funksjonen `is_desktop_link_enabled(path)` *ikke* lenger vil pakke ut filen. Den vil i stedet bare se på filnavnet (f.eks. `krita-5.3.1.AppImage`), bruke regex for å trekke ut `krita`, og sjekke om `appimage_krita.desktop` eksisterer. Dette er 1000 ganger raskere.

### 2. Aldri mer fryst UI (Multithreading for Alt)
For å hindre at appen fryser ved tunge operasjoner:
- **Asynkron Startup:** `get_appimage_info` vil kjøres i en bakgrunnstråd når vinduet åpnes. QML-grensesnittet vil vise en pen "Loading metadata..." indikator mens filen skannes.
- **Asynkron Sletting:** Funksjonene for å finne corpses (`find_app_corpses`) og slette elementer (`remove_items`) vil pakkes inn i asynkrone oppgaver som kommuniserer fremdrift tilbake til hovedvinduet.

### 3. Sletting til Papirkurv (Trash)
Slik løser vi slettingen for å unngå permanent tap av data:
- I stedet for `shutil.rmtree` og `os.remove` (som sletter permanent), vil `remove_items` benytte `subprocess.run(['kioclient', 'move', filbane, 'trash:/'])`. 
- Dette gjør at alle fjernede config-filer, cacher og selve AppImage-filen havner i KDEs standard papirkurv.

### 4. Gjenopprette Ikon-logikk & Prioritere System Theme
Vi skal hente tilbake den robuste ikon-logikken, men snu prioriteten slik du ønsket:
- **Ikon-søk:** Vi gjenoppretter koden som leter dypt i `usr/share/icons/...` og `.DirIcon`.
- **System Theme Først:** I QML-filen vil appen *først* prøve å laste `image://theme/<app_id>` (f.eks. YAMIS/Hatter). Bare hvis systemet ikke har dette ikonet, vil den falle tilbake på det utpakkede ikonet fra selve AppImage-en.
- **.desktop Ikon-bug:** Når snarveien lages, vil den ikke lenger få hardkodet `application-x-executable`. Den vil få `Icon=<app_id>`. KDE vil da automatisk bruke temaet ditt.

### 5. Exec Sanitization (Kommando-sikkerhet)
Slik løser vi sikkerheten i `.desktop`-filens `Exec`-linje på en proff måte:
- Å bruke Regex for å fjerne tegn som `;` og `|` er "hacky". KDEs startmeny utfører ikke `.desktop`-filer via et skall (`bash`), så shell-injection er egentlig ikke hovedrisikoen. 
- Løsningen er å bruke Pythons innebygde `shlex.split` for å rydde opp i AppImage-ens argumenter, fjerne unødvendige KDE/Gnome-spesifikke parameter-flagg (som `%U` eller `%f` hvis de misbrukes), og bare skrive dem rent tilbake. Dette ødelegger ingen gyldige argumenter (som `--no-sandbox`).

### 6. Oppdatere README.md
Legge til en presisering om at verktøyet er optimalisert for **Type 2 AppImages (SquashFS)**, og at eldre Type 1 formater vil fungere, men gi en tregere opplevelse (grunnet manglende squashfuse-støtte).

---

## Verifikasjonsplan

### Manuell Verifikasjon
1.  Kjøre appen med `krita-5.3.1-x86_64.AppImage`.
2.  Bekrefte at appen åpnes *umiddelbart* (viser laster-ikon), og ikke fryser i 3 sekunder.
3.  Sjekke at Krita viser det riktige systemikonet (f.eks. fra YAMIS).
4.  Lage en snarvei, sjekke Start-menyen for at den bruker systemikonet (ikke tannhjulet).
5.  Avinstallere AppImage-en, og sjekke Plasma sin Papirkurv for å bekrefte at filene ble flyttet dit, ikke slettet permanent.
