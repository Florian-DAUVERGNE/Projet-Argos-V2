#pragma once

#include <Arduino.h>

// Journal minimal : affiche sur le port série et retient la dernière erreur,
// publiée ensuite dans les diagnostics (spécification, section 10).

namespace argos {

void journalInfo(const String& message);
void journalErreur(const String& message);
const String& derniereErreur();

}  // namespace argos
