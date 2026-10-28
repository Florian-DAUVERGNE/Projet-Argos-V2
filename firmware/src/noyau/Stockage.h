#pragma once

#include <Arduino.h>

// Mémoire non volatile (NVS) : paramètres réseau saisis à l'installation,
// configuration des pilotes reçue par MQTT, compteur de redémarrages.

namespace argos {

struct ConfigReseau {
  String ssid;
  String motDePasse;
  String brokerHote;
  uint16_t brokerPort = 1883;
  String mqttUtilisateur;
  String mqttMotDePasse;

  bool complete() const { return ssid.length() > 0 && brokerHote.length() > 0; }
};

namespace stockage {

bool chargerReseau(ConfigReseau& reseau);
void enregistrerReseau(const ConfigReseau& reseau);

String chargerConfiguration();
void enregistrerConfiguration(const String& json);

// Incrémente et renvoie le nombre total de démarrages.
uint32_t compterDemarrage();

}  // namespace stockage
}  // namespace argos
