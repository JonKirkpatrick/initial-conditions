#pragma once

#include <SFML/Audio.hpp>
#include <vector>

class AudioBus 
{
public:
    void  setBaseVolume(float v);
    void  setMuted(bool m);
    float getBaseVolume() const;
    bool  isMuted() const;
    void  registerSound(sf::SoundSource* snd);
    void  playSound(sf::SoundSource& snd, float volume = -1.f);

private:
    float baseVolume = 50.f;
    bool  muted = false;
    std::vector<sf::SoundSource*> m_sounds;
    float currentVolume() const;
    void apply();
};

class MusicBus {
public:
    void setBaseVolume(float v);
    void setMuted(bool m);
    float getBaseVolume() const;
    bool  isMuted()      const;
    void registerMusic(sf::Music* m);
    void stopAll();
    void pauseAll();
    void playExclusive(sf::Music& m);

private:
    float baseVolume = 50.f;
    bool  muted = false;
    sf::Music* m_current = nullptr;
    std::vector<sf::Music*> m_tracks;
    float currentVolume() const;
    void apply();
};

class AudioManager {
    AudioManager() = default;

public:
    AudioBus sfx;
    MusicBus music;
    static AudioManager& Instance();

    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;
};