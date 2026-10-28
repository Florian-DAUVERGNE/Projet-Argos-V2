#include "Transport.h"

#include <WiFi.h>

#include <algorithm>

#include "../noyau/Journal.h"

namespace argos {

namespace {
constexpr size_t kTailleFile = 16;
constexpr size_t kTailleMaxMessage = 8192;
constexpr uint32_t kDelaiMaxEssai = 60000;
constexpr uint32_t kRelanceWifi = 30000;
constexpr uint32_t kDelaiConnexionMqtt = 20000;

bool atteint(uint32_t maintenant, uint32_t echeance) {
  return static_cast<int32_t>(maintenant - echeance) >= 0;
}
}  // namespace

void Transport::demarrer(const ConfigReseau& reseau, const String& clientId, const String& topicStatut,
                         const String& messageDerniereVolonte) {
  reseau_ = reseau;
  clientId_ = clientId;
  topicStatut_ = topicStatut;
  messageDerniereVolonte_ = messageDerniereVolonte;

  file_ = xQueueCreate(kTailleFile, sizeof(MessageRecu*));

  WiFi.mode(WIFI_STA);
  WiFi.setHostname(clientId_.c_str());
  WiFi.setAutoReconnect(true);
  WiFi.begin(reseau_.ssid.c_str(), reseau_.motDePasse.c_str());
  debutAttenteWifi_ = millis();
  journalInfo("WiFi : connexion à " + reseau_.ssid);

  mqtt_.setServer(reseau_.brokerHote.c_str(), reseau_.brokerPort);
  if (reseau_.mqttUtilisateur.length() > 0) {
    mqtt_.setCredentials(reseau_.mqttUtilisateur.c_str(), reseau_.mqttMotDePasse.c_str());
  }
  mqtt_.setClientId(clientId_.c_str());
  mqtt_.setKeepAlive(30);
  mqtt_.setCleanSession(true);
  mqtt_.setWill(topicStatut_.c_str(), 1, true, messageDerniereVolonte_.c_str());

  // Ces fonctions s'exécutent dans la tâche MQTT : elles ne font que signaler.
  mqtt_.onConnect([this](bool) { evenementConnexion_ = true; });
  mqtt_.onDisconnect([this](espMqttClientTypes::DisconnectReason) { evenementDeconnexion_ = true; });
  mqtt_.onMessage([this](const espMqttClientTypes::MessageProperties& proprietes, const char* topic,
                         const uint8_t* contenu, size_t longueur, size_t index, size_t total) {
    recevoir(topic, proprietes.retain, contenu, longueur, index, total);
  });
}

void Transport::boucle() {
  uint32_t maintenant = millis();
  surveillerWifi(maintenant);
  surveillerMqtt(maintenant);
}

void Transport::surveillerWifi(uint32_t maintenant) {
  bool ok = WiFi.status() == WL_CONNECTED;

  if (ok && !wifiConnecte_) {
    wifiConnecte_ = true;
    journalInfo("WiFi connecté, IP " + WiFi.localIP().toString());
  } else if (!ok && wifiConnecte_) {
    wifiConnecte_ = false;
    reconnexionsWifi_++;
    debutAttenteWifi_ = maintenant;
    journalErreur("WiFi perdu");
  }

  // La reconnexion automatique de l'ESP32 suffit en général. Si elle échoue
  // trop longtemps, on relance la connexion, sans jamais attendre.
  if (!ok && maintenant - debutAttenteWifi_ > kRelanceWifi) {
    journalInfo("WiFi : nouvelle tentative");
    WiFi.disconnect();
    WiFi.begin(reseau_.ssid.c_str(), reseau_.motDePasse.c_str());
    debutAttenteWifi_ = maintenant;
  }
}

void Transport::surveillerMqtt(uint32_t maintenant) {
  if (evenementConnexion_.exchange(false)) {
    connexionEnCours_ = false;
    delaiEssai_ = 1000;
    if (dejaConnecte_) reconnexionsMqtt_++;
    dejaConnecte_ = true;
    nouvelleConnexion_ = true;
    journalInfo("MQTT connecté à " + reseau_.brokerHote);
  }

  if (evenementDeconnexion_.exchange(false)) {
    connexionEnCours_ = false;
    prochainEssai_ = maintenant + delaiEssai_;
    journalErreur("MQTT déconnecté, nouvel essai dans " + String(delaiEssai_ / 1000) + " s");
    delaiEssai_ = std::min(delaiEssai_ * 2, kDelaiMaxEssai);
  }

  if (connexionEnCours_ && maintenant - debutConnexion_ > kDelaiConnexionMqtt) {
    // Aucune réponse : on abandonne cette tentative et on en replanifie une.
    mqtt_.disconnect(true);
    connexionEnCours_ = false;
    prochainEssai_ = maintenant + delaiEssai_;
    delaiEssai_ = std::min(delaiEssai_ * 2, kDelaiMaxEssai);
  }

  if (wifiConnecte_ && !mqtt_.connected() && !connexionEnCours_ && atteint(maintenant, prochainEssai_)) {
    connexionEnCours_ = true;
    debutConnexion_ = maintenant;
    if (!mqtt_.connect()) {
      connexionEnCours_ = false;
      prochainEssai_ = maintenant + delaiEssai_;
      delaiEssai_ = std::min(delaiEssai_ * 2, kDelaiMaxEssai);
    }
  }
}

bool Transport::vientDeSeConnecter() {
  bool resultat = nouvelleConnexion_;
  nouvelleConnexion_ = false;
  return resultat;
}

void Transport::recevoir(const char* topic, bool retenu, const uint8_t* contenu, size_t longueur, size_t index, size_t total) {
  if (total > kTailleMaxMessage) {
    if (index == 0) messagesPerdus_++;
    return;
  }
  if (index == 0) {
    morceauTopic_ = topic;
    morceauContenu_ = "";
    morceauContenu_.reserve(total);
  }
  morceauContenu_.concat(reinterpret_cast<const char*>(contenu), longueur);
  if (index + longueur < total) return;

  MessageRecu* message = new MessageRecu{morceauTopic_, morceauContenu_, retenu};
  if (xQueueSend(file_, &message, 0) != pdTRUE) {
    delete message;
    messagesPerdus_++;
  }
}

bool Transport::messageSuivant(MessageRecu& message) {
  MessageRecu* recu = nullptr;
  if (file_ == nullptr || xQueueReceive(file_, &recu, 0) != pdTRUE) return false;
  message = std::move(*recu);
  delete recu;
  return true;
}

bool Transport::publier(const String& topic, const String& contenu, bool retenu, uint8_t qos) {
  if (!mqtt_.connected()) return false;
  if (mqtt_.publish(topic.c_str(), qos, retenu, contenu.c_str()) == 0) {
    publicationsKo_++;
    return false;
  }
  return true;
}

bool Transport::abonner(const String& filtre, uint8_t qos) {
  return mqtt_.subscribe(filtre.c_str(), qos) != 0;
}

int Transport::rssi() const { return wifiConnecte_ ? WiFi.RSSI() : 0; }

}  // namespace argos
