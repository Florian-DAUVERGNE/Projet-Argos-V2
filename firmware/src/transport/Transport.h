#pragma once

#include <Arduino.h>
#include <espMqttClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <atomic>

#include "../noyau/Stockage.h"

// Transport WiFi + MQTT non bloquant (spécification, section 9.1).
//
// Le client MQTT tourne dans sa propre tâche FreeRTOS : la connexion TCP et
// les échanges avec le broker n'interrompent jamais la boucle principale.
// Les messages reçus sont placés dans une file et lus par la boucle principale,
// qui reste ainsi le seul endroit où l'état de l'objet est modifié.

namespace argos {

struct MessageRecu {
  String topic;
  String contenu;
  bool retenu = false;  // message rejoué par le broker à l'abonnement
};

class Transport {
 public:
  void demarrer(const ConfigReseau& reseau, const String& clientId, const String& topicStatut,
                const String& messageDerniereVolonte);

  // À appeler à chaque tour de boucle : surveille le WiFi et relance MQTT.
  void boucle();

  bool connecte() const { return mqtt_.connected(); }

  // Vrai une seule fois après chaque connexion au broker.
  bool vientDeSeConnecter();

  bool messageSuivant(MessageRecu& message);

  bool publier(const String& topic, const String& contenu, bool retenu, uint8_t qos = 0);
  bool abonner(const String& filtre, uint8_t qos = 1);

  int rssi() const;
  uint32_t reconnexionsWifi() const { return reconnexionsWifi_; }
  uint32_t reconnexionsMqtt() const { return reconnexionsMqtt_; }
  uint32_t publicationsKo() const { return publicationsKo_; }
  uint32_t messagesPerdus() const { return messagesPerdus_; }

 private:
  void surveillerWifi(uint32_t maintenant);
  void surveillerMqtt(uint32_t maintenant);
  void recevoir(const char* topic, bool retenu, const uint8_t* contenu, size_t longueur, size_t index, size_t total);

  // espMqttClient garde des pointeurs vers ces chaînes : elles doivent vivre
  // aussi longtemps que le client.
  ConfigReseau reseau_;
  String clientId_;
  String topicStatut_;
  String messageDerniereVolonte_;

  espMqttClient mqtt_;
  QueueHandle_t file_ = nullptr;

  // Écrits par la tâche MQTT, lus par la boucle principale.
  std::atomic<bool> evenementConnexion_{false};
  std::atomic<bool> evenementDeconnexion_{false};
  std::atomic<uint32_t> messagesPerdus_{0};

  // Réassemblage d'un message reçu en plusieurs morceaux (tâche MQTT seulement).
  String morceauTopic_;
  String morceauContenu_;

  bool wifiConnecte_ = false;
  uint32_t debutAttenteWifi_ = 0;

  bool nouvelleConnexion_ = false;
  bool connexionEnCours_ = false;
  bool dejaConnecte_ = false;
  uint32_t debutConnexion_ = 0;
  uint32_t prochainEssai_ = 0;
  uint32_t delaiEssai_ = 1000;

  uint32_t reconnexionsWifi_ = 0;
  uint32_t reconnexionsMqtt_ = 0;
  uint32_t publicationsKo_ = 0;
};

}  // namespace argos
