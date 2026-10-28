// Point d'entrée du firmware Argos : tout le travail est fait par le noyau.

#include <Arduino.h>

#include "noyau/Noyau.h"

namespace {
argos::Noyau noyau;
}

void setup() { noyau.demarrer(); }

void loop() { noyau.boucle(); }
