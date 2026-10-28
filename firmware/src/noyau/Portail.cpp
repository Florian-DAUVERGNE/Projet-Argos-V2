#include "Portail.h"

#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>

#include "Journal.h"

namespace argos {

namespace {

String echapper(const String& texte) {
  String sortie;
  sortie.reserve(texte.length());
  for (size_t i = 0; i < texte.length(); i++) {
    char c = texte[i];
    switch (c) {
      case '&': sortie += "&amp;"; break;
      case '<': sortie += "&lt;"; break;
      case '>': sortie += "&gt;"; break;
      case '"': sortie += "&quot;"; break;
      default: sortie += c;
    }
  }
  return sortie;
}

const char kEntete[] PROGMEM = R"(<!doctype html><html lang="fr"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>Argos</title>
<style>body{font-family:system-ui,sans-serif;max-width:28rem;margin:1.5rem auto;padding:0 1rem;color:#222}
h1{font-size:1.4rem}label{display:block;margin-top:.9rem;font-size:.9rem}
input{width:100%;box-sizing:border-box;padding:.55rem;font-size:1rem;border:1px solid #bbb;border-radius:6px}
button{margin-top:1.4rem;width:100%;padding:.7rem;font-size:1rem;border:0;border-radius:6px;background:#1d6fd6;color:#fff}
small{color:#666}.erreur{color:#b00020}</style></head><body>)";

String pageFormulaire(const String& id, const ConfigReseau& c, const String& reseaux, const String& erreur) {
  String page = FPSTR(kEntete);
  page += "<h1>Argos · " + echapper(id) + "</h1>";
  if (erreur.length() > 0) page += "<p class=\"erreur\">" + echapper(erreur) + "</p>";
  page += "<form method=\"post\" action=\"/enregistrer\">";
  page += "<label>Réseau WiFi<input name=\"ssid\" list=\"reseaux\" required value=\"" + echapper(c.ssid) + "\"></label>";
  page += "<datalist id=\"reseaux\">" + reseaux + "</datalist>";
  page += "<label>Mot de passe WiFi<input name=\"mdp\" type=\"password\"></label>";
  if (c.ssid.length() > 0) page += "<small>Laisser vide pour garder le mot de passe actuel du même réseau.</small>";
  page += "<label>Adresse du broker MQTT<input name=\"broker\" required value=\"" + echapper(c.brokerHote) + "\"></label>";
  page += "<label>Port<input name=\"port\" type=\"number\" min=\"1\" max=\"65535\" value=\"" + String(c.brokerPort) + "\"></label>";
  page += "<label>Utilisateur MQTT <small>(facultatif)</small><input name=\"mqtt_user\" value=\"" +
          echapper(c.mqttUtilisateur) + "\"></label>";
  page += "<label>Mot de passe MQTT <small>(facultatif)</small><input name=\"mqtt_mdp\" type=\"password\"></label>";
  if (c.mqttUtilisateur.length() > 0) page += "<small>Laisser vide pour garder le mot de passe actuel.</small>";
  page += "<button type=\"submit\">Enregistrer et redémarrer</button></form></body></html>";
  return page;
}

}  // namespace

void lancerPortail(const String& id, const ConfigReseau& actuel, uint32_t delaiMs) {
  String nomPointAcces = "Argos-" + id.substring(id.length() - 6);
  journalInfo("Portail ouvert : connectez-vous au WiFi « " + nomPointAcces + " »");

  // Recherche des réseaux avant d'ouvrir le point d'accès, pour les proposer.
  WiFi.mode(WIFI_STA);
  int trouves = WiFi.scanNetworks();
  String reseaux;
  for (int i = 0; i < trouves; i++) reseaux += "<option value=\"" + echapper(WiFi.SSID(i)) + "\">";
  WiFi.scanDelete();

  WiFi.mode(WIFI_AP);
  WiFi.softAP(nomPointAcces.c_str());
  delay(100);
  IPAddress ip = WiFi.softAPIP();

  DNSServer dns;
  dns.start(53, "*", ip);  // toute adresse mène au portail
  WebServer serveur(80);

  serveur.on("/", HTTP_GET, [&]() { serveur.send(200, "text/html", pageFormulaire(id, actuel, reseaux, "")); });

  serveur.on("/enregistrer", HTTP_POST, [&]() {
    ConfigReseau nouvelle;
    nouvelle.ssid = serveur.arg("ssid");
    nouvelle.ssid.trim();
    nouvelle.motDePasse = serveur.arg("mdp");
    if (nouvelle.motDePasse.length() == 0 && nouvelle.ssid == actuel.ssid) nouvelle.motDePasse = actuel.motDePasse;
    nouvelle.brokerHote = serveur.arg("broker");
    nouvelle.brokerHote.trim();
    long port = serveur.arg("port").toInt();
    nouvelle.brokerPort = (port >= 1 && port <= 65535) ? port : 1883;
    nouvelle.mqttUtilisateur = serveur.arg("mqtt_user");
    nouvelle.mqttUtilisateur.trim();
    nouvelle.mqttMotDePasse = serveur.arg("mqtt_mdp");
    if (nouvelle.mqttMotDePasse.length() == 0 && nouvelle.mqttUtilisateur == actuel.mqttUtilisateur) {
      nouvelle.mqttMotDePasse = actuel.mqttMotDePasse;
    }

    if (!nouvelle.complete()) {
      serveur.send(400, "text/html", pageFormulaire(id, nouvelle, reseaux, "Le réseau WiFi et le broker sont obligatoires."));
      return;
    }

    stockage::enregistrerReseau(nouvelle);
    String page = FPSTR(kEntete);
    page += "<h1>Enregistré</h1><p>L'objet redémarre et se connecte à « " + echapper(nouvelle.ssid) +
            " ». Vous pouvez fermer cette page.</p></body></html>";
    serveur.send(200, "text/html", page);
    journalInfo("Paramètres réseau enregistrés, redémarrage");
    delay(1500);
    ESP.restart();
  });

  // Les téléphones testent des adresses connues pour détecter un portail :
  // on les redirige toutes vers le formulaire.
  serveur.onNotFound([&]() {
    serveur.sendHeader("Location", "http://" + ip.toString() + "/", true);
    serveur.send(302, "text/plain", "");
  });

  serveur.begin();

  uint32_t debut = millis();
  while (true) {
    dns.processNextRequest();
    serveur.handleClient();
    if (delaiMs > 0 && millis() - debut > delaiMs) {
      journalInfo("Portail fermé (délai écoulé), redémarrage");
      ESP.restart();
    }
    delay(2);
  }
}

}  // namespace argos
