# Firmware

Firmware unique pour tous les objets Argos, compilé avec [PlatformIO](https://platformio.org/).

| Environnement | Puce |
|---|---|
| `esp32` | ESP32 classique |
| `esp32c3` | ESP32-C3 |
| `esp32c6` | ESP32-C6 |

```bash
pio run -e esp32                     # compiler
pio run -e esp32 -t upload           # flasher par USB
pio device monitor                   # moniteur série
```

Organisation prévue (spécification, section 12) :

```
src/
├── main.cpp
├── noyau/       configuration, description, diagnostics, OTA
├── transport/   WiFi + MQTT non bloquant
├── cycle/       modes secteur et batterie
├── pilotes/     un dossier par type de pilote
└── profils/     profil de chaque carte (broches, ports série…)
```
