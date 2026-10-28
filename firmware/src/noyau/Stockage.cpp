#include "Stockage.h"

#include <Preferences.h>

namespace argos {
namespace stockage {

namespace {
constexpr const char* kEspaceReseau = "argos-reseau";
constexpr const char* kEspaceObjet = "argos-objet";
}  // namespace

bool chargerReseau(ConfigReseau& reseau) {
  Preferences prefs;
  if (!prefs.begin(kEspaceReseau, true)) return false;
  reseau.ssid = prefs.getString("ssid", "");
  reseau.motDePasse = prefs.getString("mdp", "");
  reseau.brokerHote = prefs.getString("broker", "");
  reseau.brokerPort = prefs.getUShort("port", 1883);
  reseau.mqttUtilisateur = prefs.getString("mqtt_user", "");
  reseau.mqttMotDePasse = prefs.getString("mqtt_mdp", "");
  prefs.end();
  return reseau.complete();
}

void enregistrerReseau(const ConfigReseau& reseau) {
  Preferences prefs;
  prefs.begin(kEspaceReseau, false);
  prefs.putString("ssid", reseau.ssid);
  prefs.putString("mdp", reseau.motDePasse);
  prefs.putString("broker", reseau.brokerHote);
  prefs.putUShort("port", reseau.brokerPort);
  prefs.putString("mqtt_user", reseau.mqttUtilisateur);
  prefs.putString("mqtt_mdp", reseau.mqttMotDePasse);
  prefs.end();
}

String chargerConfiguration() {
  Preferences prefs;
  if (!prefs.begin(kEspaceObjet, true)) return "";
  String json = prefs.getString("config", "");
  prefs.end();
  return json;
}

void enregistrerConfiguration(const String& json) {
  Preferences prefs;
  prefs.begin(kEspaceObjet, false);
  prefs.putString("config", json);
  prefs.end();
}

uint32_t compterDemarrage() {
  Preferences prefs;
  prefs.begin(kEspaceObjet, false);
  uint32_t total = prefs.getUInt("demarrages", 0) + 1;
  prefs.putUInt("demarrages", total);
  prefs.end();
  return total;
}

}  // namespace stockage
}  // namespace argos
