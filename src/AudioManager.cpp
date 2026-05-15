#include "AudioManager.h"

AudioManager& AudioManager::Instance() 
{
    static AudioManager instance;
    return instance;
}

void AudioBus::setBaseVolume(float v) {
    baseVolume = std::clamp(v, 0.f, 100.f);
    apply();
}

void AudioBus::setMuted(bool m) {
    muted = m;
    apply();
}

float AudioBus::getBaseVolume() const { return baseVolume; }
bool  AudioBus::isMuted()      const { return muted; }

void AudioBus::registerSound(sf::SoundSource* snd) {
    m_sounds.push_back(snd);
    snd->setVolume(currentVolume());
}

float AudioBus::currentVolume() const {
    return muted ? 0.f : baseVolume;
}

void AudioBus::apply() {
    for (auto* snd : m_sounds)
        snd->setVolume(currentVolume());
}

void AudioBus::playSound(sf::SoundSource& snd, float volume) {
    snd.setVolume(volume >= 0.f ? volume : currentVolume());
    snd.play();
}

void MusicBus::setBaseVolume(float v) {
    baseVolume = std::clamp(v, 0.f, 100.f);
    apply();
}

void MusicBus::setMuted(bool m) {
    muted = m;
    apply();
}

float MusicBus::getBaseVolume() const { return baseVolume; }
bool  MusicBus::isMuted()      const { return muted; }

void MusicBus::registerMusic(sf::Music* m) {
    m_tracks.push_back(m);
    m->setVolume(currentVolume());
}

void MusicBus::stopAll() {
    for (auto* m : m_tracks)
        m->stop();
}

void MusicBus::pauseAll() {
    for (auto* m : m_tracks)
        m->pause();
}

void MusicBus::playExclusive(sf::Music& m) {
    if (m_current == &m && m.getStatus() == sf::Music::Status::Playing)
        return;
    stopAll();
    m.setVolume(currentVolume());
    m.setLooping(true);
    m.play();
    m_current = &m;
}

float MusicBus::currentVolume() const {
    return muted ? 0.f : baseVolume;
}

void MusicBus::apply() {
    for (auto* m : m_tracks)
        m->setVolume(currentVolume());
}