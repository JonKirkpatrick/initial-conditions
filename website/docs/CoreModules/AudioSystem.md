# High-Level Architecture: `AudioManager` Subsystem

The `AudioManager` subsystem provides centralized control over sound effects, background music streaming, volume mixing, and mute toggles across the engine. It organizes audio playback into distinct channels via dedicated bus abstractions (`AudioBus` and `MusicBus`), isolating short-form SFX processing from continuous background soundtrack management.

---

## 1. Primary Components & Architecture

* **`AudioManager`**: A thread-static Meyer's Singleton providing centralized access to system audio buses via `AudioManager::Instance()`. It owns two public member instances: `sfx` (`AudioBus`) and `music` (`MusicBus`).


* **`AudioBus`**: Manages short-lived sound effects and environmental audio sources (`sf::SoundSource`). Controls individual volume overrides, batch bus volume clamping $[0.0, 100.0]$, and global SFX muting.


* **`MusicBus`**: Controls long-form, streaming audio tracks (`sf::Music`). Provides state management for background soundtracks, including exclusive track playback, looping, global stops, and global pauses.



---

## 2. Audio Bus Capabilities & Mechanics

### AudioBus (Sound Effects)

* **Registration & Volume Sync**: `registerSound()` registers `sf::SoundSource` pointers into a internal tracking list (`m_sounds`) and syncs their initial volume to the bus's effective volume.


* **Effective Volume Calculation**: `currentVolume()` evaluates whether the bus is muted. If `muted == true`, it yields `0.0f`; otherwise, it returns `baseVolume`.


* **Playback Control**: `playSound()` sets the volume of a target `sf::SoundSource` (using a custom override if specified $\ge 0.0\text{f}$, or defaulting to the bus's `currentVolume()`) and triggers immediate playback.



### MusicBus (Background Soundtrack)

* **Exclusive Playback (`playExclusive`)**: Stops all active music tracks via `stopAll()`, configures the new track to loop infinitely (`m.setLooping(true)`), updates its volume to `currentVolume()`, begins playback, and assigns it to `m_current`. If the requested track is already active and playing, redundant state changes are ignored.


* **State Control**: `stopAll()` and `pauseAll()` iterate over registered `sf::Music*` tracks (`m_tracks`) to halt or pause playback across all tracks simultaneously.



---

## 3. Bus Propagation Flow

When base volume or mute state settings are updated, changes propagate across registered audio sources instantly:

```
                     [ Call: setBaseVolume(v) / setMuted(m) ]
                                        │
                                        ▼
                     ┌──────────────────────────────────────┐
                     │ Clamp Volume to Range [0.0, 100.0]   │
                     │         & Update Mute Flag           │
                     └──────────────────┬───────────────────┘
                                        │
                                        ▼
                     ┌──────────────────────────────────────┐
                     │          Call: apply()               │
                     └──────────────────┬───────────────────┘
                                        │
                                        ▼
                     ┌──────────────────────────────────────┐
                     │      Calculate currentVolume()       │
                     │  (Returns 0.0f if muted, else base)  │
                     └──────────────────┬───────────────────┘
                                        │
             ┌──────────────────────────┴──────────────────────────┐
             ▼                                                     ▼
┌─────────────────────────┐                           ┌─────────────────────────┐
│     AudioBus::apply     │                           │     MusicBus::apply     │
├─────────────────────────┤                           ├─────────────────────────┤
│ Iterates over m_sounds  │                           │ Iterates over m_tracks  │
│ Calls:                  │                           │ Calls:                  │
│ snd->setVolume(...)     │                           │ m->setVolume(...)       │
└─────────────────────────┘                           └─────────────────────────┘

```

---

## 4. Integration & Engine Usage

Downstream game scenes and UI menus interface with the audio subsystem via singleton calls:

```cpp
// Playing a sound effect through the SFX bus
sf::SoundSource& explosionSfx = getExplosionSound();
AudioManager::Instance().sfx.playSound(explosionSfx);

// Switching background music tracks exclusively
sf::Music& levelTheme = getLevelThemeTrack();
AudioManager::Instance().music.playExclusive(levelTheme);

// Modifying bus volumes from UI options sliders
AudioManager::Instance().sfx.setBaseVolume(75.0f);
AudioManager::Instance().music.setMuted(true);

```

This setup ensures that game logic remains separated from low-level volume mixing and channel tracking.