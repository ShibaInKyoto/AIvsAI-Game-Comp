#include "core/Settings.h"

#include "core/Storage.h"

#include <algorithm>

namespace gb {

namespace {

constexpr const char* kFileName = "settings.ini";

} // namespace

bool Settings::Load() {
    const auto text = storage::ReadTextFile(storage::DataDirectory() / kFileName);
    if (!text) {
        return false;
    }
    const auto doc = storage::KeyValueDocument::Parse(*text);
    masterVolume = doc.GetFloat("master_volume", masterVolume);
    musicVolume = doc.GetFloat("music_volume", musicVolume);
    sfxVolume = doc.GetFloat("sfx_volume", sfxVolume);
    fullscreen = doc.GetBool("fullscreen", fullscreen);
    vsync = doc.GetBool("vsync", vsync);
    renderScale = static_cast<int>(doc.GetInt("render_scale", renderScale));
    showFps = doc.GetBool("show_fps", showFps);
    screenShake = doc.GetFloat("screen_shake", screenShake);
    bloom = doc.GetBool("bloom", bloom);
    bloomIntensity = doc.GetFloat("bloom_intensity", bloomIntensity);
    chromaticAberration = doc.GetBool("chromatic_aberration", chromaticAberration);
    filmGrain = doc.GetBool("film_grain", filmGrain);
    scanlines = doc.GetBool("scanlines", scanlines);
    reduceFlashing = doc.GetBool("reduce_flashing", reduceFlashing);
    damageNumbers = doc.GetBool("damage_numbers", damageNumbers);
    autoFire = doc.GetBool("auto_fire", autoFire);
    aimAssist = doc.GetBool("aim_assist", aimAssist);
    Sanitize();
    return true;
}

bool Settings::Save() const {
    storage::KeyValueDocument doc;
    doc.SetFloat("master_volume", masterVolume);
    doc.SetFloat("music_volume", musicVolume);
    doc.SetFloat("sfx_volume", sfxVolume);
    doc.SetBool("fullscreen", fullscreen);
    doc.SetBool("vsync", vsync);
    doc.SetInt("render_scale", renderScale);
    doc.SetBool("show_fps", showFps);
    doc.SetFloat("screen_shake", screenShake);
    doc.SetBool("bloom", bloom);
    doc.SetFloat("bloom_intensity", bloomIntensity);
    doc.SetBool("chromatic_aberration", chromaticAberration);
    doc.SetBool("film_grain", filmGrain);
    doc.SetBool("scanlines", scanlines);
    doc.SetBool("reduce_flashing", reduceFlashing);
    doc.SetBool("damage_numbers", damageNumbers);
    doc.SetBool("auto_fire", autoFire);
    doc.SetBool("aim_assist", aimAssist);
    return storage::WriteTextFileAtomic(storage::DataDirectory() / kFileName, doc.Serialize());
}

void Settings::Sanitize() {
    masterVolume = std::clamp(masterVolume, 0.0f, 1.0f);
    musicVolume = std::clamp(musicVolume, 0.0f, 1.0f);
    sfxVolume = std::clamp(sfxVolume, 0.0f, 1.0f);
    screenShake = std::clamp(screenShake, 0.0f, 1.5f);
    bloomIntensity = std::clamp(bloomIntensity, 0.0f, 2.0f);
    if (renderScale != 0) {
        renderScale = std::clamp(renderScale, 50, 100);
    }
}

} // namespace gb
