// Point d'entrée du firmware Argos.
// Squelette de départ : le noyau (configuration, description, diagnostics,
// transport) et les pilotes arrivent à l'étape 1 de la feuille de route.

#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  Serial.printf("Argos %s sur %s\n", ARGOS_FIRMWARE_VERSION, ARGOS_PUCE);
}

void loop() {
}
