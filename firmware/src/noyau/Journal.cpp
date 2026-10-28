#include "Journal.h"

namespace argos {

namespace {
String derniere;
}

void journalInfo(const String& message) {
  Serial.print("[argos] ");
  Serial.println(message);
}

void journalErreur(const String& message) {
  Serial.print("[argos] ERREUR ");
  Serial.println(message);
  derniere = message;
}

const String& derniereErreur() { return derniere; }

}  // namespace argos
