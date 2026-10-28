#pragma once

#include <ArduinoJson.h>

#include <memory>

#include "../noyau/Pilote.h"

// Registre des types de pilotes compilés dans le firmware. Ajouter un type de
// pilote, c'est l'écrire puis l'ajouter à la liste de Registre.cpp.

namespace argos {

std::unique_ptr<Pilote> creerPilote(const String& type);
void listerTypesPilotes(JsonArray sortie);

}  // namespace argos
