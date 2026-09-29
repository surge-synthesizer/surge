// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "ControlSnapshot.h"
#include <cstddef>
#include <mutex>

struct OscillatorExtraConfiguration
{
    static constexpr size_t max_config = 64;
    int nData = 0;
    float data[max_config]{};
    OscillatorExtraConfiguration read() const { return *this; }
    OscillatorExtraConfiguration *edit() { return this; }
    const OscillatorExtraConfiguration &audioValue() const { return *this; }
};

// Control writers serialize complete edits, then publish before releasing the
// lock. DSP reads only the snapshot. An audio-thread type reset must first use
// tryLock(); its nested edit cannot wait because that thread already owns it.
class BrowserOscillatorExtraConfiguration
{
    mutable std::recursive_mutex writers;
    OscillatorExtraConfiguration model{}, audio{};
    ControlSnapshot<OscillatorExtraConfiguration> snapshot;
  public:
    class Edit
    {
        BrowserOscillatorExtraConfiguration &owner;
        std::lock_guard<std::recursive_mutex> lock;
      public:
        explicit Edit(BrowserOscillatorExtraConfiguration &o) : owner(o), lock(o.writers) {}
        ~Edit() { owner.snapshot.publish(owner.model); }
        Edit(const Edit &) = delete;
        Edit &operator=(const Edit &) = delete;
        OscillatorExtraConfiguration *operator->() { return &owner.model; }
    };
    Edit edit() { return Edit(*this); }
    auto tryLock() { return std::unique_lock<std::recursive_mutex>(writers, std::try_to_lock); }
    OscillatorExtraConfiguration read() const
    {
        const std::lock_guard<std::recursive_mutex> lock(writers);
        return model;
    }
    BrowserOscillatorExtraConfiguration &operator=(const OscillatorExtraConfiguration &value)
    {
        auto transaction = edit();
        *transaction.operator->() = value;
        return *this;
    }
    // One serialized audio consumer. No writer lock, allocation or retry.
    const OscillatorExtraConfiguration &audioValue()
    {
        snapshot.consume(audio);
        return audio;
    }
};
