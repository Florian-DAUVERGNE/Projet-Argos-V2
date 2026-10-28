#include "PiloteRelais.h"

namespace argos {

namespace {
constexpr size_t kMaxRelais = 16;
}

bool PiloteRelais::demarrer(JsonObjectConst params, Contexte& contexte, String& erreur) {
  JsonArrayConst broches = params["broches"];
  if (broches.isNull() || broches.size() == 0) {
    erreur = "paramètre « broches » manquant ou vide";
    return false;
  }
  if (broches.size() > kMaxRelais) {
    erreur = "au plus " + String(kMaxRelais) + " relais par pilote";
    return false;
  }

  actifBas_ = params["actif_bas"] | true;
  bool etatInitial = params["etat_initial"] | false;
  JsonArrayConst noms = params["noms"];

  std::vector<Relais> relais;
  for (size_t i = 0; i < broches.size(); i++) {
    if (!broches[i].is<int>()) {
      erreur = "« broches » ne doit contenir que des numéros";
      return false;
    }
    int broche = broches[i].as<int>();
    if (!contexte.reserverSortie(broche, erreur)) return false;

    String nom = noms[i] | "";
    if (nom.length() == 0) nom = "Relais " + String(i + 1);
    relais.push_back({static_cast<uint8_t>(broche), nom, etatInitial});
  }

  // Toutes les broches sont valides : on peut toucher au matériel.
  relais_ = std::move(relais);
  for (size_t i = 0; i < relais_.size(); i++) {
    pinMode(relais_[i].broche, OUTPUT);
    appliquer(i, etatInitial);
  }
  return true;
}

void PiloteRelais::arreter() {
  // Les broches gardent leur niveau. Après une reconfiguration, le noyau
  // réapplique aussitôt les états souhaités reçus par MQTT.
  relais_.clear();
}

void PiloteRelais::capacites(std::vector<Capacite>& liste) const {
  for (size_t i = 0; i < relais_.size(); i++) {
    Capacite c;
    c.cle = String(i + 1);
    c.nom = relais_[i].nom;
    c.type = "switch";
    liste.push_back(c);
  }
}

bool PiloteRelais::ecrire(const String& cle, JsonVariantConst valeur, String& erreur) {
  long numero = cle.toInt();
  if (numero < 1 || numero > static_cast<long>(relais_.size())) {
    erreur = "relais « " + cle + " » inconnu";
    return false;
  }
  bool enclencher;
  if (!lireBooleen(valeur, enclencher)) {
    erreur = "valeur attendue : true ou false";
    return false;
  }
  appliquer(numero - 1, enclencher);
  return true;
}

void PiloteRelais::appliquer(size_t index, bool enclencher) {
  Relais& r = relais_[index];
  bool niveauHaut = actifBas_ ? !enclencher : enclencher;
  digitalWrite(r.broche, niveauHaut ? HIGH : LOW);
  if (r.enclenche != enclencher) commutations_++;
  r.enclenche = enclencher;
  signaler(String(index + 1), enclencher);
}

void PiloteRelais::diagnostics(JsonObject sortie) const { sortie["commutations"] = commutations_; }

}  // namespace argos
