# Learning — Tessera

Espace d'apprentissage personnel de Maxime pour le projet Tessera.

L'objectif n'est pas seulement de livrer un plugin qui marche, mais de comprendre **pourquoi** chaque ligne de C++ et chaque choix d'archi est ce qu'il est.

## Contenu

| Fichier | Rôle |
|---|---|
| [`notes.md`](notes.md) | Concepts C++ et DSP rencontrés au fil du projet, expliqués pour un dev React/TS. Une entrée par concept. Ajouté automatiquement par l'agent `cpp-mentor`. |
| [`exercises.md`](exercises.md) | Exercices C++ progressifs, du basique vers le DSP. Solutions cachées dans `<details>`. À faire dans l'ordre. |
| [`progress.md`](progress.md) | Checklist de suivi : concepts vus, exercices faits, modules complétés, checkpoints validés. Vue d'ensemble de la progression. |

## Méthode

1. **Apprendre par le code du projet** — chaque module DSP est l'occasion d'introduire 1-3 concepts C++ nouveaux.
2. **Renforcer par les exercices** — `exercises.md` propose des petits problèmes ciblés pour chaque concept. Faits hors du projet pour ne pas polluer le code prod.
3. **Vérifier par les checkpoints** — après chaque module, `cpp-mentor` génère 3-5 questions. Réponse libre, correction inline.
4. **Tracer dans `progress.md`** — checklist mise à jour à chaque validation.

## Workflow type pour un nouveau module

```
1. Lecture rapide de la section concernée dans docs/architecture-engines.md
2. Identification des concepts C++ nouveaux qui vont apparaître (cpp-mentor)
3. Mini-explication inline pour chaque concept (chat ou notes.md)
4. (Optionnel) Exercice ciblé sur le concept si tu te sens flou
5. Écriture du module
6. Checkpoint quiz (cpp-mentor)
7. Update de progress.md
```

## Ressources externes

Référencées dans `notes.md` au fil de l'eau. Référence permanente : [cppreference.com](https://en.cppreference.com/w/cpp).
