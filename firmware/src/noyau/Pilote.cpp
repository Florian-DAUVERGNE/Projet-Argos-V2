#include "Pilote.h"

#include <algorithm>

namespace argos {

bool Contexte::reserver(int broche, String& erreur) {
  if (std::find(reservees_.begin(), reservees_.end(), broche) != reservees_.end()) {
    erreur = "broche " + String(broche) + " déjà utilisée par un autre pilote";
    return false;
  }
  reservees_.push_back(broche);
  return true;
}

bool Contexte::reserverSortie(int broche, String& erreur) {
  if (!profil.estSortie(broche)) {
    erreur = "broche " + String(broche) + " : sortie impossible sur " + profil.puce;
    return false;
  }
  return reserver(broche, erreur);
}

bool Contexte::reserverEntree(int broche, String& erreur) {
  if (!profil.estEntree(broche)) {
    erreur = "broche " + String(broche) + " : entrée impossible sur " + profil.puce;
    return false;
  }
  return reserver(broche, erreur);
}

bool lireBooleen(JsonVariantConst valeur, bool& resultat) {
  if (valeur.is<bool>()) {
    resultat = valeur.as<bool>();
    return true;
  }
  if (valeur.is<long>()) {
    resultat = valeur.as<long>() != 0;
    return true;
  }
  if (valeur.is<const char*>()) {
    String texte = valeur.as<const char*>();
    texte.toLowerCase();
    if (texte == "true" || texte == "on" || texte == "1") {
      resultat = true;
      return true;
    }
    if (texte == "false" || texte == "off" || texte == "0") {
      resultat = false;
      return true;
    }
  }
  return false;
}

}  // namespace argos
