#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

#include <vector>

// Profil de carte (spécification, section 12.3) : ce que la puce permet.
// Il est compilé dans chaque binaire et publié dans la description, pour que
// l'interface ne propose que des choix valides.

namespace argos {

struct ProfilCarte {
  const char* puce;
  std::vector<uint8_t> sorties;        // broches utilisables en sortie
  std::vector<uint8_t> entreesSeules;  // broches utilisables en entrée uniquement
  uint8_t portsSerie;                  // ports série libres pour les pilotes
  uint8_t brocheBoot;                  // bouton BOOT, lu au démarrage

  bool estSortie(int broche) const;
  bool estEntree(int broche) const;
  void decrire(JsonObject sortie) const;
};

const ProfilCarte& profilCarte();

}  // namespace argos
