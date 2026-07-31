#pragma once

#include <SFML/Audio.hpp>
#include <vector>

/**
 * @class AudioBus
 * @brief Manages sound effects volume, muting, and active playback.
 * 
 * AudioBus acts as a controller for sound sources (like `sf::Sound` or `sf::SoundStream`), 
 * allowing batch manipulation of volume levels and mute states across registered SFML sounds.
 */
class AudioBus 
{
public:
    /**
     * @brief Sets the base volume for all registered sounds.
     * @param v Target volume, clamped between 0.0 and 100.0.
     */
    void setBaseVolume(float v);

    /**
     * @brief Mutes or unmutes all sounds on this bus.
     * @param m True to mute, false to restore base volume.
     */
    void setMuted(bool m);

    /**
     * @brief Gets the current unmuted base volume setting.
     * @return The base volume level (0.0 to 100.0).
     */
    float getBaseVolume() const;

    /**
     * @brief Checks if the audio bus is currently muted.
     * @return True if muted, false otherwise.
     */
    bool isMuted() const;

    /**
     * @brief Registers a sound source to be controlled by this bus.
     * @param snd Pointer to an SFML SoundSource instance.
     */
    void registerSound(sf::SoundSource* snd);

    /**
     * @brief Plays a sound using optional custom volume overrides.
     * @param snd Reference to the sound source to play.
     * @param volume Volume level to set before playing. If negative, defaults to current bus volume.
     */
    void playSound(sf::SoundSource& snd, float volume = -1.f);

private:
    float baseVolume = 50.f;
    bool  muted = false;
    std::vector<sf::SoundSource*> m_sounds;

    /** @brief Calculates effective volume considering the current mute state. */
    float currentVolume() const;

    /** @brief Applies the effective volume to all registered sounds. */
    void apply();
};

/**
 * @class MusicBus
 * @brief Manages background music playback, track switching, and volume levels.
 * 
 * MusicBus handles streaming music tracks, providing functionality for exclusive 
 * background track playback, global pauses, and global stops.
 */
class MusicBus {
public:
    /**
     * @brief Sets the base volume for all managed music tracks.
     * @param v Target volume, clamped between 0.0 and 100.0.
     */
    void setBaseVolume(float v);

    /**
     * @brief Mutes or unmutes all background music.
     * @param m True to mute, false to un-mute.
     */
    void setMuted(bool m);

    /**
     * @brief Gets the unmuted base volume setting.
     * @return The base volume level (0.0 to 100.0).
     */
    float getBaseVolume() const;

    /**
     * @brief Checks if background music is currently muted.
     * @return True if muted, false otherwise.
     */
    bool isMuted() const;

    /**
     * @brief Registers a music track pointer to be controlled by this bus.
     * @param m Pointer to an SFML Music object.
     */
    void registerMusic(sf::Music* m);

    /**
     * @brief Stops all currently registered music tracks.
     */
    void stopAll();

    /**
     * @brief Pauses all currently registered music tracks.
     */
    void pauseAll();

    /**
     * @brief Stops existing playback and plays the specified music track exclusively.
     * @param m Reference to the music track to loop and play.
     * @note Automatically sets the track to looping mode.
     */
    void playExclusive(sf::Music& m);

private:
    float baseVolume = 50.f;
    bool  muted = false;
    sf::Music* m_current = nullptr;
    std::vector<sf::Music*> m_tracks;

    /** @brief Calculates effective volume considering current mute state. */
    float currentVolume() const;

    /** @brief Applies the current volume setting across all registered music tracks. */
    void apply();
};

/**
 * @class AudioManager
 * @brief Singleton wrapper exposing global access to SFX and Music buses.
 * 
 * Access via `AudioManager::Instance()`.
 */
class AudioManager {
    AudioManager() = default;

public:
    AudioBus sfx;   ///< SFX channel bus for short audio effects
    MusicBus music; ///< Music channel bus for streaming background tracks

    /**
     * @brief Retrieves the global Singleton instance of the audio manager.
     * @return Reference to the thread-static AudioManager instance.
     */
    static AudioManager& Instance();

    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;
};