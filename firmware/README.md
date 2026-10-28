# Firmware

Firmware unique pour tous les objets Argos, compilé avec [PlatformIO](https://platformio.org/). Le fonctionnement est décrit dans la [spécification](../docs/specification.md).

**État actuel (étape 1)** : noyau en mode secteur et pilote `relais`. Les modes batterie, la mise à jour à distance (OTA) et les autres pilotes arrivent dans les étapes suivantes.

## Compiler et flasher

| Environnement | Puce |
|---|---|
| `esp32` | ESP32 classique |
| `esp32c3` | ESP32-C3 |
| `esp32c6` | ESP32-C6 |

```bash
cd firmware
pio run -e esp32 -t upload     # compiler et flasher par USB
pio device monitor             # moniteur série (115 200 bauds)
```

Chaque push compile aussi les trois puces sur GitHub Actions. Les binaires sont téléchargeables dans l'onglet **Actions** du dépôt.

## Première installation

1. Flasher l'objet, puis le démarrer.
2. Sans paramètres réseau, il ouvre un point d'accès WiFi **`Argos-xxxxxx`** (les 6 derniers caractères de son adresse MAC).
3. S'y connecter avec un téléphone : la page de configuration s'ouvre (sinon, aller sur `http://192.168.4.1`).
4. Saisir le WiFi, l'adresse du broker MQTT et, si besoin, ses identifiants. L'objet redémarre et se connecte.

Pour rouvrir le portail plus tard, **maintenir le bouton BOOT pendant 3 secondes** au démarrage. Il se referme seul au bout de 5 minutes.

L'identifiant de l'objet (`argos-xxxxxx`) s'affiche sur le moniteur série au démarrage, et sert de racine à tous ses topics.

## Tester avec `mosquitto`

Dans les exemples, remplacer `argos-3fa21c` par l'identifiant de l'objet et `raspmqtt` par l'adresse du broker.

**Voir tout ce que publie l'objet :**

```bash
mosquitto_sub -h raspmqtt -t 'argos/argos-3fa21c/#' -v
```

Juste après la connexion, on doit voir `status` (`{"etat":"online"}`), `meta`, `config/appliquee` et `diag`.

**Configurer une carte de 8 relais** (les broches de l'ancienne carte Actionneurs, modules actifs à l'état bas) :

```bash
mosquitto_pub -h raspmqtt -r -t 'argos/argos-3fa21c/config' -m '{
  "version": 1,
  "nom": "Relais panneaux",
  "pilotes": [
    {
      "type": "relais",
      "instance": "relais",
      "params": {
        "broches": [16, 17, 18, 19, 21, 22, 23, 4],
        "actif_bas": true,
        "noms": ["Charge forcée"]
      }
    }
  ]
}'
```

L'objet répond sur `config/appliquee` (`{"version":1,"ok":true}`), puis publie une nouvelle description avec les capacités `relais/1` à `relais/8`, et leurs états sur `state/relais/1`, etc. Pour modifier la configuration, republier avec une **version supérieure**.

**Enclencher puis déclencher le relais 1 :**

```bash
mosquitto_pub -h raspmqtt -r -t 'argos/argos-3fa21c/set/relais/1' -m true
mosquitto_pub -h raspmqtt -r -t 'argos/argos-3fa21c/set/relais/1' -m false
```

L'option `-r` (message retenu) est voulue : après un redémarrage, l'objet retrouve l'état demandé.

**Commandes :**

```bash
mosquitto_pub -h raspmqtt -t 'argos/argos-3fa21c/cmd' -m diag         # publier les diagnostics maintenant
mosquitto_pub -h raspmqtt -t 'argos/argos-3fa21c/cmd' -m redemarrer   # redémarrer
```

**Vérifier la surveillance :** débrancher l'objet. Au bout d'environ 45 secondes, le broker publie `{"etat":"offline"}` sur `status` à sa place.

## Organisation du code

```
src/
├── main.cpp
├── noyau/
│   ├── Noyau        configuration, pilotes, contrat MQTT
│   ├── Pilote       interface commune à tous les pilotes
│   ├── Profil       broches et ports série de chaque puce
│   ├── Stockage     mémoire non volatile (NVS)
│   ├── Portail      page de première installation
│   └── Journal      messages série et dernière erreur
├── transport/
│   └── Transport    WiFi + MQTT non bloquant
└── pilotes/
    ├── Registre     liste des types de pilotes disponibles
    └── relais/      pilote relais
```

## Ajouter un type de pilote

1. Créer `src/pilotes/<type>/` avec une classe qui hérite de `argos::Pilote` (voir `noyau/Pilote.h`).
2. Implémenter au minimum `demarrer()` et `capacites()`, plus `ecrire()` pour une capacité commandable et `boucle()` pour un travail continu, **sans jamais bloquer**.
3. Ajouter le type dans la liste `kTypes` de `src/pilotes/Registre.cpp`.

Le noyau, la description publiée, le serveur et l'interface n'ont rien d'autre à changer.
