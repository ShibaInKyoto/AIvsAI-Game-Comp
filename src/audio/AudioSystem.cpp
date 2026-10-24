#include "audio/AudioSystem.h"

namespace gb {

bool AudioSystem::Init(bool) { return true; }
void AudioSystem::Shutdown() {}
void AudioSystem::Update(float) {}
void AudioSystem::ApplyVolumes(const Settings&) {}

} // namespace gb
