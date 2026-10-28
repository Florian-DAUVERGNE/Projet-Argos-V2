#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "../transport/Transport.h"
#include "Pilote.h"
#include "Stockage.h"

// Noyau commun à tous les objets Argos : identité, configuration, pilotes,
// contrat MQTT (spécification, sections 4 à 10). Mode secteur uniquement pour
// cette première version.

namespace argos {

class Noyau {
 public:
  void demarrer();
  void boucle();

 private:
  struct Instance {
    String nom;
    String type;
    std::unique_ptr<Pilote> pilote;
  };

  // Configuration et pilotes
  void appliquerConfiguration(const String& texte, bool depuisReseau);
  bool construire(JsonObjectConst config, std::vector<Instance>& instances, std::vector<String>& erreurs);
  void arreterPilotes(std::vector<Instance>& instances);
  void reappliquerSouhaits();
  Pilote* trouver(const String& instance);
  String typeCapacite(const String& cleComplete);
  void noterEtat(const String& cleComplete, const String& valeurJson);

  // Messages reçus
  void surConnexion();
  void traiterMessage(const MessageRecu& message);
  void traiterOrdre(const String& cleComplete, const String& contenu, bool retenu);
  void traiterCommande(const String& contenu);

  // Publications
  void publierStatut(const char* etat);
  void publierDescription();
  void publierAccuse();
  void publierEtats(bool tous);
  void publierDiagnostics();
  void publierJson(const String& suffixe, JsonDocument& doc, bool retenu);

  String topic(const String& suffixe) const { return base_ + suffixe; }

  String id_;
  String base_;
  ConfigReseau reseau_;
  Transport transport_;

  std::vector<Instance> instances_;
  String configTexte_;
  long version_ = 0;
  long versionRefusee_ = -1;
  String nom_;
  uint32_t rappelMs_ = 60000;

  std::map<String, String> etats_;    // clé complète → valeur JSON
  std::set<String> aPublier_;         // états modifiés depuis la dernière publication
  std::map<String, String> souhaits_; // ordres `switch` reçus, réappliqués après reconfiguration

  long accuseVersion_ = 0;
  bool accuseOk_ = true;
  std::vector<String> accuseErreurs_;

  uint32_t demarrages_ = 0;
  uint32_t dernierRappel_ = 0;
  uint32_t dernierDiag_ = 0;
  bool redemarrageDemande_ = false;
  uint32_t redemarrageA_ = 0;
};

}  // namespace argos
