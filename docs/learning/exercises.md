# Exercises — C++ pour devs web

Exercices progressifs pour acquérir les fondamentaux C++ dont on a besoin sur Tessera. Du basique vers le DSP / RT-safety.

Format : énoncé court, contraintes, solution cachée dans `<details>`. Ouvre la solution **après** avoir essayé.

Compilation pour tester : `g++ -std=c++20 -Wall -Wextra ex.cpp -o ex && ./ex`

---

## Niveau 1 — Fondamentaux

### Exercice 1.1 — Value, reference, pointer

Écris une fonction `increment` qui ajoute 1 à un entier. Fais 3 versions :
- `incrementByValue(int x)` — modifie une copie
- `incrementByRef(int& x)` — modifie l'original
- `incrementByPtr(int* x)` — modifie via pointeur (vérifie qu'il n'est pas null)

Question : si on appelle les 3 sur le même `int a = 5;`, que vaut `a` après les 3 appels et dans quel ordre ?

<details>
<summary>Solution</summary>

```cpp
#include <iostream>

void incrementByValue(int x)  { x += 1; }            // modifie la copie
void incrementByRef(int& x)   { x += 1; }            // modifie l'original
void incrementByPtr(int* x)   { if (x) *x += 1; }    // modifie via pointeur

int main() {
    int a = 5;
    incrementByValue(a);   // a = 5 (inchangé)
    incrementByRef(a);     // a = 6
    incrementByPtr(&a);    // a = 7
    std::cout << a << "\n";
}
```

**Analogie TS** : en TS, tous les types primitifs (number, boolean) passent par valeur. Les objets passent par référence implicite. En C++, c'est explicite : `T` = valeur, `T&` = référence, `T*` = pointeur (nullable).

**Règle Tessera** : dans le path audio, on passe par `const T&` ou `std::span<T>` pour éviter les copies coûteuses sur les buffers.
</details>

---

### Exercice 1.2 — RAII et lifetimes

Écris une classe `LogScope` qui :
- Dans son constructeur, affiche `"enter <name>"`
- Dans son destructeur, affiche `"exit <name>"`

Puis utilise-la dans une fonction `foo()`. Affiche aussi `"middle"` au milieu. L'ordre attendu :
```
enter foo
middle
exit foo
```

<details>
<summary>Solution</summary>

```cpp
#include <iostream>
#include <string>

class LogScope {
    std::string name;
public:
    LogScope(std::string n) : name(std::move(n)) {
        std::cout << "enter " << name << "\n";
    }
    ~LogScope() {
        std::cout << "exit " << name << "\n";
    }
};

void foo() {
    LogScope guard("foo");
    std::cout << "middle\n";
}  // guard tombe ici → destructeur appelé automatiquement

int main() { foo(); }
```

**Concept clé : RAII** = Resource Acquisition Is Initialization. Le destructeur d'un objet est appelé **automatiquement** quand l'objet sort de portée (fin du bloc `{}`). Cela permet de garantir la libération de ressources (mémoire, fichier, mutex) sans `try/finally`.

**Analogie React** : pas d'équivalent direct. Le plus proche est `useEffect(() => { setup(); return cleanup; }, [])` — mais en C++, le cleanup est **obligatoire** et lié au scope.

**Règle Tessera** : `juce::ScopedNoDenormals` est un RAII. Il flip les flags FPU au constructeur et les remet au destructeur. C'est pour ça qu'on le déclare juste comme variable locale en haut de `processBlock`.
</details>

---

### Exercice 1.3 — `const` partout

Modifie ce code pour ajouter `const` là où c'est légitime. Compile et explique chaque ajout.

```cpp
int compute(int input) {
    int factor = 2;
    int result = input * factor;
    return result;
}

class Counter {
    int value = 0;
public:
    int get() { return value; }
    void increment() { value += 1; }
};
```

<details>
<summary>Solution</summary>

```cpp
int compute(int input) {                    // pas de const sur le retour : valeur primitive
    const int factor = 2;                   // factor ne change jamais → const
    const int result = input * factor;      // result ne change jamais après init
    return result;
}

class Counter {
    int value = 0;
public:
    int get() const { return value; }       // const après () → ne modifie pas l'objet
    void increment() { value += 1; }        // pas const, on modifie value
};
```

**Concept clé : const-correctness** = `const` indique au compilateur (et au lecteur) qu'une chose ne change pas. Avantages :
- Erreurs de compilation si on essaie de modifier par accident → bug évité
- Le compilateur peut mieux optimiser
- Code plus lisible : un `const Knob&` n'est jamais modifié dans la fonction qui le reçoit

**Règle Tessera** : passer des buffers en `const juce::AudioBuffer<float>&` quand on ne les modifie pas. Méthodes "getter" sur les modules DSP toutes en `const`.
</details>

---

## Niveau 2 — Mémoire et types

### Exercice 2.1 — `std::array` vs `std::vector`

Crée un buffer de 1024 floats. Implémente-le deux fois :
- Version A : `std::array<float, 1024>`
- Version B : `std::vector<float>` avec `reserve(1024)` puis `push_back` dans une boucle

Compile en `-O2`. Compare la taille du binaire et la lisibilité.

Quel est le bon choix dans le path audio ? Pourquoi ?

<details>
<summary>Solution</summary>

```cpp
#include <array>
#include <vector>

void versionA() {
    std::array<float, 1024> buf{};   // sur la pile, taille fixe, zéro alloc
    for (int i = 0; i < 1024; ++i) buf[i] = i * 0.1f;
}

void versionB() {
    std::vector<float> buf;
    buf.reserve(1024);               // alloue 1024 sur le HEAP au démarrage
    for (int i = 0; i < 1024; ++i) buf.push_back(i * 0.1f);
}
```

**Différence fondamentale** :
- `std::array<T, N>` : taille connue à la **compilation**, vit sur la **pile** (ou dans la classe contenante), zéro allocation dynamique.
- `std::vector<T>` : taille runtime, alloue sur le **tas** au premier `reserve`/`push_back`, peut réallouer.

**Dans le path audio Tessera** :
- `std::array` : ✅ RT-safe
- `std::vector` : ❌ même avec `reserve`, l'allocation initiale peut bloquer ; `push_back` peut allouer si capacity dépassée ; et certaines implémentations debug touchent l'allocateur même at-capacity (cf. lesson 0003)

**Règle générale** : si la taille est connue à la compile (`MaxVoices = 32`), `std::array`. Si runtime mais bounded, `std::array` avec un compteur `active`. Jamais `std::vector` dans `processBlock`.
</details>

---

### Exercice 2.2 — `std::span<float>` comme view

Écris une fonction `applyGain` qui multiplie chaque sample d'un buffer par un gain.

Signature attendue : `void applyGain(std::span<float> buffer, float gain);`

Appelle-la avec :
- Un `std::array<float, 64>`
- Un `std::vector<float>` de 128 éléments
- Un C-array `float c[32]`

Pourquoi `std::span` est-il préférable à `(float* ptr, size_t len)` ?

<details>
<summary>Solution</summary>

```cpp
#include <span>
#include <array>
#include <vector>

void applyGain(std::span<float> buffer, float gain) {
    for (float& s : buffer) s *= gain;
}

int main() {
    std::array<float, 64> a{};
    std::vector<float> v(128);
    float c[32]{};

    applyGain(a, 0.5f);          // ✅ array → span auto
    applyGain(v, 0.5f);          // ✅ vector → span auto
    applyGain(c, 0.5f);          // ✅ C-array → span auto
}
```

**Concept clé** : `std::span<T>` (C++20) = paire (pointeur, taille) **sans posséder** la mémoire. C'est une **vue** : tu lis/écris à travers, tu ne possèdes pas, tu ne libères pas.

**Avantages vs `(T* ptr, size_t len)`** :
- Un seul argument au lieu de deux → impossible de désaccorder taille et pointeur
- Range-based for marche directement
- Conversion implicite depuis array/vector/C-array
- `[[nodiscard]]` natif dans la STL

**Analogie TS** : `readonly Float32Array` (mais en C++ le const est explicite : `std::span<const float>` pour read-only).

**Règle Tessera** : on préfère `std::span` aux paires (ptr, size) dans nos APIs DSP internes. Les APIs JUCE existantes utilisent `juce::AudioBuffer<float>&` (équivalent moral), on garde ça pour l'interop.
</details>

---

## Niveau 3 — Threading et atomics

### Exercice 3.1 — `std::atomic<int>` counter

Deux threads incrémentent un compteur 1 000 000 fois chacun. Version A avec `int`, version B avec `std::atomic<int>`. Compile avec `-pthread`. Quelle valeur attends-tu à la fin ?

```cpp
// Squelette
#include <thread>
#include <atomic>
#include <iostream>

int counterA = 0;
std::atomic<int> counterB{0};

int main() {
    std::thread t1([] { for (int i = 0; i < 1'000'000; ++i) counterA++; });
    std::thread t2([] { for (int i = 0; i < 1'000'000; ++i) counterA++; });
    t1.join(); t2.join();
    std::cout << "A=" << counterA << "\n";

    // Même chose avec counterB
    // ...
}
```

<details>
<summary>Solution</summary>

```cpp
#include <thread>
#include <atomic>
#include <iostream>

int counterA = 0;
std::atomic<int> counterB{0};

int main() {
    {
        std::thread t1([] { for (int i = 0; i < 1'000'000; ++i) counterA++; });
        std::thread t2([] { for (int i = 0; i < 1'000'000; ++i) counterA++; });
        t1.join(); t2.join();
    }
    std::cout << "A=" << counterA << "\n";   // ❌ data race : valeur indéterminée

    {
        std::thread t1([] { for (int i = 0; i < 1'000'000; ++i) counterB++; });
        std::thread t2([] { for (int i = 0; i < 1'000'000; ++i) counterB++; });
        t1.join(); t2.join();
    }
    std::cout << "B=" << counterB << "\n";   // ✅ 2 000 000 garanti
}
```

**Concept clé** : un `int` non-atomic incrémenté depuis 2 threads = **data race** = comportement indéfini (UB). En pratique, le compilateur fait `load → add → store`, et un autre thread peut s'intercaler entre `load` et `store`. Tu perds des incréments.

`std::atomic<int>` garantit que chaque opération (load, store, fetch_add, etc.) est **indivisible** vu de tous les threads.

**Analogie React** : pas d'équivalent direct (JS est single-threaded). Le plus proche : `useState` setter via callback `setX(prev => prev + 1)` qui garantit la cohérence — mais en C++ multi-thread, ce n'est pas suffisant, il faut vraiment de l'atomic.

**Règle Tessera** : tout échange UI ↔ DSP passe par `std::atomic` ou lock-free FIFO. Jamais `int` ou `float` "nu" partagé entre threads. APVTS expose tous les paramètres comme `std::atomic<float>*` pour cette raison.
</details>

---

### Exercice 3.2 — Ring buffer minimaliste

Implémente un mini ring buffer de capacité 8 `float`, avec :
- `write(float x)` : écrit à la position courante, avance
- `read(int offset)` : lit à `(writePos - offset) % 8`
- `writePos` en `std::atomic<int>`

Pas de protection contre lecture du futur — on suppose que l'appelant lit toujours dans le passé.

<details>
<summary>Solution</summary>

```cpp
#include <atomic>
#include <array>

class MiniRingBuffer {
    static constexpr int N = 8;
    std::array<float, N> buf{};
    std::atomic<int> writePos{0};
public:
    void write(float x) {
        int pos = writePos.load();
        buf[pos % N] = x;
        writePos.store(pos + 1);
    }

    float read(int samplesAgo) const {
        int pos = writePos.load() - samplesAgo;
        // attention : pos peut être négatif → wrap correct
        int idx = ((pos % N) + N) % N;
        return buf[idx];
    }
};
```

**Préparation pour CaptureBuffer** : c'est la même idée à plus grande échelle (4 bars × 96 kHz = ~750 000 samples au lieu de 8). Différences à venir :
- `int64_t` au lieu de `int` pour `writePos` (on ne wrap jamais sur 64 bits → simplifie les calculs)
- Interpolation linéaire pour lire à position fractionnaire
- Pré-allocation dans `prepare(sampleRate)`, pas dans le constructeur
- Stéréo : `std::array<std::array<float, N>, 2>` ou `juce::AudioBuffer<float>` directement

**Le piège à éviter** : si tu fais `read` pendant que `write` est en cours, l'atomic load de writePos est sûr, mais la valeur du sample lu peut être "torn" (à moitié écrite). En pratique sur x86 c'est OK pour `float` aligné mais c'est techniquement UB. Pour CaptureBuffer on accepte ce risque parce qu'un grain qui lit un sample bizarre 1× sur 750k est inaudible. Pour des données critiques (un counter), on utiliserait un double buffer.
</details>

---

## Niveau 4 — Templates et concepts (C++20)

À ajouter avant la semaine 7 (ModulationMatrix utilise des templates).

---

## Niveau 5 — DSP-spécifique

À ajouter au fur et à mesure :
- Convolution / FIR filter
- Enveloppe Hann
- Phase accumulation pour LFO
- Forward Euler pour Lorenz
