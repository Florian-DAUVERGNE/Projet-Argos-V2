#pragma once

#include <Arduino.h>

#include "Stockage.h"

// Portail de première installation (spécification, section 8).
//
// L'objet ouvre un point d'accès WiFi « Argos-xxxxxx ». En s'y connectant avec
// un téléphone, une page s'ouvre pour saisir le WiFi et le broker MQTT.
// Après enregistrement, l'objet redémarre. Cette fonction ne revient jamais.

namespace argos {

[[noreturn]] void lancerPortail(const String& id, const ConfigReseau& actuel, uint32_t delaiMs);

}  // namespace argos
