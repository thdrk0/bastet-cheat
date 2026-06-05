# Rapport TP - Triche sur Bastet

## Introduction

Le but du TP était de tricher dans le jeu Bastet de plusieurs manières, en partant d'une triche simple sur la sauvegarde, puis en allant vers une modification du score pendant l'exécution du jeu, et enfin vers une modification de la génération des blocs.

Le dépôt contient plusieurs preuves et outils utilisés pendant le TP :

- `trace.txt` : trace `strace` utilisée pour comprendre où Bastet sauvegarde les scores.
- `cheat.c` : première version simple de recherche d'un score en mémoire.
- `write_in_game.c` : version plus complète avec scan itératif de la mémoire du processus.
- `demo_best_score.cast` / `demo_best_score.gif` : démonstration du score élevé dans le menu des highscores.
- `modif_score_in_game.gif` : démonstration de la modification du score pendant une partie.
- `same_block_gen.gif` : démonstration de la génération truquée des blocs.
- `bastet/` : sources du jeu, utilisées pour comprendre la sauvegarde, le score et la génération des blocs.

## 1. Triche par sauvegarde du highscore

### Objectif

La première partie consistait à obtenir un score élevé dans le menu `View highscores`, sans réellement jouer une longue partie. La contrainte était de faire une partie courte avec un score inférieur ou égal à 300, puis de comprendre comment Bastet sauvegarde les scores.

### Recherche avec strace

J'ai lancé Bastet avec `strace` pour observer les appels système :

```bash
strace -f -o trace.txt bastet
```

Dans `trace.txt`, on voit les fichiers ouverts par le jeu :

```text
openat(AT_FDCWD, "/var/games/bastet.scores2", O_RDWR) = -1 EACCES
openat(AT_FDCWD, "/home/tomubt/.bastetscores", O_RDWR) = 4
openat(AT_FDCWD, "/home/tomubt/.bastetscores", O_WRONLY|O_CREAT|O_TRUNC, 0666) = 4
```

Bastet tente d'abord d'utiliser le fichier global `/var/games/bastet.scores2`. Comme mon utilisateur n'a pas les droits en écriture dessus, le jeu utilise le fichier local :

```text
/home/tomubt/.bastetscores
```

Cette observation est confirmée par le code source dans `bastet/Config.cpp` :

```cpp
const std::string LocalHighScoresFileName="/.bastetscores";
const std::string GlobalHighScoresFileName="/var/games/bastet.scores2";
```

La fonction `Config::GetHighScoresFileName()` essaie le fichier global, puis bascule vers `$HOME/.bastetscores` si le fichier global n'est pas accessible. Ensuite, le destructeur `Config::~Config()` réécrit les highscores dans ce fichier avec des entrées de la forme :

```text
Scorer0000 = nom
Score0000 = score
```

### Modification du fichier de sauvegarde

Après avoir fait une courte partie avec un score inférieur à 300, j'ai fermé le jeu afin qu'il écrive le fichier de sauvegarde. Ensuite, j'ai modifié le fichier `~/.bastetscores` pour remplacer un score par une valeur élevée, par exemple un million et quelques.

Le principe est simple : Bastet ne protège pas cryptographiquement ses scores. Il relit directement le fichier texte au lancement suivant. En modifiant une entrée `Score...` et l'entrée `Scorer...` associée, le menu `View highscores` affiche ensuite mon nom avec le score choisi.

### Preuve

La preuve vidéo est présente dans le dépôt sous forme d'enregistrement terminal :

```text
demo_best_score.cast
demo_best_score.gif
```

On y voit le lancement de Bastet, l'ouverture du menu `View highscores`, puis l'affichage du nom et du score modifié.

## 2. Modification du score pendant une partie

### Objectif

La deuxième partie consistait à modifier le score pendant que Bastet était en cours d'exécution. Pour cela, j'ai écrit un programme C qui ouvre la mémoire du processus cible avec `/proc/$PID/mem`.

La difficulté principale est que l'adresse du score change à chaque lancement à cause de l'ASLR et de l'allocation dynamique. Il faut donc trouver l'adresse à l'exécution.

### Obtention du PID

Une fois Bastet lancé dans un terminal, on peut récupérer son PID avec :

```bash
pidof bastet
```

ou :

```bash
pgrep -n bastet
```

Le PID est ensuite passé au programme de triche.

### Accès aux mappings mémoire

Le programme final est `write_in_game.c`. Il utilise deux fichiers du pseudo-système `/proc` :

- `/proc/$PID/maps` pour connaître les plages d'adresses virtuelles du processus.
- `/proc/$PID/mem` pour lire et écrire directement dans la mémoire du processus.

Le programme ouvre d'abord la mémoire :

```c
sprintf(path, "/proc/%d/mem", pid);
int mem = open(path, O_RDWR);
```

Comme cet accès est protégé, il faut généralement lancer le programme avec `sudo` :

```bash
gcc write_in_game.c -o write_in_game
sudo ./write_in_game <PID>
```

### Scan initial

Le programme demande le score actuellement affiché dans le jeu. Il refuse le score 0, car cette valeur est trop commune en mémoire et donnerait trop de faux positifs.

Ensuite, il parcourt les mappings mémoire accessibles en lecture/écriture :

```c
if (perms[0] == 'r' && perms[1] == 'w') {
    ...
}
```

Pour chaque zone mémoire, il lit des entiers avec `pread` :

```c
pread(mem, &value, sizeof(int), addr)
```

Si la valeur lue correspond au score affiché, l'adresse est ajoutée à la liste des candidats.

### Scan itératif

Un seul scan ne suffit pas toujours, car plusieurs adresses peuvent contenir la même valeur. Pour résoudre ce problème, le programme fonctionne comme Cheat Engine :

1. Je saisis le score actuel.
2. Le programme trouve toutes les adresses contenant cette valeur.
3. Je gagne encore quelques points dans Bastet.
4. Je saisis le nouveau score.
5. Le programme filtre les candidats en conservant uniquement les adresses qui ont évolué vers cette nouvelle valeur.

La boucle continue jusqu'à ce qu'il ne reste qu'une seule adresse :

```c
while (nb > 1) {
    printf("Nouveau score affiche : ");
    scanf("%d", &score);

    int new_nb = 0;

    for (int i = 0; i < nb; i++) {
        if (pread(mem, &value, sizeof(int), candidates[i]) == sizeof(int)) {
            if (value == score) {
                candidates[new_nb] = candidates[i];
                new_nb++;
            }
        }
    }

    nb = new_nb;
}
```

### Ecriture du nouveau score

Quand l'adresse unique est trouvée, le programme écrit un nouveau score avec `pwrite` :

```c
int new_score = 50000;
pwrite(mem, &new_score, sizeof(int), candidates[0]);
```

Le programme relit ensuite la valeur pour vérifier que l'écriture a bien fonctionné :

```c
pread(mem, &value, sizeof(int), candidates[0]);
printf("[OK] Nouvelle valeur lue = %d\n", value);
```

### Première version

Le fichier `cheat.c` correspond à une version plus simple. Il prend directement un ancien score en argument, scanne seulement le heap et écrit `50000` à la première adresse correspondante. Cette version fonctionne parfois, mais elle est moins robuste que `write_in_game.c` car elle ne gère pas les faux positifs avec plusieurs itérations.

### Preuve

La démonstration de cette partie est présente dans :

```text
modif_score_in_game.gif
```

On y voit le score évoluer pendant la partie après l'écriture dans la mémoire du processus.

## 3. Trucage de la génération des blocs

### Objectif

La troisième partie demandait de truquer la génération des blocs pour que Bastet donne toujours le même bloc. L'objectif était de pouvoir montrer plusieurs lignes faites d'affilée avec un score déjà élevé.

### Identification de la fonction

Bastet étant open-source, j'ai utilisé directement les sources du jeu pour identifier la génération des blocs.

Les types de blocs sont définis dans `bastet/Block.hpp` :

```cpp
enum BlockType{
  O=0,
  I=1,
  Z=2,
  T=3,
  J=4,
  S=5,
  L=6
};
```

La génération normale est dans `bastet/BastetBlockChooser.cpp`. Les fonctions importantes sont :

- `BastetBlockChooser::GetStartingQueue()`
- `BastetBlockChooser::GetNext(...)`
- `NoPreviewBlockChooser::GetStartingQueue()`
- `NoPreviewBlockChooser::GetNext(...)`

Par exemple, dans le code normal, la file de départ est générée avec du hasard :

```cpp
q.push_back(first);
q.push_back(BlockType(random()%nBlockTypes));
```

Puis les blocs suivants sont choisis par `GetNext(...)`, qui calcule le bloc le plus défavorable pour le joueur.

### Modification

Pour forcer toujours le même bloc, il suffit de remplacer les choix dynamiques par un bloc fixe. Par exemple, pour forcer le bloc `I` :

```cpp
q.push_back(I);
q.push_back(I);
return I;
```

L'idée est de modifier les fonctions de génération afin que le jeu ne puisse plus choisir un autre type de bloc. Avec uniquement des blocs identiques, il devient beaucoup plus simple de préparer le plateau et de faire plusieurs lignes d'affilée.

Cette méthode correspond à la partie "petit coup de pouce" du sujet : au lieu de chercher la fonction uniquement dans Ghidra/GDB, j'ai utilisé le code source pour trouver directement l'endroit à modifier dans le binaire ou dans les sources recompilées.

### Preuve

La preuve de génération truquée est présente dans :

```text
same_block_gen.gif
```

On y voit que le jeu produit toujours le même bloc, ce qui permet de faire des lignes de manière artificielle.

## 4. Amélioration du dépôt : autoplayer et pause

En plus des triches demandées, le dépôt a été amélioré pour rendre la démonstration plus simple : Bastet joue maintenant automatiquement.

Cette partie est principalement dans `bastet/Ui.cpp`.

### Choix automatique des coups

L'autoplayer cherche les positions possibles pour la pièce courante, simule le verrouillage de la pièce, puis évalue le plateau obtenu. La fonction principale d'évaluation est :

```cpp
static long AiEvaluateWell(const Well &w, int linesCleared)
```

Elle prend en compte plusieurs critères :

- le nombre de lignes supprimées ;
- la hauteur totale ;
- la hauteur maximale ;
- les trous ;
- les cases couvertes au-dessus des trous ;
- l'irrégularité du plateau ;
- les transitions de lignes et de colonnes ;
- les puits.

Le but est d'éviter les placements qui donnent un bon résultat immédiat mais rendent la suite de la partie difficile.

### Anticipation de la prochaine pièce

Le bot ne regarde pas seulement la pièce courante : quand la prochaine pièce est visible, il simule aussi le meilleur coup possible avec cette prochaine pièce. Cela rend les décisions plus stables.

La logique est dans :

```cpp
AiEvaluateAfterMove(...)
AiChooseMove(...)
DropBlock(...)
```

### Contrôles ajoutés

Des contrôles ont été ajoutés pendant l'autoplay :

- `p` : mettre en pause ;
- `SPACE` ou `ENTER` : reprendre ;
- `q` : abandonner la partie et revenir au menu ;
- `+` ou `=` : accélérer ;
- `-` ou `_` : ralentir.

La vitesse est affichée dans le panneau du score avec la ligne `Speed`.

## Conclusion

Le TP montre trois niveaux de triche différents :

1. Une triche simple par modification du fichier de sauvegarde `~/.bastetscores`.
2. Une triche plus avancée par écriture directe dans la mémoire du processus avec `/proc/$PID/mem`.
3. Une triche sur la logique du jeu, en modifiant la génération des blocs dans les sources de Bastet.

La première méthode attaque la persistance des scores. La deuxième attaque l'état du programme en mémoire. La troisième attaque directement les règles du jeu. Ensemble, elles montrent que si un jeu local ne protège ni ses fichiers de score, ni sa mémoire, ni sa logique de génération, il est très facile de produire une démonstration avec un score artificiellement élevé.

## Remarque Git

Dans ce dépôt, `bastet` est enregistré par Git comme un gitlink/submodule (`mode 160000`). Cela explique pourquoi `git status` ne montre pas directement les modifications de fichiers comme `bastet/Ui.cpp`.

Pour que Git suive les sources de `bastet/` comme des fichiers normaux, il faudrait convertir le gitlink en dossier suivi :

```bash
git rm --cached bastet
git add bastet
```

Cette opération ne supprime pas les fichiers du disque, elle change seulement la manière dont Git les indexe.
