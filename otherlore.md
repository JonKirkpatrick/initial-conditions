# ノムノム野郎 — Game Lore & Design Bible
*Working document — do not stray from this path*

---

## The World

The game is set in **Newfoundland** — a wilderness teeming with fauna whose dung is an extraordinarily valuable resource. The harvesting of this dung is the foundation of an entire industry, headquartered in **Poltergeist**, a fictional country that has built its economy entirely around Newfoundland's output.

---

## NomNomCo

**NomNomCo** is the corporation responsible for harvesting and processing Newfoundland's dung into food. It is a serious, professional organisation that takes its mission with complete sincerity.

The company is known to its employees and the wider world as **NomNomCo**. However, back in the protagonist's home prefecture in Japan, his friends and family refer to it simply as **ノムノム** — because the syllabic construction of "NomNom" doesn't follow proper Japanese phonetic conventions, and they improvised their own version. To them it means the same thing. To the corporate branding team, it would be a minor affront. Nobody has mentioned it.

---

## The Legend of PacMan

Within this fictional world, the arcade game **PacMan** was not a game at all — it was a dramatisation of real events. PacMan was a historical figure: a man who worked in the **shipping and receiving department** of NomNomCo. The shipping containers were laid out as mazes. The auditors dispatched from Poltergeist to oversee operations happened, for reasons the lore does not need to explain, to be ghosts.

PacMan navigated the mazes. He ate. He survived. He became a legend.

What the legend does not mention is that **PacMan's grandfather** was deeply, quietly disappointed in his grandson's career choice. The grandfather was a master harvester — a man who understood that the true work, the honourable work, was out in the field. Shipping and receiving was, in his view, a waste of a life. He never said this aloud. He was too dignified. But everyone knew.

---

## ノムノム野郎 — NomNom Bastard

**ノムノム野郎** (romanised: *Nomu Nomu Yarō*) is the protagonist. "野郎" sits somewhere between "bastard", "rascal", and "guy" — not quite an insult, not quite a term of endearment. Someone who gets things done and doesn't particularly care what you think about it.

He comes from an **agricultural family in a backwater prefecture** in Japan. As a young man, he was taken on as a student by **PacMan's grandfather**, who taught him the art of harvesting — not as mere labour, but as a discipline. A bushido of harvesting. There are proper techniques. There is honour in doing it correctly. There is shame in doing it poorly.

The grandfather saw in ノムノム野郎 the student his own grandson had never been.

ノムノム野郎 eventually came to work for NomNomCo, drawn by the scale of the operation and a sense that this was where the harvesting tradition truly mattered. He is a professional. He is serious about his work. The fact that his work involves dung is entirely beside the point.

**Appearance:** He resembles PacMan in basic form — the family resemblance to the corporate mythology is intentional — but wears a headband in the spirit of Afro-Samurai. The headband signals purpose and identity. He has a mission. He takes it seriously.

---

## The Corruption

Production at NomNomCo has been declining. After investigation, the source has been identified: **corruption** spreading through the Newfoundland ecosystem.

The corruption takes the form of **swarms and nests of flies**.

The mechanics are as follows:

- **Nests** spawn **swarms**
- Swarms **damage the player** and **corrupt the fauna**
- Corrupted fauna produce **corrupted dung**
- Corrupted dung provides a **brooding site** for new flies
- The flies spawned from corrupted dung **take on the characteristics of the fauna it came from**

This means the corruption is self-reinforcing. A corrupted wolf produces dung that spawns wolf-like flies — potentially with pack behaviour. A corrupted Komodo produces dung that spawns poisonous flies. A corrupted starling produces dung that spawns airborne swarming flies. Left unchecked, the ecosystem tips toward total collapse and NomNomCo's supply chain fails entirely.

**ノムノム野郎's job** is to enter each level, locate the source of corruption, remove it, deal with the corrupted fauna, and restore equilibrium. He approaches this with the same discipline and craft that PacMan's grandfather taught him. The flies are not just an ecological problem. They are a desecration of the harvesting tradition. It is personal.

---

## The Fauna & Biogenic Payload Matrix

Each creature's dung serves as its own ideal behavioral countermeasure, weaponized via high-end *NomNomCo* industrial equipment.

### Starling
- **Role:** Flocking prey species, most numerous
- **Behaviour:** Classic boids flocking, murmuration at scale
- **Appearance:** Nearly black sphere with brilliant violet iridescence — view-dependent
- **Corrupted variant:** Murmuration behaviour inverts into aggressive swarming
- **Dung/Ammo Mechanic (Hitscan Ray):** Deployed as a linear particle sniper rifle. Instantly strikes target, forcing inverted swarms back into defensive, passive boids.

### Hawk
- **Role:** Apex aerial predator, sparse population
- **Behaviour:** Soaring thermals, diving strikes on starling flocks
- **Appearance:** Dark golden orb, simple
- **Dung/Ammo Mechanic (Fly-By-Wire Missile):** Launches a remote-controlled ordnance. The player's perspective cuts to a high-altitude camera view, allowing them to survey the landscape from above before piloting the missile directly into targets.

### Yeti *(originally Bear)*
- **Role:** Large omnivore, interior ecosystem
- **Behaviour:** Slow, powerful, opportunistic
- **Appearance:** Grey-white fur texture with pronounced normal map
- **Corrupted variant:** Loses omnivore opportunism, becomes pure aggressor
- **Dung/Ammo Mechanic:** TBD (On hold).

### Deer
- **Role:** Grazer, open terrain
- **Behaviour:** Nervous, strong flight response to wolves and yeti
- **Corrupted variant:** High flight response becomes a dangerous, stampeding charge.
- **Dung/Ammo Mechanic (Decoy Pheromone):** Creates a localized "safe zone" scent cloud that immediately pacifies stampeding deer, snapping them out of their flight frenzy.

### Wolf
- **Role:** Pursuit predator, pack behaviour
- **Behaviour:** Pack dynamics emerging from boids alignment rules, drives deer population
- **Corrupted variant:** Hyper-coordinated pack AI shifts from hunting prey to aggressively guarding corruption nests.
- **Dung/Ammo Mechanic (Disruption Mist):** Releases a gas that shatters their boids alignment rules, causing the pack to lose coordination and scatter blindly in random directions.

### Pangolin *(originally Crocodile, then Bear — twice displaced)*
- **Role:** Terrestrial tank, slow, ancient, largely ignored by predators
- **Behaviour:** Persists regardless of surroundings. Can retract its eyes and roll down terrain using environmental slopes.
- **Appearance:** Heavy armoured scales with deep inter-scale shadows, extraordinary normal map detail
- **Corrupted variant:** Loses passivity, becomes a high-velocity, unstoppable armored bowling ball targeting the player.
- **Dung/Ammo Mechanic (Crater Ordnance):** Heavy explosive payload that deforms the mesh terrain on impact. When a rolling Pangolin hits the altered surface normals of the crater lip, its physical vector is violently recalculated, launching it safely off-course.

### Komodo *(water/land omnivore)*
- **Role:** Coastal ambush predator, confluence of land and water
- **Behaviour:** Slow and patient, waits near water edges, follows wounded prey at distance
- **Poison mechanic:** Bite delivers a delayed effect where the victim weakens while the Komodo stalks from a sinister distance.
- **Corrupted variant:** Poison becomes airborne, creating toxic routing hazards for other creatures.
- **Dung/Ammo Mechanic (Universal Speed Retardant):** Neurotoxic sludge that serves as a universal slowness debuff. Used on a Komodo, it drops its tracking velocity to a crawl, breaking its stalk AI. Can be used improvisationally to slow down any creature in the sandbox.

### Giant Snail *(The Golden Ratio)*
- **Role:** Environmental anchor, ancient gastropod.
- **Appearance:** Massive, towering shell built on a flawless logarithmic Fibonacci spiral. Deeply normal-mapped translucent body.
- **Behaviour:** Moves along strict paths leaving a thick slime trail.
- **Dung/Ammo Mechanic (Zero-Friction Secretion):** Overwrites the terrain with a completely frictionless surface. Erases all traction, causing enemies (like a rolling Pangolin or charging Wolf) to violently spin out or slide past their targets. Used by the player to execute high-velocity world traversal.

---

## Cape Saint Mary's

The bird sanctuary level. The arrival moment.

The game builds toward this. The player has spent time in the interior with land creatures, and then crests a rise to find the whole Atlantic opening up and the sky full of birds. Starlings in murmuration. Hawks riding thermals. The Komodo on the rocks below the cliffs.

This is the **Journey Down** moment — the world has been building toward something and here it is.

The corruption at Cape Saint Mary's has a particular character: corrupted starling dung spawning airborne flies that corrupt the hawk population, the whole coastal ecosystem tipping toward collapse. The Komodo on the rocks below is either a source or a victim depending on the state of the level.

---

## Levels

- **Newfoundland interior** — yeti, wolf, deer, pangolin territory
- **Cape Saint Mary's** — bird sanctuary, coastal confluence, the arrival moment
- **The Moose Level (Secret Dimension)** — An unlisted corporate sector accessed via a hidden gameplay progression chain.

---

## Tone

The game plays everything completely straight. NomNomCo is a real corporation with real quarterly earnings at stake. ノムノム野郎 is a professional with a genuine philosophical tradition behind his work. The corruption is a serious threat. The dung is valuable.

The comedy emerges entirely from the gap between the gravity with which everyone treats the situation and the nature of the situation itself. No winking at the camera. No self-aware jokes. The game knows what it is and commits completely.

---

## Secret Progression: Eastwood & The Fissure

A completely undocumented, unguided sequence left for players to discover blindly. 

1. **The Rescue:** The player utilizes **Snail Dung Ammo** on a steep downslope to execute a high-velocity, zero-friction slide, catching massive airtime to clear a gap onto a high, isolated rock crag. Here, they retrieve ノムノム野郎's faithful Lagotto Romagnolo companion: **Eastwood**.
2. **The Tracker:** Once rescued, Eastwood acts as a mycological consultant, using his nose to sniff out and dig up Newfoundland's rare, underground psychoactive mushrooms.
3. **The Altered State:** Consuming the fungus places the player in an altered reality state. Standard environmental textures are stripped, revealing the underlying **Bake Shader** where the RGB channels encode the literal XZ and height coordinates of the world mesh. In this state, a spatial nexus becomes visible on the ground.
4. **The Fissure:** Firing a **Pangolin Dung Ordnance** directly into the nexus deforms the spatial normals, tearing a fissure through the fabric of the ecosystem and opening a portal to the Moose Level.

### The Moose Level Mechanics
- **Visuals:** Entirely rendered in the glowing, functional data-mesh of the Bake Shader.
- **The Hazards/Inhabitants:** Colossal, looming Moose Orbs that shed persistent vector trails behind them.
- **The Reward (Moose Ordnance):** Acquired within this realm, it shoots a temporary, walkable horizontal bridge made of data-mesh particles. It allows gravity-defying Z-axis platforming and can be smuggled back into standard *NomNomCo* corporate levels to completely break and optimize map geometry.

---

## Open Questions

- Is PacMan still alive in this world? Did he ever reconcile with his grandfather?
- Does ノムノム野郎 carry complicated feelings about being the replacement student?
- Final names for all six species
- Player vulnerability to Komodo poison and its gameplay implications
- Bespoke mechanic for the Yeti

---

## Production Credits (Internal Notes)
*   **Lead Shell Geometrician:** Leonardo Fibonacci *(Posthumous)*
*   **Lead Truffle Consultant & Mycological Tracker:** Eastwood

---

*This document was updated to lock in the foundational physics sandbox, weapon matrix, and deep secret architecture. Do not lose this thread.*