#include "Noyau.h"

#include <esp_mac.h>
#include <esp_system.h>
#include <esp_timer.h>

#include "../pilotes/Registre.h"
#include "Journal.h"
#include "Portail.h"
#include "Profil.h"

namespace argos {

namespace {

constexpr uint32_t kPeriodeDiagnostics = 60000;
constexpr uint32_t kRappelMinimal = 5000;
constexpr size_t kTailleMaxConfiguration = 3800;  // limite d'une chaîne en NVS
constexpr size_t kMessagesParTour = 8;

String identifiantMateriel() {
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  char id[16];
  snprintf(id, sizeof(id), "argos-%02x%02x%02x", mac[3], mac[4], mac[5]);
  return String(id);
}

const char* causeRedemarrage() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON: return "mise_sous_tension";
    case ESP_RST_SW: return "logiciel";
    case ESP_RST_PANIC: return "plantage";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT: return "chien_de_garde";
    case ESP_RST_BROWNOUT: return "baisse_de_tension";
    case ESP_RST_DEEPSLEEP: return "deep_sleep";
    case ESP_RST_EXT: return "reset_externe";
    default: return "inconnue";
  }
}

// Le portail s'ouvre si l'on maintient le bouton BOOT pendant 3 s au démarrage.
bool boutonBootMaintenu() {
  uint8_t broche = profilCarte().brocheBoot;
  pinMode(broche, INPUT_PULLUP);
  if (digitalRead(broche) != LOW) return false;
  journalInfo("Bouton BOOT appuyé : maintenir 3 s pour ouvrir le portail");
  uint32_t debut = millis();
  while (millis() - debut < 3000) {
    if (digitalRead(broche) != LOW) return false;
    delay(20);
  }
  return true;
}

bool nomInstanceValide(const String& nom) {
  if (nom.length() == 0 || nom.length() > 24) return false;
  for (size_t i = 0; i < nom.length(); i++) {
    char c = nom[i];
    bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
    if (!ok) return false;
  }
  return true;
}

// Les ordres arrivent en JSON (true, 1, "on"…) ; un texte brut est accepté aussi.
void lireValeur(const String& contenu, JsonDocument& doc) {
  if (deserializeJson(doc, contenu) != DeserializationError::Ok) doc.set(contenu);
}

}  // namespace

// ---------------------------------------------------------------------------
// Démarrage et boucle
// ---------------------------------------------------------------------------

void Noyau::demarrer() {
  Serial.begin(115200);
  delay(200);

  id_ = identifiantMateriel();
  base_ = "argos/" + id_ + "/";
  journalInfo(String("Argos ") + ARGOS_FIRMWARE_VERSION + " sur " + ARGOS_PUCE + ", identifiant " + id_);

  demarrages_ = stockage::compterDemarrage();

  // Les pilotes démarrent avant le réseau : l'objet fonctionne même hors ligne.
  String config = stockage::chargerConfiguration();
  if (config.length() > 0) appliquerConfiguration(config, false);

  bool portailDemande = boutonBootMaintenu();
  bool reseauConnu = stockage::chargerReseau(reseau_);
  if (portailDemande || !reseauConnu) {
    // Sans réseau connu, le portail reste ouvert ; sinon il se referme après 5 min.
    lancerPortail(id_, reseau_, reseauConnu ? 5 * 60 * 1000 : 0);
  }

  transport_.demarrer(reseau_, id_, topic("status"), "{\"etat\":\"offline\"}");
}

void Noyau::boucle() {
  transport_.boucle();

  if (transport_.vientDeSeConnecter()) surConnexion();

  MessageRecu message;
  for (size_t i = 0; i < kMessagesParTour && transport_.messageSuivant(message); i++) {
    traiterMessage(message);
  }

  for (Instance& instance : instances_) instance.pilote->boucle();

  uint32_t maintenant = millis();
  if (transport_.connecte()) {
    if (!aPublier_.empty()) publierEtats(false);
    if (maintenant - dernierRappel_ >= rappelMs_) {
      publierEtats(true);
      dernierRappel_ = maintenant;
    }
    if (maintenant - dernierDiag_ >= kPeriodeDiagnostics) {
      publierDiagnostics();
      dernierDiag_ = maintenant;
    }
  }

  if (redemarrageDemande_ && static_cast<int32_t>(maintenant - redemarrageA_) >= 0) ESP.restart();
}

// ---------------------------------------------------------------------------
// Configuration et pilotes
// ---------------------------------------------------------------------------

void Noyau::appliquerConfiguration(const String& texte, bool depuisReseau) {
  JsonDocument doc;
  DeserializationError erreurJson = deserializeJson(doc, texte);
  long version = doc["version"] | 0L;

  if (depuisReseau && !erreurJson && version <= version_) {
    // Message retenu déjà appliqué (cas normal à chaque reconnexion).
    return;
  }

  std::vector<String> erreurs;
  if (erreurJson || !doc.is<JsonObject>()) {
    erreurs.push_back(String("configuration illisible : ") + erreurJson.c_str());
  } else if (texte.length() > kTailleMaxConfiguration) {
    erreurs.push_back("configuration trop longue (" + String(texte.length()) + " octets, maximum " +
                      String(kTailleMaxConfiguration) + ")");
  } else if (String(doc["alimentation"]["mode"] | "secteur") != "secteur") {
    erreurs.push_back("seul le mode « secteur » est disponible dans cette version");
  }

  if (!erreurs.empty()) {
    accuseVersion_ = version;
    accuseOk_ = false;
    accuseErreurs_ = erreurs;
    for (const String& e : erreurs) journalErreur("configuration v" + String(version) + " : " + e);
    publierAccuse();
    return;
  }

  std::set<String> anciennesCles;
  for (const auto& etat : etats_) anciennesCles.insert(etat.first);

  arreterPilotes(instances_);
  etats_.clear();
  aPublier_.clear();

  std::vector<Instance> nouvelles;
  if (!construire(doc.as<JsonObjectConst>(), nouvelles, erreurs)) {
    // Échec : on remet en place la configuration précédente, qui fonctionnait.
    arreterPilotes(nouvelles);
    etats_.clear();
    aPublier_.clear();
    if (configTexte_.length() > 0) {
      JsonDocument ancienne;
      deserializeJson(ancienne, configTexte_);
      std::vector<String> ignorees;
      construire(ancienne.as<JsonObjectConst>(), instances_, ignorees);
    }
    reappliquerSouhaits();

    accuseVersion_ = version;
    accuseOk_ = false;
    accuseErreurs_ = erreurs;
    for (const String& e : erreurs) journalErreur("configuration v" + String(version) + " : " + e);
    publierAccuse();
    publierEtats(true);
    return;
  }

  instances_ = std::move(nouvelles);
  configTexte_ = texte;
  version_ = version;
  nom_ = doc["nom"] | "";
  uint32_t rappel = (doc["alimentation"]["rappel_s"] | 60UL) * 1000UL;
  rappelMs_ = rappel < kRappelMinimal ? kRappelMinimal : rappel;

  if (depuisReseau) stockage::enregistrerConfiguration(texte);
  reappliquerSouhaits();

  // Une capacité qui disparaît ne doit pas laisser de valeur retenue sur le broker.
  for (const String& cle : anciennesCles) {
    if (etats_.find(cle) == etats_.end()) transport_.publier(topic("state/" + cle), "", true, 1);
  }

  accuseVersion_ = version;
  accuseOk_ = true;
  accuseErreurs_.clear();
  journalInfo("configuration v" + String(version) + " appliquée (" + String(instances_.size()) + " pilote(s))");

  publierAccuse();
  publierDescription();
  publierEtats(true);
}

bool Noyau::construire(JsonObjectConst config, std::vector<Instance>& instances, std::vector<String>& erreurs) {
  Contexte contexte(profilCarte());
  std::set<String> noms;

  JsonArrayConst pilotes = config["pilotes"];
  for (JsonObjectConst p : pilotes) {
    String type = p["type"] | "";
    String nom = p["instance"] | "";

    if (!nomInstanceValide(nom)) {
      erreurs.push_back("instance « " + nom + " » : nom invalide (a-z, 0-9, - et _, 24 caractères au plus)");
      continue;
    }
    if (!noms.insert(nom).second) {
      erreurs.push_back("instance « " + nom + " » : nom déjà utilisé");
      continue;
    }

    std::unique_ptr<Pilote> pilote = creerPilote(type);
    if (!pilote) {
      erreurs.push_back("instance « " + nom + " » : type de pilote « " + type + " » inconnu");
      continue;
    }

    pilote->initialiser(nom, [this, nom](const String& cle, const String& valeurJson) {
      noterEtat(nom + "/" + cle, valeurJson);
    });

    String erreur;
    if (!pilote->demarrer(p["params"], contexte, erreur)) {
      erreurs.push_back("instance « " + nom + " » : " + erreur);
      continue;
    }
    instances.push_back({nom, type, std::move(pilote)});
  }

  return erreurs.empty();
}

void Noyau::arreterPilotes(std::vector<Instance>& instances) {
  for (Instance& instance : instances) instance.pilote->arreter();
  instances.clear();
}

void Noyau::reappliquerSouhaits() {
  for (const auto& souhait : souhaits_) {
    int separateur = souhait.first.indexOf('/');
    Pilote* pilote = trouver(souhait.first.substring(0, separateur));
    if (pilote == nullptr) continue;
    JsonDocument valeur;
    lireValeur(souhait.second, valeur);
    String erreur;
    pilote->ecrire(souhait.first.substring(separateur + 1), valeur.as<JsonVariantConst>(), erreur);
  }
}

Pilote* Noyau::trouver(const String& instance) {
  for (Instance& i : instances_) {
    if (i.nom == instance) return i.pilote.get();
  }
  return nullptr;
}

String Noyau::typeCapacite(const String& cleComplete) {
  int separateur = cleComplete.indexOf('/');
  Pilote* pilote = trouver(cleComplete.substring(0, separateur));
  if (pilote == nullptr) return "";
  String locale = cleComplete.substring(separateur + 1);
  std::vector<Capacite> liste;
  pilote->capacites(liste);
  for (const Capacite& c : liste) {
    if (c.cle == locale) return c.type;
  }
  return "";
}

void Noyau::noterEtat(const String& cleComplete, const String& valeurJson) {
  auto existant = etats_.find(cleComplete);
  if (existant != etats_.end() && existant->second == valeurJson) return;
  etats_[cleComplete] = valeurJson;
  aPublier_.insert(cleComplete);
}

// ---------------------------------------------------------------------------
// Messages reçus
// ---------------------------------------------------------------------------

void Noyau::surConnexion() {
  transport_.abonner(topic("config"), 1);
  transport_.abonner(topic("set/#"), 1);
  transport_.abonner(topic("cmd"), 1);

  publierStatut("online");
  publierDescription();
  publierAccuse();
  publierEtats(true);
  publierDiagnostics();

  dernierRappel_ = millis();
  dernierDiag_ = dernierRappel_;
}

void Noyau::traiterMessage(const MessageRecu& message) {
  if (!message.topic.startsWith(base_)) return;
  String suffixe = message.topic.substring(base_.length());

  if (suffixe == "config") {
    if (message.contenu.length() > 0) appliquerConfiguration(message.contenu, true);
  } else if (suffixe.startsWith("set/")) {
    traiterOrdre(suffixe.substring(4), message.contenu, message.retenu);
  } else if (suffixe == "cmd") {
    traiterCommande(message.contenu);
  }
}

void Noyau::traiterOrdre(const String& cleComplete, const String& contenu, bool retenu) {
  if (contenu.length() == 0) {
    // Message retenu effacé côté serveur : plus d'état souhaité pour cette capacité.
    souhaits_.erase(cleComplete);
    return;
  }

  int separateur = cleComplete.indexOf('/');
  Pilote* pilote = separateur > 0 ? trouver(cleComplete.substring(0, separateur)) : nullptr;
  if (pilote == nullptr) {
    journalErreur("ordre pour une capacité inconnue : " + cleComplete);
    return;
  }

  String type = typeCapacite(cleComplete);
  if (type == "action" && retenu) {
    // Une impulsion ne doit jamais être rejouée (spécification, section 5.2).
    journalErreur("ordre retenu ignoré pour l'action " + cleComplete);
    return;
  }

  JsonDocument valeur;
  lireValeur(contenu, valeur);
  String erreur;
  if (!pilote->ecrire(cleComplete.substring(separateur + 1), valeur.as<JsonVariantConst>(), erreur)) {
    journalErreur(cleComplete + " : " + erreur);
    return;
  }
  if (type == "switch") souhaits_[cleComplete] = contenu;
}

void Noyau::traiterCommande(const String& contenu) {
  String commande = contenu;
  commande.trim();
  commande.replace("\"", "");

  if (commande == "redemarrer") {
    journalInfo("redémarrage demandé");
    publierStatut("offline");
    redemarrageDemande_ = true;
    redemarrageA_ = millis() + 1000;
  } else if (commande == "diag") {
    publierDiagnostics();
  } else {
    journalErreur("commande inconnue : " + commande);
  }
}

// ---------------------------------------------------------------------------
// Publications
// ---------------------------------------------------------------------------

void Noyau::publierJson(const String& suffixe, JsonDocument& doc, bool retenu) {
  String texte;
  serializeJson(doc, texte);
  transport_.publier(topic(suffixe), texte, retenu, 1);
}

void Noyau::publierStatut(const char* etat) {
  JsonDocument doc;
  doc["etat"] = etat;
  publierJson("status", doc, true);
}

void Noyau::publierDescription() {
  JsonDocument doc;
  doc["id"] = id_;
  doc["nom"] = nom_;
  doc["config_version"] = version_;

  JsonObject firmware = doc["firmware"].to<JsonObject>();
  firmware["version"] = ARGOS_FIRMWARE_VERSION;
  firmware["puce"] = ARGOS_PUCE;

  doc["alimentation"]["mode"] = "secteur";
  profilCarte().decrire(doc["carte"].to<JsonObject>());
  listerTypesPilotes(doc["pilotes_disponibles"].to<JsonArray>());

  JsonArray capacites = doc["capacites"].to<JsonArray>();
  for (const Instance& instance : instances_) {
    std::vector<Capacite> liste;
    instance.pilote->capacites(liste);
    for (const Capacite& c : liste) {
      JsonObject o = capacites.add<JsonObject>();
      o["cle"] = instance.nom + "/" + c.cle;
      o["nom"] = c.nom;
      o["type"] = c.type;
      if (c.unite.length() > 0) o["unite"] = c.unite;
      if (c.precision >= 0) o["precision"] = c.precision;
    }
  }

  publierJson("meta", doc, true);
}

void Noyau::publierAccuse() {
  JsonDocument doc;
  doc["version"] = accuseVersion_;
  doc["ok"] = accuseOk_;
  if (!accuseErreurs_.empty()) {
    JsonArray erreurs = doc["erreurs"].to<JsonArray>();
    for (const String& e : accuseErreurs_) erreurs.add(e);
  }
  publierJson("config/appliquee", doc, true);
}

void Noyau::publierEtats(bool tous) {
  if (!transport_.connecte()) return;
  if (tous) {
    for (const auto& etat : etats_) transport_.publier(topic("state/" + etat.first), etat.second, true);
    aPublier_.clear();
    return;
  }
  for (auto it = aPublier_.begin(); it != aPublier_.end();) {
    auto etat = etats_.find(*it);
    if (etat == etats_.end() || transport_.publier(topic("state/" + *it), etat->second, true)) {
      it = aPublier_.erase(it);
    } else {
      ++it;
    }
  }
}

void Noyau::publierDiagnostics() {
  JsonDocument doc;
  doc["uptime_s"] = static_cast<uint32_t>(esp_timer_get_time() / 1000000);
  doc["redemarrages"] = demarrages_ > 0 ? demarrages_ - 1 : 0;
  doc["cause_dernier_redemarrage"] = causeRedemarrage();
  doc["wifi_rssi"] = transport_.rssi();
  doc["reconnexions_wifi"] = transport_.reconnexionsWifi();
  doc["reconnexions_mqtt"] = transport_.reconnexionsMqtt();
  doc["publications_ko"] = transport_.publicationsKo();
  doc["messages_perdus"] = transport_.messagesPerdus();
  doc["memoire_libre"] = ESP.getFreeHeap();
  doc["config_version"] = version_;
  if (derniereErreur().length() > 0) doc["derniere_erreur"] = derniereErreur();

  JsonObject pilotes = doc["pilotes"].to<JsonObject>();
  for (const Instance& instance : instances_) {
    instance.pilote->diagnostics(pilotes[instance.nom].to<JsonObject>());
  }

  publierJson("diag", doc, true);
}

}  // namespace argos
