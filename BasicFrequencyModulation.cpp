// Karl Yerkes
// 2023-01-25
// MAT 240B ~ Audio Programming
// Basic Frequency Modulation
//

#include <juce_audio_processors/juce_audio_processors.h>

template <typename T>
T mtof(T m) {
  return T(440) * pow(T(2), (m - T(69)) / T(12));
}
template <typename T>
T dbtoa(T db) {
  return pow(T(10), db / T(20));
}

// valid on (-1, 1)
template <class T>
inline T sine(T n) {
  T nn = n * n;
  return n * (T(3.138982) +
              nn * (T(-5.133625) + nn * (T(2.428288) - nn * T(0.433645))));
}

static float softclip(float x) {
  if (x >= 1.0f) return 1.0f;
  if (x <= -1.0f) return -1.0f;
  return (3 * x - x * x * x) / 2;
}

struct Phasor {};

template <class T>
inline T wrap(T v, T hi, T lo) {
  if (lo == hi) return lo;

  // if(v >= hi){
  if (!(v < hi)) {
    T diff = hi - lo;
    v -= diff;
    if (!(v < hi)) v -= diff * (T)(unsigned)((v - lo) / diff);
  } else if (v < lo) {
    T diff = hi - lo;
    v += diff;  // this might give diff if range is too large, so check at end
                // of block...
    if (v < lo) v += diff * (T)(unsigned)(((lo - v) / diff) + 1);
    if (v == diff) return std::nextafter(v, lo);
  }
  return v;
}

struct Cycle {  // like max's cycle~ object
  float phase = 0;
  // functor ~ overload the "call operator"
  float operator()(float hertz) {
    float value = sine(phase);  // compute output sample

    // side effects
    phase += 2 * hertz / 48000;
    phase = wrap(phase, 1.0f, -1.0f);
    // if (phase >= 1) {
    //   phase -= 2;
    // } else if (phase <= -1) {
    //   phase += 2;
    // }

    return value;  // return output sample
  }
};

using namespace juce;

// http://scp.web.elte.hu/papers/synthesis1.pdf

struct QuasiBandLimited : public AudioProcessor {
  AudioParameterFloat* gain;
  AudioParameterFloat* note;
  AudioParameterFloat* mod;
  AudioParameterFloat* depth;
  AudioParameterFloat* drive;
  Cycle carrier, modulator;
  /// add parameters here ///////////////////////////////////////////////////
  /// add your objects here /////////////////////////////////////////////////

  QuasiBandLimited()
      : AudioProcessor(BusesProperties()
                           .withInput("Input", AudioChannelSet::stereo())
                           .withOutput("Output", AudioChannelSet::stereo())) {
    addParameter(gain = new AudioParameterFloat(
                     {"gain", 1}, "Gain",
                     NormalisableRange<float>(-65, -1, 0.01f), -65));
    addParameter(
        note = new AudioParameterFloat(
            {"note", 1}, "Note", NormalisableRange<float>(-2, 129, 0.01f), 40));
    addParameter(
        mod = new AudioParameterFloat(
            {"mod", 1}, "Mod", NormalisableRange<float>(-27, 100, 0.01f), -27));
    addParameter(
        depth = new AudioParameterFloat(
            {"depth", 1}, "Depth", NormalisableRange<float>(0, 127, 0.01f), 0));
    addParameter(drive = new AudioParameterFloat(
                     {"drive", 1}, "Drive",
                     NormalisableRange<float>(0, 0.5, 0.001f), 0.1f));
    /// add parameters here /////////////////////////////////////////////////
  }

  /// this function handles the audio ///////////////////////////////////////
  // (16, 32, 64 ... 512, 1024 ... 4096) / samplerate
  float t = 0;
  void processBlock(AudioBuffer<float>& buffer, MidiBuffer&) override {
    buffer.clear();
    auto left = buffer.getWritePointer(0, 0);
    auto right = buffer.getWritePointer(1, 0);
    for (int i = 0; i < buffer.getNumSamples(); ++i) {
      float A = dbtoa(gain->get());
      float alpha = mtof(note->get());
      float index = dbtoa(depth->get());
      float beta = mtof(mod->get());
      float k = drive->get();
      left[i] = right[i] =
          A * carrier(alpha + index * softclip(k * modulator(beta)));
    }
  }

  /// start and shutdown callbacks///////////////////////////////////////////
  void prepareToPlay(double, int) override {}
  void releaseResources() override {}

  /// maintaining persistant state on suspend ///////////////////////////////
  void getStateInformation(MemoryBlock& destData) override {
    MemoryOutputStream(destData, true).writeFloat(*gain);
    /// add parameters here /////////////////////////////////////////////////
  }

  void setStateInformation(const void* data, int sizeInBytes) override {
    gain->setValueNotifyingHost(
        MemoryInputStream(data, static_cast<size_t>(sizeInBytes), false)
            .readFloat());
    /// add parameters here /////////////////////////////////////////////////
  }

  /// do not change anything below this line, probably //////////////////////

  /// general configuration /////////////////////////////////////////////////
  const String getName() const override { return "Quasi Band Limited"; }
  double getTailLengthSeconds() const override { return 0; }
  bool acceptsMidi() const override { return false; }
  bool producesMidi() const override { return false; }

  /// for handling presets //////////////////////////////////////////////////
  int getNumPrograms() override { return 1; }
  int getCurrentProgram() override { return 0; }
  void setCurrentProgram(int) override {}
  const String getProgramName(int) override { return "None"; }
  void changeProgramName(int, const String&) override {}

  /// ?????? ////////////////////////////////////////////////////////////////
  bool isBusesLayoutSupported(const BusesLayout& layouts) const override {
    const auto& mainInLayout = layouts.getChannelSet(true, 0);
    const auto& mainOutLayout = layouts.getChannelSet(false, 0);

    return (mainInLayout == mainOutLayout && (!mainInLayout.isDisabled()));
  }

  /// automagic user interface //////////////////////////////////////////////
  AudioProcessorEditor* createEditor() override {
    return new GenericAudioProcessorEditor(*this);
  }
  bool hasEditor() const override { return true; }

 private:
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(QuasiBandLimited)
};

AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
  return new QuasiBandLimited();
}