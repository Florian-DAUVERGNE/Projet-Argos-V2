#include "Profil.h"

#include <algorithm>

namespace argos {

namespace {

// Les listes excluent les broches de la mémoire flash, de l'USB et du port
// série de debug. Les broches de démarrage (strapping) restent autorisées mais
// sont à éviter pour des relais : leur état au démarrage peut gêner le boot.

#if CONFIG_IDF_TARGET_ESP32
// ESP32 classique. 6 à 11 : flash. 1 et 3 : port série USB. 34 à 39 : entrées seules.
// Strapping : 0, 2, 5, 12, 15.
const ProfilCarte profil{
    "esp32",
    {2, 4, 5, 12, 13, 14, 15, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33},
    {34, 35, 36, 39},
    2,
    0,
};
#elif CONFIG_IDF_TARGET_ESP32C3
// ESP32-C3. 11 à 17 : flash. 18 et 19 : USB. 20 et 21 : port série de debug.
// Strapping : 2, 8, 9.
const ProfilCarte profil{
    "esp32c3",
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10},
    {},
    1,
    9,
};
#elif CONFIG_IDF_TARGET_ESP32C6
// ESP32-C6. 12 et 13 : USB. 16 et 17 : port série de debug. 24 à 30 : flash.
// Strapping : 8, 9, 15.
const ProfilCarte profil{
    "esp32c6",
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 14, 15, 18, 19, 20, 21, 22, 23},
    {},
    1,
    9,
};
#else
#error "Puce non supportée par Argos"
#endif

bool contient(const std::vector<uint8_t>& liste, int broche) {
  return std::find(liste.begin(), liste.end(), broche) != liste.end();
}

}  // namespace

bool ProfilCarte::estSortie(int broche) const { return contient(sorties, broche); }

bool ProfilCarte::estEntree(int broche) const {
  return contient(sorties, broche) || contient(entreesSeules, broche);
}

void ProfilCarte::decrire(JsonObject sortie) const {
  sortie["puce"] = puce;
  JsonArray s = sortie["sorties"].to<JsonArray>();
  for (uint8_t b : sorties) s.add(b);
  JsonArray e = sortie["entrees_seules"].to<JsonArray>();
  for (uint8_t b : entreesSeules) e.add(b);
  sortie["ports_serie"] = portsSerie;
}

const ProfilCarte& profilCarte() { return profil; }

}  // namespace argos
