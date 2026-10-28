#include "Registre.h"

#include "relais/PiloteRelais.h"

namespace argos {

namespace {

struct TypePilote {
  const char* nom;
  std::unique_ptr<Pilote> (*creer)();
};

template <typename T>
std::unique_ptr<Pilote> fabriquer() {
  return std::unique_ptr<Pilote>(new T());
}

const TypePilote kTypes[] = {
    {"relais", &fabriquer<PiloteRelais>},
};

}  // namespace

std::unique_ptr<Pilote> creerPilote(const String& type) {
  for (const TypePilote& t : kTypes) {
    if (type == t.nom) return t.creer();
  }
  return nullptr;
}

void listerTypesPilotes(JsonArray sortie) {
  for (const TypePilote& t : kTypes) sortie.add(t.nom);
}

}  // namespace argos
