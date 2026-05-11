# Bootstrap Prompt pour Claude Code — Semaine 1

> **Comment utiliser** : copie-colle ce prompt à Claude Code dans un nouveau projet vide. Joins-y aussi `spec-vst.md` et `architecture-engines.md` en context.

---

## Le prompt

Tu vas m'aider à démarrer un plugin audio VST3 multi-FX glitch/granulaire avec JUCE 8 et C++20. Tu as accès à deux documents en context : `spec-vst.md` (vision produit + scope MVP + plan de phases) et `architecture-engines.md` (deep-dive technique des moteurs DSP).

### Mission de cette session : Semaine 1

Bootstrap complet du projet basé sur le template **Pamplejuce**, avec un VST3 vide qui se charge dans Reaper, plus le premier module DSP (`CaptureBuffer`) implémenté avec ses tests unitaires.

### Tâches précises, dans l'ordre

1. **Cloner Pamplejuce** depuis `https://github.com/sudara/pamplejuce` dans le répertoire courant (ou démarrer un nouveau repo Git basé dessus). Adapter :
   - Nom du projet → `Glitchwave` (placeholder, on changera)
   - Company name → `MaxLab` (placeholder)
   - Plugin code unique 4 chars → `Glwv`
   - Manufacturer code → `MxLb`
   - Bundle ID → `com.maxlab.glitchwave`
   - Formats à activer : `VST3 AU Standalone`
   - On ajoutera CLAP en semaine ultérieure

2. **Vérifier que le build fonctionne** :
   - `cmake -B build -G Ninja` (ou Xcode/MSVC selon OS)
   - `cmake --build build`
   - Le VST3 résultant doit se charger dans Reaper sans crash

3. **Créer la structure de répertoires** comme défini dans `spec-vst.md` §9 :
   ```
   source/
   ├── PluginProcessor.{h,cpp}
   ├── PluginEditor.{h,cpp}
   ├── dsp/
   │   ├── CaptureBuffer.{h,cpp}
   │   └── fx/
   │       └── IFxModule.h
   └── ui/  (vide pour l'instant)
   tests/
   └── dsp/
       └── CaptureBuffer_tests.cpp
   ```

4. **Implémenter `CaptureBuffer`** selon §1 de `architecture-engines.md` :
   - Ring buffer pré-alloué pour 4 bars à 96 kHz max stéréo
   - `writePos` en `std::atomic<int64_t>`
   - Méthodes : `prepare(sampleRate)`, `write(buffer)`, `readInterpolated(channel, samplePos)`, `freeze(bool)`, `getWritePos()`
   - Interpolation linéaire pour démarrer (Hermite plus tard)
   - Allocation **uniquement** dans `prepare()`, jamais dans `write()` ou `readInterpolated()`

5. **Écrire les tests Catch2 pour `CaptureBuffer`** :
   - Test : write puis readInterpolated à la même position retourne la même valeur (round-trip)
   - Test : wrap-around correct quand on dépasse la taille du buffer
   - Test : `freeze(true)` empêche bien `write` de modifier le buffer
   - Test : interpolation linéaire entre deux samples connus produit la bonne valeur fractionnaire
   - Test : pas d'allocation pendant `write` ou `readInterpolated` (utiliser un test allocator si Pamplejuce en fournit, sinon `JUCE_LEAK_DETECTOR` au minimum)

6. **Câbler `CaptureBuffer` dans `PluginProcessor::processBlock`** :
   - `captureBuffer.write(buffer)` au début
   - Pour cette première semaine, le buffer **passe ensuite directement en sortie** (pas de FX encore). On valide juste que la capture marche sans dégrader l'audio.

7. **Vérifier RT-safety** dans `processBlock` :
   - `juce::ScopedNoDenormals` au début
   - Aucun `new`, `malloc`, `std::vector::push_back` dans le path audio
   - Pas de `std::mutex`, pas de `std::string` avec allocation
   - Documente cette contrainte dans `/docs/RT_SAFETY_RULES.md`

8. **Setup CI minimal** :
   - GitHub Actions (Pamplejuce en a déjà un de base, l'adapter)
   - Build sur Mac + Windows + Linux
   - Run des tests Catch2 à chaque PR
   - On ajoutera pluginval en semaine 12

9. **Commit Git granulaires** :
   - `feat: bootstrap from pamplejuce template`
   - `chore: configure project metadata (name, ids, formats)`
   - `feat(dsp): add CaptureBuffer with ring buffer + interpolation`
   - `test(dsp): add unit tests for CaptureBuffer`
   - `feat(processor): wire CaptureBuffer in processBlock`
   - `docs: add RT_SAFETY_RULES.md`

### Contraintes non négociables

- **C++20** (concepts, ranges, `std::span` pour les buffers)
- **Pas d'exceptions** dans le path audio (compile flag à activer si Pamplejuce ne le fait pas déjà)
- **Aucune allocation** dans `processBlock` (ni JUCE, ni STL, ni custom)
- **Atomics seulement** pour synchronisation UI ↔ DSP, jamais de mutex
- **Tests d'abord** (TDD light) : écris le test avant l'implémentation
- **Code commenté en français** pour l'API publique des modules DSP (cohérence avec les docs)

### Ce que tu fais PAS cette semaine

Pas de FX (sauf un module `ThruFx` trivial à des fins de test), pas de sequencer, pas de granular, pas d'UI custom (l'UI auto-générée par `juce::GenericAudioProcessorEditor` suffit). On reste **strictement** sur le bootstrap + CaptureBuffer.

### Definition of Done de la semaine 1

- [ ] Le projet build sur Mac/Win/Linux (ou au moins ta plateforme actuelle)
- [ ] Le VST3 se charge dans Reaper sans crash et l'audio passe en bypass
- [ ] `CaptureBuffer` est implémenté, documenté, et a 5+ tests qui passent
- [ ] `processBlock` capture l'input et le repasse à l'output sans dégrader le signal (round-trip = identité)
- [ ] CI GitHub Actions passe (build + tests)
- [ ] Au moins 6 commits Git atomiques propres
- [ ] `/docs/RT_SAFETY_RULES.md` existe et liste les règles

### Quand tu es bloqué

Si quelque chose dans `architecture-engines.md` est ambigu, **demande-moi avant de coder** plutôt que d'inventer. Sur les choix de design (ex: granularité de l'interpolation, taille exacte du ring buffer), propose 2-3 options avec trade-offs et laisse-moi trancher.

À toi.

---

## Notes pour Maxime (post-bootstrap)

Quand Claude Code aura fini sa semaine 1, vérifie toi-même :

1. **Charge le VST3 dans Reaper et joue un kick dessus** : tu dois entendre le kick passer sans aucune dégradation. Si le son est altéré, c'est qu'il y a un bug d'interpolation ou de write.

2. **Lance les tests sur ta machine** : ils doivent tous passer. Si un test foire, ne demande PAS à Claude Code de le contourner — fais-lui débugger.

3. **Inspecte le `processBlock`** : s'il y a un seul `new`, `make_unique`, `push_back`, ou `std::string` dedans, c'est un bug de RT-safety. Renvoie ça en correction.

4. **Vérifie que `CaptureBuffer` ne se lock pas** : pas de `std::lock_guard`, pas de `juce::ScopedLock`, rien. Que des `std::atomic`.

Une fois ces 4 checks validés, tu peux passer au prompt semaine 2 (FX bank initial avec Stutter).
