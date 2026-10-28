#pragma once

#include "../../noyau/Pilote.h"

// Pilote `relais` : une capacité `switch` par broche de sortie.
//
// Paramètres :
//   broches       liste des broches, dans l'ordre des relais (obligatoire)
//   actif_bas     true si un relais s'enclenche quand la broche est à LOW,
//                 cas de la plupart des modules à optocoupleurs (défaut : true)
//   noms          noms affichés, dans le même ordre (facultatif)
//   etat_initial  état de tous les relais au démarrage (défaut : false)
//
// Les clés des capacités sont les numéros des relais, à partir de 1.

namespace argos {

class PiloteRelais : public Pilote {
 public:
  bool demarrer(JsonObjectConst params, Contexte& contexte, String& erreur) override;
  void arreter() override;
  void capacites(std::vector<Capacite>& liste) const override;
  bool ecrire(const String& cle, JsonVariantConst valeur, String& erreur) override;
  void diagnostics(JsonObject sortie) const override;

 private:
  struct Relais {
    uint8_t broche;
    String nom;
    bool enclenche;
  };

  void appliquer(size_t index, bool enclencher);

  std::vector<Relais> relais_;
  bool actifBas_ = true;
  uint32_t commutations_ = 0;
};

}  // namespace argos
