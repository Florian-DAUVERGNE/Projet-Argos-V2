#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

#include <functional>
#include <vector>

#include "Profil.h"

// Interface commune à tous les pilotes (spécification, section 12.1).
// Le noyau crée les pilotes à partir de la configuration, leur demande leurs
// capacités pour la description, et leur transmet les ordres reçus.

namespace argos {

// Une capacité telle que décrite dans `meta` (spécification, section 6).
// La clé est locale au pilote : le noyau la préfixe par le nom de l'instance.
struct Capacite {
  String cle;
  String nom;
  String type;   // sensor, binary, text, switch, action
  String unite;  // vide si sans objet
  int precision = -1;
};

// Ressources partagées entre pilotes : le profil de carte et les broches déjà
// prises, pour refuser une configuration où deux pilotes utilisent la même.
class Contexte {
 public:
  explicit Contexte(const ProfilCarte& profil) : profil(profil) {}

  const ProfilCarte& profil;

  bool reserverSortie(int broche, String& erreur);
  bool reserverEntree(int broche, String& erreur);

 private:
  bool reserver(int broche, String& erreur);
  std::vector<int> reservees_;
};

class Pilote {
 public:
  // Reçoit une clé locale et sa valeur déjà sérialisée en JSON.
  using Rapporteur = std::function<void(const String& cle, const String& valeurJson)>;

  virtual ~Pilote() = default;

  void initialiser(const String& instance, Rapporteur rapporteur) {
    instance_ = instance;
    rapporteur_ = std::move(rapporteur);
  }
  const String& instance() const { return instance_; }

  // Valide les paramètres et initialise le matériel. En cas d'échec, remplit
  // `erreur` et renvoie false ; le pilote ne doit alors rien laisser actif.
  virtual bool demarrer(JsonObjectConst params, Contexte& contexte, String& erreur) = 0;

  // Libère le matériel avant une reconfiguration.
  virtual void arreter() {}

  // Travail courant. Ne doit jamais bloquer.
  virtual void boucle() {}

  virtual void capacites(std::vector<Capacite>& liste) const = 0;

  // Applique un ordre sur une capacité `switch` ou `action`.
  virtual bool ecrire(const String& cle, JsonVariantConst valeur, String& erreur) {
    (void)cle;
    (void)valeur;
    erreur = "capacité en lecture seule";
    return false;
  }

  // Compteurs propres au pilote, publiés dans `diag`.
  virtual void diagnostics(JsonObject sortie) const { (void)sortie; }

 protected:
  void signaler(const String& cle, bool valeur) { envoyer(cle, valeur ? "true" : "false"); }
  void signaler(const String& cle, float valeur, int precision) { envoyer(cle, String(valeur, precision)); }
  void signalerTexte(const String& cle, const String& valeur) {
    JsonDocument doc;
    doc.set(valeur);
    String json;
    serializeJson(doc, json);
    envoyer(cle, json);
  }

 private:
  void envoyer(const String& cle, const String& json) {
    if (rapporteur_) rapporteur_(cle, json);
  }

  String instance_;
  Rapporteur rapporteur_;
};

// Interprète une valeur reçue comme booléen : true/false, 1/0, "on"/"off".
bool lireBooleen(JsonVariantConst valeur, bool& resultat);

}  // namespace argos
