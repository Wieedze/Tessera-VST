# Design System — Multi-FX VST

> Compagnon de `spec-vst.md`. Définit l'identité visuelle, les tokens, les composants et les layouts. À filer à Claude Code en parallèle des autres specs.

---

## 1. Direction esthétique

**Industriel scientifique sombre**. Pense instrument de laboratoire haut de gamme, console de mixage Solid State Logic, calibration Apple Pro Display, interfaces Blade Runner *2049*.

### Ce qu'on prend des références

| Plugin | Ce qu'on garde |
|---|---|
| **Pigments** | Code couleur par moteur/section, halos de modulation pulsants, breathing room généreux |
| **Vital** | Drag-to-modulate visuel (faire glisser une source sur un knob), glow subtil sur les éléments actifs |
| **Serum** | Densité d'info contrôlée, visualisations centrales (waveform, spectrum), valeurs mono lisibles |

### Ce qu'on évite

- Le **cyberpunk neon générique** (cyans flashy partout, glow over-the-top)
- Le **flat design pseudo-Apple** (trop tiède pour un plugin de glitch)
- Le **skeuomorphisme bois/métal** (Arturia V Collection style — daté)
- Les **dégradés purple sur fond blanc** (cliché AI, vu et revu)
- La **typo Inter** par défaut (générique, on prend IBM Plex à la place)

---

## 2. Color tokens

### Palette de base

```
/* Fonds (du plus sombre au plus clair) */
--bg-void:        #0A0B0F;  /* fond extérieur du plugin */
--bg-surface-1:   #14161D;  /* zones principales */
--bg-surface-2:   #1C1F28;  /* cards, panels */
--bg-surface-3:   #252834;  /* éléments élevés (knobs centrés) */
--bg-hover:       #2F3340;  /* hover state */

/* Bordures */
--border-subtle:  #1F2230;  /* séparateurs faibles */
--border-default: #2F3340;  /* bordures standard */
--border-strong:  #3D4254;  /* hover/focus */

/* Texte */
--text-primary:   #E8EAF0;  /* titres, valeurs */
--text-secondary: #8B92A8;  /* labels */
--text-tertiary:  #5A6075;  /* hints, unités */
--text-disabled:  #3D4254;
```

### Couleurs sémantiques (signal flow)

```
--signal-primary: #00D9C4;  /* signal principal (input/output, dry/wet) */
--signal-active:  #00FFD9;  /* flash sur trigger step */
--mod-purple:     #B084EB;  /* TOUTES les modulations (sources + halos) */
--mod-purple-dim: #6B4F8F;  /* mod inactive */
```

### Couleurs par FX (color-coding Pigments-style)

Chaque FX a sa couleur identitaire. Quand un FX est actif sur un step, le step prend cette couleur. Le panel de paramètres du FX prend cette couleur en accent.

```
--fx-stutter:     #00D9C4;  /* cyan */
--fx-reverser:    #FF6B9D;  /* pink */
--fx-tapestop:    #FFA94D;  /* orange */
--fx-filter:      #FFE066;  /* yellow */
--fx-bitcrusher:  #FF5252;  /* red */
--fx-gater:       #51CF66;  /* green */
--fx-pitch:       #FF8FA3;  /* rose */
--fx-granular:    #DDA0DD;  /* lavender */
--fx-slicer:      #4DABF7;  /* blue */
--fx-thru:        #5A6075;  /* gris (= bypass) */
```

**Règle d'or** : ces couleurs apparaissent **uniquement** sur les FX et leurs steps. Le reste de l'UI est en gris/cyan. Sinon ça devient un sapin de Noël.

### Variantes pour fonds colorés (8% opacity)

Pour les step cells actifs, fonds de panneau FX, etc., utiliser la couleur FX à **8-12% d'opacité** sur fond `--bg-surface-2`. Le contour reste à pleine opacité.

---

## 3. Typographie

### Familles

```css
--font-sans: 'IBM Plex Sans', -apple-system, sans-serif;
--font-mono: 'IBM Plex Mono', 'JetBrains Mono', monospace;
```

**Pourquoi IBM Plex** : open source (OFL), distinctif sans être hipster, look "instrument scientifique" qui matche notre direction. Embeddable dans BinaryData JUCE sans soucis de licence.

### Échelle

| Usage | Taille | Weight | Famille | Letter spacing |
|---|---|---|---|---|
| Plugin title (header) | 14px | 500 | Sans | 0.5px |
| Section label | 11px | 500 | Sans uppercase | 1.2px |
| Param label | 11px | 400 | Sans | 0.3px |
| Param value | 13px | 500 | Mono | 0 |
| Step number | 9px | 400 | Mono | 0 |
| FX name (selected) | 13px | 500 | Sans | 0.3px |
| Hint / unit | 10px | 400 | Mono | 0 |
| Macro label | 12px | 500 | Sans | 0.5px |

### Règles

- **Sentence case partout**, sauf section labels en `UPPERCASE` avec letter-spacing
- Jamais de bold > 500 (700/800 = trop lourd, fait toy)
- Jamais de font-size < 9px (illisibilité)
- Numbers tabulaires (`font-feature-settings: 'tnum'`) pour les valeurs qui changent

---

## 4. Espacement & grid

```css
--space-1:  4px;
--space-2:  8px;
--space-3:  12px;
--space-4:  16px;
--space-5:  24px;
--space-6:  32px;
--space-7:  48px;
--space-8:  64px;
```

**Grille de base** : 8px. Tout s'aligne sur des multiples de 8 (parfois 4 pour fine-tuning).

**Border-radius** :
```
--radius-sm:   3px;   /* badges, mini-pills */
--radius-md:   6px;   /* boutons, inputs */
--radius-lg:   10px;  /* cards, panels */
--radius-xl:   14px;  /* modales, conteneur principal */
```

**Tailles fenêtre VST** :
- Default : 1100 × 700
- Min : 980 × 620
- Resize avec ratio fixe (pas de stretch désordonné)

---

## 5. Composants

### 5.1 Knob (le composant le plus important)

Inspiration directe Pigments + Vital.

**Anatomie** :
- Cercle extérieur 40-56px (selon contexte)
- Track de fond : arc 270° (de 7h à 5h en sens horaire), couleur `--bg-surface-3`
- Track actif : même arc, fill couleur du FX ou `--signal-primary` selon contexte
- Indicateur central : ligne fine (1.5px) du centre vers le bord, position = valeur
- Bouton central : disque `--bg-surface-2` avec micro-bordure `--border-default`
- Label en dessous, valeur au survol/focus en mono

**États** :
- Idle : track actif à 80% opacité
- Hover : track à 100%, label devient `--text-primary`
- Drag : valeur en gros au centre du knob, le label se cache
- Modulé : **halo concentrique** couleur `--mod-purple` (voir §7)

### 5.2 Step cell (le cœur du sequencer)

16 cellules carrées, alignées horizontalement.

**Anatomie d'un step** :
```
┌──────┐
│  07  │  ← numéro du step (mono, top-left, 9px)
│  ●   │  ← grosse pastille couleur FX (au centre)
│ STT  │  ← abbreviation FX (3-4 chars, 9px mono)
└──────┘
```

**Dimensions** : ~52×52px, gap 4px entre eux

**États** :
- Off : fond `--bg-surface-1`, pastille gris
- On (FX assigné) : fond couleur FX à 8%, pastille à 100%, abbréviation en couleur FX
- Active (en cours de lecture) : pulse white-flash 80ms, accent ring `--signal-active`
- Probability < 100% : bordure droite gradient (50% prob = moitié de bordure)

### 5.3 FX selector dropdown

Quand on clique sur une cell pour assigner un FX.

Layout : grille 3×4 de petites cards (40×40px chacune), chacune avec :
- Icône abstrait du FX (forme géométrique simple)
- Couleur du FX en bordure
- Au hover : nom complet apparaît dessous

### 5.4 Slider (pour params linéaires)

- Track : 4px de haut, fond `--bg-surface-3`
- Track actif : fill `--signal-primary` (ou couleur FX en contexte)
- Thumb : 14×14px, `--bg-surface-2` avec bordure 1px `--border-strong`
- Modulé : même halo purple que les knobs

### 5.5 Toggle / switch

Pill 36×20px.
- Off : fond `--bg-surface-3`, dot `--text-tertiary`
- On : fond couleur contextuelle (FX ou primary), dot blanc

### 5.6 Macro pad (pour le Live tab)

Composant XY pad **carré**, 140×140px, parfait pour la perf scénique.

- Fond `--bg-surface-1` avec grille subtile (lignes à 25/50/75%)
- Curseur : disque 24px, `--mod-purple`, traîne semi-transparente sur 200ms
- Label en haut left (assignation)
- Valeurs X/Y en bottom right (mono)

### 5.7 Visualization panels

#### Spectrum analyzer
- Bars verticaux, 64 ou 128 bins
- Couleur : `--signal-primary` à 60% opacité
- Décay 8 dB/s
- Background `--bg-void`

#### Waveform display (input vs output)
- Deux courbes superposées
- Input : `--text-tertiary` (gris)
- Output : `--signal-primary` (cyan)
- Background `--bg-void` avec ligne médiane subtile

---

## 6. Layouts par onglet

### 6.1 Production tab (vue principale)

```
┌─────────────────────────────────────────────────────────────────┐
│ [logo] PRESET NAME ▾  [<] [A] [B] [>]   [tab P|L|B]  [⚙]      │ ← header 48px
├──────────────┬──────────────────────────────────────────────────┤
│              │ ┌────────────────────────────────────────────┐   │
│  MACROS      │ │       SPECTRUM        │      WAVEFORM      │   │
│  [pad1]      │ └────────────────────────────────────────────┘   │
│  [pad2]      │ ┌────────────────────────────────────────────┐   │
│  [pad3]      │ │  STEP SEQUENCER  16 cells                   │   │
│  [pad4]      │ │  [01][02][03][04][05][06][07][08]...        │   │
│              │ │  swing: ▬▬▬   length: 16   division: 1/16  │   │
│  ── source ──│ └────────────────────────────────────────────┘   │
│  LFO 1       │ ┌────────────────────┬─────────────────────────┐ │
│  LFO 2       │ │ FX PARAMS          │ MODULATION ROUTINGS    │ │
│  S&H         │ │ (couleur du FX     │  src → dest [depth]    │ │
│  Env Foll.   │ │  sélectionné)      │  LFO1 → cutoff [+45]   │ │
│              │ │ [knob][knob][knob] │  ENV → density [+22]   │ │
│  ── globals -│ └────────────────────┴─────────────────────────┘ │
│  In   ▬▬▬   │                                                   │
│  Out  ▬▬▬   │                                                   │
│  D/W  ▬▬▬   │                                                   │
└──────────────┴──────────────────────────────────────────────────┘
   220px              880px
```

### 6.2 Live tab (perf-oriented)

Layout pensé pour être **lisible à 2m de distance** sur scène.

```
┌─────────────────────────────────────────────────────────────────┐
│ [logo] PRESET   |   [P|L|B]                                     │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│   [BIG MACRO PAD 1]    [BIG MACRO PAD 2]    [BIG MACRO PAD 3]  │
│       240×240px            240×240px            240×240px      │
│                                                                 │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│   [TRIGGER PAD A]  [TRIG B]  [TRIG C]  [TRIG D]                │
│   patterns                                                      │
│                                                                 │
├─────────────────────────────────────────────────────────────────┤
│   STEP INDICATOR :  ● ● ● ● ● ● ● ● ● ● ● ● ● ● ● ●            │
│   BPM 128.0    SCENE A ─────● B    DRY/WET ▬▬▬▬▬▬             │
└─────────────────────────────────────────────────────────────────┘
```

**Priorités Live tab** :
- Tout doit être tappable au doigt sur écran tactile
- Indicateur de step géant (boules de 16px de diamètre minimum)
- Police 16-20px pour le BPM et les labels macros
- Couleurs vives : pas peur du saturé pour la visibilité scène

### 6.3 Browser tab

```
┌─────────────────────────────────────────────────────────────────┐
│ [logo] BROWSER   |   [P|L|B]                       [search 🔍] │
├─────────────────┬──────────────────────────────────────────────┤
│ TAGS            │  ┌──────────┬──────────┬──────────┐          │
│ ─ glitch (24)   │  │  PRESET  │  PRESET  │  PRESET  │          │
│ ─ ambient (12)  │  │ thumbnail│ thumbnail│ thumbnail│          │
│ ─ live (18)     │  │  name    │  name    │  name    │          │
│ ─ transition    │  │  tags    │  tags    │  tags    │          │
│ ─ percussive    │  └──────────┴──────────┴──────────┘          │
│                 │  (grid 3 colonnes auto-fit)                  │
│ AUTHOR          │                                              │
│ ─ Factory (30)  │                                              │
│ ─ User (8)      │                                              │
└─────────────────┴──────────────────────────────────────────────┘
```

---

## 7. Modulation visualization (le killer feature)

C'est ce qui différencie ton plugin du tout-venant. Inspiré de Vital, mais avec ton propre twist.

### Drag-to-modulate

L'utilisateur **fait glisser** une source de modulation (LFO1, ENV, etc.) depuis le panel sources sur n'importe quel knob de l'UI. Au drop : le routing est créé, le knob acquiert un halo purple.

### Halo de modulation autour d'un knob

Visuel double-arc :
- Arc inférieur (= valeur de base APVTS) : couleur du FX, opacité 100%
- Arc supérieur (= modulation appliquée en temps réel) : couleur `--mod-purple`, opacité 60%, animé avec la valeur instantanée

```
       ▁▁▁▂▂▂▃▃▃            ← halo modulation purple animé
     ▁▆      ▆▁
    ▆          ▆
   ▆     ●      ▆           ← knob central
    ▆          ▆
     ▆▁      ▁▆
        ▔▔▔▔
       (arc base)
```

### Pulsation au control rate

Le halo "pulse" subtilement à la fréquence de la source modulante (LFO 0.5 Hz = halo qui respire à 0.5 Hz). Pour un envelope follower, le halo réagit à l'amplitude du signal.

### Right-click pour éditer

Right-click sur un knob modulé → popup avec liste des routings :
```
┌────────────────────────────────┐
│ FilterCutoff                    │
├────────────────────────────────┤
│ LFO1     depth +45%   [×]      │
│ Macro1   depth -20%   [×]      │
│ + Add modulation...            │
└────────────────────────────────┘
```

---

## 8. Motion principles

### Durées

```
--motion-instant: 80ms;    /* feedback immédiat (click, step trigger) */
--motion-fast:    150ms;   /* hover, focus */
--motion-medium:  300ms;   /* panel transitions, tab switch */
--motion-slow:    500ms;   /* preset load, scene morph */
```

### Easing

```
--ease-out:    cubic-bezier(0.16, 1, 0.3, 1);     /* sortie naturelle */
--ease-in-out: cubic-bezier(0.65, 0, 0.35, 1);    /* transition fluide */
--ease-spring: cubic-bezier(0.34, 1.56, 0.64, 1); /* léger overshoot pour les pads */
```

### Règles

- **Toujours** animer les changements d'état (hover, focus, active)
- **Jamais** d'animation > 500ms (sauf preset load)
- Les valeurs de paramètres : interpolation linéaire 16ms (≠ animation, c'est du smoothing audio)
- Les visualisations (spectrum, waveform) : 60fps via OpenGL

---

## 9. Implémentation JUCE

### LookAndFeel custom

Crée une classe `GlitchwaveLookAndFeel : public juce::LookAndFeel_V4` qui override :
- `drawRotarySlider` (pour les knobs custom avec halo)
- `drawLinearSlider` (pour les sliders)
- `drawButtonBackground`
- `drawComboBox`
- `getLabelFont` (pour appliquer IBM Plex)

### Intégration des polices

1. Télécharger IBM Plex Sans + Mono depuis Google Fonts
2. Convertir en TTF
3. Embed dans BinaryData via `juce_add_binary_data` dans CMake
4. Charger au démarrage :

```cpp
auto plexSans = juce::Typeface::createSystemTypefaceFor(
    BinaryData::IBMPlexSans_ttf, BinaryData::IBMPlexSans_ttfSize);
juce::LookAndFeel::getDefaultLookAndFeel().setDefaultSansSerifTypeface(plexSans);
```

### Renderer OpenGL

Mandatoire pour la fluidité des visualisations et halos modulation.

```cpp
class PluginEditor : public juce::AudioProcessorEditor {
    juce::OpenGLContext openGLContext;
public:
    PluginEditor(...) {
        openGLContext.attachTo(*this);
        // ...
    }
};
```

### Couleurs en code

Définir une struct `Colors` accessible globalement :

```cpp
namespace Colors {
    inline const juce::Colour bgVoid       { 0xFF0A0B0F };
    inline const juce::Colour bgSurface1   { 0xFF14161D };
    inline const juce::Colour bgSurface2   { 0xFF1C1F28 };
    inline const juce::Colour textPrimary  { 0xFFE8EAF0 };
    inline const juce::Colour signalPrimary{ 0xFF00D9C4 };
    inline const juce::Colour modPurple    { 0xFFB084EB };
    
    namespace Fx {
        inline const juce::Colour stutter   { 0xFF00D9C4 };
        inline const juce::Colour reverser  { 0xFFFF6B9D };
        inline const juce::Colour tapestop  { 0xFFFFA94D };
        // ... etc
    }
}
```

---

## 10. Checklist before shipping UI

- [ ] Toutes les couleurs du design system sont en variables CSS / constants C++ (zéro hex codé en dur dans les composants)
- [ ] IBM Plex Sans + Mono embarqués dans BinaryData
- [ ] LookAndFeel custom override les composants standards
- [ ] OpenGL renderer attaché à l'editor principal
- [ ] Tous les knobs supportent l'affichage du halo de modulation
- [ ] Drag-to-modulate fonctionne entre n'importe quelle source et n'importe quel knob
- [ ] Onglet Live testé sur écran tactile (Surface, iPad via Sidecar)
- [ ] Resize fenêtre conserve les proportions
- [ ] Dark theme cohérent : pas un seul élément qui "explose" en lumineux
- [ ] Animations à 60fps minimum, jamais de stutter visuel
- [ ] Mockup HTML compagnon (`mockup-production.html`) referencé pour la conformité visuelle

---

*Design system v0.1 — 10 mai 2026. Itère après le premier vrai rendu, pas avant.*
