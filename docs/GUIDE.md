# CRITICAL 4.0 — guide

*TACET Laboratories, Reactor Division.*

**CRITICAL is a synthesizer powered by a reactor.** The sound is not played: it is produced by a model of a nuclear core, the way a power station produces its current. Your hands do not play notes, **they pull control rods.** The reactor does the rest: it rises, settles, poisons itself, drifts on its own, and can run away.

## Install

Open the installer page, **https://isod89.github.io/critical-fm1/** (or `installer-critical-4.0.html` from the release, opened from your disk), in Chrome or Edge. It explains every step.

1. **m-bootloader first (once).** [m-bootloader](https://github.com/jvitkauskas/m-bootloader) is a free, open-source bootloader for the FM-1 by Julius Vitkauskas (MIT licence). It is required **for your safety**: it checks the firmware before starting it, and if an install is interrupted or a firmware is damaged, the FM-1 opens a recovery menu instead of hanging. Hold **PLAY** at power-on for the menu (MIDI MODE); hold **REC** for USB download mode (M-Flasher). The page gives two ways to install it: with M-VAVE's M-UPGRADE app and `fm1-uboot.fwsc` (the simplest), or with [M-Flasher](https://github.com/jvitkauskas/m-flasher) after a full backup. It also warns about the newer flash chip (UC25HQ80) and about M-VAVE's official updates, which put their own bootloader back.
2. **Connect and back up.** Switch the FM-1 on while holding **PLAY**, choose **MIDI MODE**, then in the browser: **1. Connect**, **2. Back up** (the whole flash, 1 MB, to put the FM-1 back exactly as it was with M-Flasher).
3. **3. Install CRITICAL.** The FM-1 restarts on the plant's terminal.

## Start-up: the terminal

At power-on the screen is the plant's terminal, green on black: the self-check (memory, core monitor, delayed neutrons, rods, source counting, the AZ-5 circuit), then the operator's login, typed letter by letter (LOGIN: OPERATOR, the password in stars), ACCESS GRANTED and the last login. Then the session's menu recalls the controls, and **> PRESS PLAY** waits. If a saved plant came back, the second line says so.

## The reactor in one minute

- **Sixteen fuel channels**, one per white key, each tuned to its key's note (in the key and scale you chose). They sit in a ring, laid out **in fourths**: a channel that heats up warms its neighbours, a fourth above and below.
- **Each channel is a small reactor**: six groups of delayed neutrons (Keepin's data for uranium 235), its rod, its fuel temperature, its steam, its xenon. The arithmetic is the operating manuals' (the prompt jump approximation, reactivity in dollars).
- **A channel's sound is its neutron flux, as a detector hears it.** At low power: single counts, struck into the channel's resonances. As the flux rises, the counts become a grain, then a held tone, which brightens with the power the way Cherenkov light turns bluer.
- **A neutron source** keeps every channel counting a little, all the time: the bed of the sound, even with no key held.
- **The regulator (AR)** moves a bank of rods to hold the total power. So when you raise one channel, it takes power from the others: the harmony rearranges itself.
- **Xenon**: a channel held for a long time poisons itself after you let it go, and the poison moves around the ring. The power, and with it the harmony, drifts by itself for minutes. That is what makes the sound hypnotic: it never quite repeats.
- **Void (steam)**: a positive coefficient, as in an RBMK. Past a point, the channels ring, boil, then run away.
- **AZ-5**: every rod goes in, graphite tips first. For one second, the bottom of the core gains reactivity. With a lot of steam and a low margin (many rods out), the core can go **prompt critical**: a blow, the cut, two and a half seconds of nothing, then everything restarts cold.

Around it, **the loop** follows the reactor's heat, not your hands: the turbine spins faster, the pumps beat harder, the generator's hum takes its load, the Geiger counter speeds up. And **the hall** keeps six seconds of what is played and plays it back into a room that keeps only its own resonances, generation after generation (as in Alvin Lucier's room, or the rooms of Chernobyl Jacob Kirkegaard recorded): what is played dissolves into the hall's drone.

## Playing

| You want to | You do |
|---|---|
| Raise a channel | **hold its white key**: its rod comes out. The power rises as fast as its period allows |
| Let it fall back | let go: the rod goes in, the power falls slowly (the delayed neutrons) |
| Leave rods out | **ARP** (HOLD); the key again puts its rod back; switching HOLD off puts them all back |
| Let the reactor play by itself | **SEQ**: GEN, the hand that pulls rods by itself |
| Change the atmosphere | **PRESETS**: the scene |
| Strike a sound of the plant | the black keys |
| Another plant | **hold PLAY** one second |
| See and set everything that was drawn | **HOME** |
| Keep what you set | **hold SAVE** one second |
| Stop everything | **hold REC** one second: **AZ-5** |

- **OCT- / OCT+**: the keyboard's octave (one octave down by default).
- **SELECT**: the key. **ALGORITHM**: the scale (PHRYGIAN, AEOLIAN, LOCRIAN, HARMONIC MINOR, HUNGARIAN MINOR, DORIAN).
- A channel whose rod is out has its key lit. Its neighbours a fourth away warm with it.

## The pages and their four knobs

**FX SEL ENV LFO GLO** choose the page. **Hold EDIT** for the page's second row. On CHANNELS, MODERATOR and HALL, **pressing the same button again** switches that source off or on.

| Button | Page | KNOB 1 – 4 | EDIT held |
|---|---|---|---|
| **FX** | REACTOR | BANK (the bank of rods, the regulator's limit) · VOID (the steam) · XENON (the poison) · TEMPO (how fast the core's time runs) | COUPLING (how many neutrons the channels pass each other) · AR TARGET (the power the regulator holds) · AR SPEED (0: the regulator off) · SOURCE (the neutron source) |
| **SEL** | CHANNELS | RING (how long the resonances ring) · CHERENKOV (how much the sound brightens with power) · NOISE (the flux trembling, the boiling) · SPREAD (partials detuned) | LEVEL · SPACE (the plate) · HALL (the send to the hall) |
| **ENV** | MODERATOR | TONE · FAULT (it breaks down into what lies under it) · DEPTH · DECAY | LEVEL · SPACE · HALL |
| **LFO** | LOOP | HUM · TURBINE · PUMPS · GEIGER | RELAYS · GROANS · GHOST · HORROR |
| **GLO** | HALL | TIME · GENERATIONS · GRAINS · SCATTER | the room: SIZE · DECAY · DAMP · MIX |

**The moderator** is a single deep voice under the hottest channel. Now and then it breaks down into its half, its third, its fifth, its seventh, more often with steam.

## Scenes (PRESETS)

| Scene | What it is |
|---|---|
| **HOT STANDBY** | the start: a core at low power, held by its regulator. The counts, a bed of channels, what your hands raise |
| **XENON PIT** | much xenon, slow, loosely coupled channels: what you play poisons itself, and the power wanders around the ring |
| **BOILING** | the steam works against you: the channels ring and boil, the margin is thin |
| **BLUE POOL** | a pool: long resonances, the bluest light, a hall that keeps only its own resonance |

Changing the scene sets the five pages and the room. The plant that was drawn, its sounds and their levels stay.

## The screen

- **The core's map**: the ring of sixteen channels in Cherenkov colour (dark blue to bluish white; red above full power). Channels that are nearly off blink with each count. A withdrawn rod lights its cell. In the middle, the regulator's rods.
- **The instruments**: N (the power over eight decades, a red mark at full power), PERIOD and SUR (the period in seconds and the startup rate in decades per minute), RHO in dollars (with the prompt critical mark), ORM (the operating margin, in rods), XE, VOID, and the regulator's position.
- **Twelve annunciators**: AR (the regulator at work), PER (short period), OVER (overpower), PRMT (a channel prompt critical), VOID, XENON, ORM (low margin), AZ-5, GRID (GRID LOST in a blackout), TURB, HOLD/GEN, LIMIT.
- **The button lights**: FX = the regulator at work; SEL, ENV, GLO = that source on; LFO = the grid holds; white keys = rods out; REC = AZ-5 or a protection trip.
- **HOME**: the board (below), then **the rods' wall**: sixteen rod dials in a circle around the bank's dial, the POWER, HOTTEST, RHO $ and ORM RODS counters, and a four-pen recorder (power, reactivity, heat, xenon).
- At the bottom: the version, the processor load (CPU, PEAK) and the output level.

## AZ-5 and the protections

- **Hold REC one second: AZ-5.** Every rod goes in over two seconds, graphite tips first.
- Normally, the power falls on its last period, the turbine slows, and the core recovers.
- **With VOID high and the margin (ORM) low**, the tips can take the core prompt critical: the blow, then the cut. It is not certain: it depends on the state of the core when you press.
- The core also protects itself: overpower (more than three times full power) or a period too short at high power trips it.

## GEN: the hand that pulls rods by itself

**SEQ** switches it on and off (at 50 % the first time). It pulls the rods of the channels nearest the notes of its figure, in your key. Your hands keep control of their own rods.

On HOME, page THE NOTES, row **GEN**: **KIND** is the figure, **VARIANT** its speed, **LEVEL** how much. Around 20 %: a figure now and then. Around 50 %: figure after figure. Above 60 %: the lowest channel is also held out, for a long time. At 100 %: almost no silence; with no hands, the reactor plays alone.

| Figure | What it does |
|---|---|
| STALKER | two notes a semitone apart, closing in as it heats |
| DESCENT | a slow line that keeps falling, and starts again higher |
| LULLABY | a nursery cell, repeated, its last note wrong; hot, it goes wrong more often |
| DIES IRAE | the opening of the medieval chant of the Day of Wrath |
| TRITONE | the root, then its tritone |
| CLUSTER | a semitone against each channel you hold |
| PULSE | one note in five beats, faster as it heats |

## Everything that is drawn: HOME

**HOME** opens the board where you left it; **HOME again** goes to its other page, then to the rods' wall, then back.

- **THE NOTES**: FUEL, MODERATOR, HALL, SHIFT, GEN.
- **THE PLANT**: on the left, what is always there; on the right, what the black keys strike.

| KNOB | What it does to the chosen row |
|---|---|
| **1 · SOUND** | chooses the row. Striking a black key also goes to its row |
| **2 · KIND** | the sound. One step right on a free sound **locks it as it is**; then right and left step through the kinds; left of the first: **FREE** |
| **3 · VARIANT** | its size, pitch or speed. For FUEL: **the core's loading** (each channel's enrichment, so which channels sound the most) |
| **4 · LEVEL** | its level |

A **locked** sound is shown in green: holding PLAY no longer changes it. A **free** sound is drawn again at each new plant. **Hold EDIT** on the board for the room (SIZE, DECAY, DAMP, MIX).

**FUEL**, the channels' voice:

| Fuel | What you hear |
|---|---|
| DETECTOR | the counts struck into each channel's harmonics |
| RODS | bowed metal: partials that are no series (like a waterphone) |
| GLASS | a struck bar: glass partials, high and far apart |
| MAINS | each note pulled onto the mains' harmonics (50 Hz) |

**SHIFT**: SINK (everything moves down), RISE (everything moves up), TEAR (one ear down, the other up), more as it heats. LEVEL sets how far.

**The plant** and the black keys:

| Row | Kinds | VARIANT |
|---|---|---|
| HUM | MAINS · SICK (harsher) · DEEP (an octave down) | the mains' pitch, 46 to 54 Hz |
| TURBINE | FALLING (an endless fall) · RISING (an endless rise) · TIDAL (up and back) | its register |
| PUMPS | HEART (a double beat) · TRIPLE · SLOW (one heavy beat) | their pitch |
| GEIGER | DRIPS · PULSE (a tick locked to the pumps) | its pitch |
| RELAYS | SMALL · HEAVY (big contactors) | their pitch |
| GROAN | BEND · WHALE (up and back) · CREAK (in jerks) · DEEP | its pitch |
| STEPS | STEEL · PIPES (blows in a pipe) · HEART (a dull beat) | their weight |
| DOOR | GREAT · HATCH · KNOCKS (three knocks) · PLATE (a cracked gong) | its size |
| BLACKOUT | SINGLE · DOUBLE (it drops in two steps) | the breaker's size |
| RISER | STRINGS · FALL · TREMOLO · ENDLESS | its length, 7 to 11 seconds |
| SWELL | BREATH · BELL (a reversed bell) · VOID (rises, cut on nothing) · SHUDDER | its length |
| DROP | FALL · BOMB (from high, ends on a blow) · FROM BELOW | its length |
| BRAAM | BRASS · TRITONE · GRIND (rubbing semitones) · HORN (swells slowly) | its brightness |
| STINGER | SHOCK · CLUSTER (a fist on a piano) · SCREECH (a falling cry) · ANVIL (struck iron) | its pitch |

They follow the reactor's heat. GHOST follows the hottest channel; GROAN and RISER start from the lowest channel whose rod is out. **HORROR**: how much the heat bends everything (the pitch sags and wavers).

The black keys, in their groups: **STEPS, DOOR, GROAN | RELAYS, BLACKOUT (on / off) | RISER (held), SWELL (held), DROP | BRAAM, STINGER | TRIP** (the turbine's). Hold a black key and turn **PRESETS** to change its kind without leaving the page.

## Another plant, and keeping it

- **Hold PLAY one second**: a new plant. Everything free is drawn again: the fuel and the core's loading, SHIFT's direction, GEN's figure, the plant's sounds and their VARIANT. A short press starts or stops the sound.
- **Hold SAVE one second**: everything is kept in the FM-1 (the locks, the VARIANTs, the levels, the five pages' knobs, the scene, the key). At the next start it all comes back, with every rod in, and what is free is drawn again.
- To get exactly the same plant at every start: lock every row (one step of KIND to the right), then hold SAVE.

## Six things to try

1. **The start-up.** PLAY, then wait thirty seconds without touching anything: the source, the counts, the bed forming.
2. **One rod.** Hold a key for ten seconds. Watch PERIOD and N, listen to the channel go from counts to tone. Let go and listen to the long tail.
3. **The redistribution.** Hold two keys far apart, then a third: the regulator takes power back from the others.
4. **The poison.** Scene XENON PIT, HOLD, pull two rods, wait a minute, put them back, and let it run for three minutes.
5. **The boiling.** FX, turn VOID past 60, play fast in the upper keys.
6. **The risk.** VOID high, HOLD, pull most of the rods (ORM low), then hold REC. Sometimes nothing happens. Sometimes it does.

## Good to know

- If the processor load goes over 85 %, the instrument lightens itself (fewer partials per channel, fewer grains in the hall, fewer strings in RISER).
- **SAVE writes a single flash sector** reserved for CRITICAL (0x77000), only when SAVE is held. Nothing else is written.
- The model follows real physics, simplified and sped up (TEMPO): it is an instrument, not a simulator. TACET Laboratories is fictional.

## Sources

Reactor physics and instrumentation:

- [DOE Fundamentals Handbook, Nuclear Physics and Reactor Theory, vol. 2](http://www.hss.doe.gov/nuclearsafety/techstds/docs/handbook/h1019v2.pdf): period, startup rate, reactivity, prompt criticality, xenon
- [Reactor Period](http://nuclearpowertraining.tpub.com/h1019v2/css/Reactor-Period-103.htm) · [Reactor Startup Rate](http://nuclearpowertraining.tpub.com/h1019v2/css/Reactor-Startup-Rate-Sur-109.htm) · [Prompt Criticality](http://nuclearpowertraining.tpub.com/h1019v2/css/Prompt-Criticality-107.htm) · [Xenon-135 Oscillations](http://nuclearpowertraining.tpub.com/h1019v2/css/Xenon-135-Oscillations-63.htm) · [Xenon-135 Response to Power Changes](http://nuclearpowertraining.tpub.com/h1019v2/css/Xenon-135-Response-To-Reactor-Power-Changes-64.htm)
- [Xenon-135 (nuclear-power.net)](http://www.nuclear-power.net/nuclear-power/reactor-physics/reactor-operation/xenon-135/) · [Xenon poisoning (HyperPhysics)](http://hyperphysics.phy-astr.gsu.edu/hbase/nucene/xenon.html)
- [Source range detectors](https://www.nuclear-power.com/nuclear-power-plant/nuclear-reactor/nuclear-instrumentation/excore-nuclear-instrumentation/source-range-detectors) · [Instrumentation and control (DOE)](https://www.tpub.com/doeinstrument/instrumentationandcontrol64.htm)
- [Risø, reactor noise report (RISØ-M-2437)](http://www.risoe.dk/rispubl/reports_INIS/RISOM2437.pdf)
- [Chernobyl disaster](https://en.wikipedia.org/wiki/Chernobyl_disaster): the RBMK, its positive void coefficient, the graphite tip effect, AZ-5, the operating margin
- [Ignalina Nuclear Power Plant](https://en.wikipedia.org/wiki/Ignalina_Nuclear_Power_Plant)

Sound and music drawn from power plants:

- [Hildur Guðnadóttir & Sam Slater, Sound On Sound](https://www.soundonsound.com/people/hildur-gudnadottir-sam-slater): the *Chernobyl* score, made from the sounds of the Ignalina plant
- [Scoring Chernobyl using the sounds of a power plant (Synthtopia)](https://www.synthtopia.com/content/2019/06/01/scoring-chernobyl-using-the-sounds-of-a-power-plant/)
- Jacob Kirkegaard, *4 Rooms* (rooms at Chernobyl, re-recorded in generations): [Musique Machine](https://www.musiquemachine.com/reviews/jacob-kirkegaard-4-rooms-773) · [Tiny Mix Tapes](https://www.tinymixtapes.com/music-review/jacob-kirkegaard-4-rooms)
- Alvin Lucier, *I Am Sitting in a Room*: the hall's generations
