// host_probe -- TD-001 repro harness: load the REAL "The Dreamer.vst3" in a
// minimal JUCE VST3 host and render long offline sessions, measuring output.
// Offline processing is unclocked (runs ~30-60x realtime), so long wall-clock
// scenarios are cheap. 10 tests with growing durations 12s..120s, each from a
// fresh prepareToPlay (a fresh "session"), in two variants:
//   idle : one 1 s note at t=0, then silence to the end (the reported repro)
//   held : note held for the whole duration ("use")
// Reports per-block peak evolution, first >1.0 block, first non-finite block.
//
// THROWAWAY diagnostic (dsp-pass step 2). Not part of the validator matrix.

#include <juce_audio_processors/juce_audio_processors.h>
#include <cstdio>
#include <thread>
#include <atomic>

using namespace juce;

static double kSR  = 44100.0;     // overridable via argv[2]
static constexpr int kBlk = 512;

struct TestResult {
    float  globalPeak = 0.0f;
    double firstOver  = -1.0;
    double firstNaN   = -1.0;
};

static TestResult renderTest(AudioPluginInstance& p, int durationSec, bool hold)
{
    p.releaseResources();
    p.setNonRealtime(true);                    // offline render (unclocked)
    p.prepareToPlay(kSR, kBlk);

    AudioBuffer<float> buf(2, kBlk);
    MidiBuffer midi;

    const int totalBlocks  = (int)(kSR * durationSec) / kBlk;
    const int noteOffBlock = hold ? -1 : (int)kSR / kBlk;   // release at 1 s
    TestResult res;
    float secPeak = 0.0f;
    const int oneSec = (int)kSR / kBlk;

    for (int b = 0; b < totalBlocks; ++b) {
        midi.clear();
        if (b == 0)            midi.addEvent(MidiMessage::noteOn (1, 60, 0.8f), 0);
        if (b == noteOffBlock) midi.addEvent(MidiMessage::noteOff(1, 60), 0);
        buf.clear();
        p.processBlock(buf, midi);

        float blkPeak = 0.0f; bool finite = true;
        for (int ch = 0; ch < 2; ++ch) {
            const float* d = buf.getReadPointer(ch);
            for (int i = 0; i < kBlk; ++i) {
                if (!std::isfinite(d[i])) finite = false;
                blkPeak = jmax(blkPeak, std::abs(d[i]));
            }
        }
        const double t = (double)b * kBlk / kSR;
        if (!finite       && res.firstNaN  < 0) res.firstNaN  = t;
        if (blkPeak > 1.0f && res.firstOver < 0) res.firstOver = t;
        res.globalPeak = jmax(res.globalPeak, blkPeak);
        secPeak = jmax(secPeak, blkPeak);
        if ((b + 1) % oneSec == 0) {
            const int sec = (b + 1) / oneSec;
            if (secPeak > 0.99f)
                std::printf("      t=%3ds peak=%.5f  <-- HOT\n", sec, secPeak);
            secPeak = 0.0f;
        }
    }
    return res;
}

int main (int argc, char* argv[])
{
    ScopedJuceInitialiser_GUI juceInit;        // message loop for instantiation

    const String pluginPath = argc > 1 ? String(argv[1])
                                       : String("C:\\the-dreamer\\The Dreamer.vst3");
    if (argc > 2) kSR = String(argv[2]).getDoubleValue();
    // argv[3]: phase filter ("all" | "hammer"); argv[4]: writer param-name
    // substring filter, comma-separated ("none" = program changes only)
    const String phase  = argc > 3 ? String(argv[3]).toLowerCase() : String("all");
    const String wfilt  = argc > 4 ? String(argv[4]) : String();
    std::printf("host_probe: loading %s @ %.0f Hz (phase=%s writer='%s')\n",
                pluginPath.toRawUTF8(), kSR, phase.toRawUTF8(), wfilt.toRawUTF8());

    VST3PluginFormat format;
    OwnedArray<PluginDescription> descs;
    format.findAllTypesForFile(descs, pluginPath);
    if (descs.isEmpty()) { std::printf("FATAL: no VST3 found\n"); return 2; }
    std::printf("  found: %s %s (%s)\n", descs[0]->name.toRawUTF8(),
                descs[0]->version.toRawUTF8(), descs[0]->manufacturerName.toRawUTF8());

    String err;
    auto inst = format.createInstanceFromDescription(*descs[0], kSR, kBlk, err);
    if (inst == nullptr) { std::printf("FATAL: instantiate failed: %s\n", err.toRawUTF8()); return 2; }
    std::printf("  instantiated. params=%d\n\n", inst->getParameters().size());

    int faults = 0;
    const bool runAll = phase == "all";
    for (int variant = 0; runAll && variant < 2; ++variant) {
        const bool hold = variant == 1;
        std::printf("---- variant: %s ----\n", hold ? "HELD (use)" : "IDLE (1s note, then silence)");
        for (int test = 1; test <= 10; ++test) {
            const int dur = 12 * test;                       // 12s .. 120s
            std::printf("  [%2d] %3ds %s : ", test, dur, hold ? "held" : "idle");
            std::fflush(stdout);
            const auto r = renderTest(*inst, dur, hold);
            const bool fault = r.firstOver >= 0 || r.firstNaN >= 0;
            faults += fault ? 1 : 0;
            std::printf("peak=%.5f over1.0=%.2fs NaN=%.2fs%s\n",
                        r.globalPeak, r.firstOver, r.firstNaN,
                        fault ? "   <== FAULT" : "");
        }
    }
    // ---- phase 3: factory-preset sweep (the user's real "use" state) -------
    // Fresh-instance defaults are INIT-like (all FX off), so phases 1-2 miss
    // preset FX configurations entirely. 30 s idle render per program.
    const int numProgs = inst->getNumPrograms();
    if (runAll || phase == "presets") {
    std::printf("\n---- variant: PRESET sweep (%d programs, 30s idle each) ----\n", numProgs);
    for (int prog = 0; prog < numProgs; ++prog) {
        inst->setCurrentProgram(prog);
        const auto r = renderTest(*inst, 30, false);
        const bool fault = r.firstOver >= 0 || r.firstNaN >= 0;
        faults += fault ? 1 : 0;
        if (fault || r.globalPeak > 0.99f)
            std::printf("  [prog %2d] %-28s peak=%.5f over1.0=%.2fs NaN=%.2fs%s\n",
                        prog, inst->getProgramName(prog).toRawUTF8(),
                        r.globalPeak, r.firstOver, r.firstNaN,
                        fault ? "   <== FAULT" : "");
    }
    std::printf("  (quiet programs suppressed; only >0.99 peaks / faults shown)\n");
    }   // runAll

    // ---- phase 4: LIVE-USE fuzz -- params move WHILE audio renders ---------
    // Cubase "use" = knob moves / automation / preset steps / retriggers with
    // sound running. Static renders (phases 1-3) never mutate state mid-stream.
    // Seeded RNG -> reproducible. Every ~4 blocks nudge a random param; every
    // ~2 s retrigger a note; every ~10 s step the program. 120 s x 4 seeds.
    auto& params = inst->getParameters();
    if (runAll) {
    std::printf("\n---- variant: LIVE-USE fuzz (param moves + retriggers + program steps) ----\n");
    for (uint32_t seed = 1; seed <= 4; ++seed) {
        Random rng((int64)seed * 0x9E3779B9u);
        inst->setCurrentProgram(0);
        inst->releaseResources();
        inst->setNonRealtime(true);
        inst->prepareToPlay(kSR, kBlk);

        AudioBuffer<float> buf(2, kBlk);
        MidiBuffer midi;
        const int totalBlocks = (int)(kSR * 120.0) / kBlk;
        const int oneSec = (int)kSR / kBlk;
        float peak = 0.0f; double firstOver = -1.0, firstNaN = -1.0;
        int lastNote = -1;

        for (int b = 0; b < totalBlocks; ++b) {
            // random param nudges (skip the Program meta-param at index 0 if any)
            if (rng.nextInt(4) == 0 && !params.isEmpty()) {
                auto* p = params[rng.nextInt(params.size())];
                p->setValue(rng.nextFloat());
            }
            midi.clear();
            if (b % (2 * oneSec) == 0) {                       // retrigger every 2 s
                if (lastNote >= 0) midi.addEvent(MidiMessage::noteOff(1, lastNote), 0);
                lastNote = 48 + rng.nextInt(24);
                midi.addEvent(MidiMessage::noteOn(1, lastNote, 0.8f), 4);
            }
            if (b % (10 * oneSec) == oneSec)                   // program step every 10 s
                inst->setCurrentProgram(rng.nextInt(jmax(1, numProgs)));

            buf.clear();
            inst->processBlock(buf, midi);

            float blkPeak = 0.0f; bool finite = true;
            for (int ch = 0; ch < 2; ++ch) {
                const float* d = buf.getReadPointer(ch);
                for (int i = 0; i < kBlk; ++i) {
                    if (!std::isfinite(d[i])) finite = false;
                    blkPeak = jmax(blkPeak, std::abs(d[i]));
                }
            }
            const double t = (double)b * kBlk / kSR;
            if (!finite        && firstNaN  < 0) firstNaN  = t;
            if (blkPeak > 1.0f && firstOver < 0) firstOver = t;
            peak = jmax(peak, blkPeak);
        }
        const bool fault = firstOver >= 0 || firstNaN >= 0;
        faults += fault ? 1 : 0;
        std::printf("  [seed %u] 120s fuzz : peak=%.5f over1.0=%.2fs NaN=%.2fs%s\n",
                    seed, peak, firstOver, firstNaN, fault ? "   <== FAULT" : "");
    }

    }   // runAll (phase 4)

    // ---- phase 5: VARIABLE-BLOCK fuzz --------------------------------------
    if (runAll) {
    // Cubase never delivers a steady 512: loop points, automation nodes and
    // ASIO-Guard hand the plugin ragged block sizes (<= prepared). A stage
    // that caches the prepared size while numSamples varies corrupts slowly.
    std::printf("\n---- variant: VARIABLE-BLOCK fuzz (ragged numSamples 1..%d) ----\n", kBlk);
    for (uint32_t seed = 1; seed <= 4; ++seed) {
        Random rng((int64)seed * 0x51ED2701u);
        inst->setCurrentProgram(0);
        inst->releaseResources();
        inst->setNonRealtime(true);
        inst->prepareToPlay(kSR, kBlk);

        AudioBuffer<float> store(2, kBlk);
        MidiBuffer midi;
        const long totalSamples = (long)(kSR * 120.0);
        long done = 0; int sinceNote = 0; int lastNote = -1;
        float peak = 0.0f; double firstOver = -1.0, firstNaN = -1.0;

        while (done < totalSamples) {
            const int n = 1 + rng.nextInt(kBlk);              // ragged 1..512
            if (rng.nextInt(4) == 0 && !params.isEmpty())
                params[rng.nextInt(params.size())]->setValue(rng.nextFloat());
            midi.clear();
            sinceNote += n;
            if (sinceNote >= (int)(2 * kSR)) {                // retrigger ~2 s
                sinceNote = 0;
                if (lastNote >= 0) midi.addEvent(MidiMessage::noteOff(1, lastNote), 0);
                lastNote = 48 + rng.nextInt(24);
                midi.addEvent(MidiMessage::noteOn(1, lastNote, 0.8f), 0);
            }
            float* chans[2] = { store.getWritePointer(0), store.getWritePointer(1) };
            AudioBuffer<float> view(chans, 2, n);             // no alloc, ragged len
            view.clear();
            inst->processBlock(view, midi);

            bool finite = true; float blkPeak = 0.0f;
            for (int ch = 0; ch < 2; ++ch) {
                const float* d = view.getReadPointer(ch);
                for (int i = 0; i < n; ++i) {
                    if (!std::isfinite(d[i])) finite = false;
                    blkPeak = jmax(blkPeak, std::abs(d[i]));
                }
            }
            const double t = (double)done / kSR;
            if (!finite        && firstNaN  < 0) firstNaN  = t;
            if (blkPeak > 1.0f && firstOver < 0) firstOver = t;
            peak = jmax(peak, blkPeak);
            done += n;
        }
        const bool fault = firstOver >= 0 || firstNaN >= 0;
        faults += fault ? 1 : 0;
        std::printf("  [seed %u] 120s ragged : peak=%.5f over1.0=%.2fs NaN=%.2fs%s\n",
                    seed, peak, firstOver, firstNaN, fault ? "   <== FAULT" : "");
    }

    }   // runAll (phase 5)

    // ---- phase 6: CONCURRENT writer hammer ---------------------------------
    // The real Cubase shape: message/automation threads write params and step
    // programs WHILE the audio thread is inside processBlock. All crossings
    // are supposed to be atomics/SPSC -- prove it under load.
    // Optional writer param filter: comma-separated name substrings, "none" =
    // program changes only. Bisection lever for WHICH family racing faults.
    if (runAll || phase == "hammer") {
    Array<AudioProcessorParameter*> wparams;
    if (wfilt != "none") {
        StringArray subs = StringArray::fromTokens(wfilt, ",", "");
        for (auto* p : params) {
            if (wfilt.isEmpty()) { wparams.add(p); continue; }
            const String nm = p->getName(64).toLowerCase();
            for (const auto& s : subs)
                if (s.isNotEmpty() && nm.contains(s.toLowerCase())) { wparams.add(p); break; }
        }
    }
    std::printf("\n---- variant: CONCURRENT hammer (writer thread vs processBlock, 120s) ----\n");
    std::printf("  writer params: %d of %d%s\n", wparams.size(), params.size(),
                wfilt == "none" ? " (program changes only)" : "");
    for (uint32_t seed = 1; seed <= 2; ++seed) {
        inst->setCurrentProgram(0);
        inst->releaseResources();
        inst->setNonRealtime(true);
        inst->prepareToPlay(kSR, kBlk);

        std::atomic<bool> stop{false};
        std::thread writer([&] {
            Random wr((int64)seed * 0xC0FFEEu);
            while (!stop.load(std::memory_order_relaxed)) {
                if (!wparams.isEmpty())
                    wparams[wr.nextInt(wparams.size())]->setValue(wr.nextFloat());
                if ((wfilt == "none" || wfilt.isEmpty()) && wr.nextInt(64) == 0)
                    inst->setCurrentProgram(wr.nextInt(jmax(1, numProgs)));
                std::this_thread::yield();
            }
        });

        AudioBuffer<float> buf(2, kBlk);
        MidiBuffer midi;
        Random rng((int64)seed * 0xBADC0DEu);
        const int totalBlocks = (int)(kSR * 120.0) / kBlk;
        const int oneSec = (int)kSR / kBlk;
        float peak = 0.0f; double firstOver = -1.0, firstNaN = -1.0;
        int lastNote = -1;
        float bucket[12] = {};                 // per-10s max (sustain trace)
        for (int b = 0; b < totalBlocks; ++b) {
            midi.clear();
            if (b % (2 * oneSec) == 0) {
                if (lastNote >= 0) midi.addEvent(MidiMessage::noteOff(1, lastNote), 0);
                lastNote = 48 + rng.nextInt(24);
                midi.addEvent(MidiMessage::noteOn(1, lastNote, 0.8f), 0);
            }
            buf.clear();
            inst->processBlock(buf, midi);
            bool finite = true; float blkPeak = 0.0f;
            for (int ch = 0; ch < 2; ++ch) {
                const float* d = buf.getReadPointer(ch);
                for (int i = 0; i < kBlk; ++i) {
                    if (!std::isfinite(d[i])) finite = false;
                    blkPeak = jmax(blkPeak, std::abs(d[i]));
                }
            }
            const double t = (double)b * kBlk / kSR;
            if (!finite        && firstNaN  < 0) firstNaN  = t;
            if (blkPeak > 1.0f && firstOver < 0) firstOver = t;
            peak = jmax(peak, blkPeak);
            const int bk = jmin(11, (int)(t / 10.0));
            bucket[bk] = jmax(bucket[bk], blkPeak);
        }
        stop.store(true);
        writer.join();
        const bool fault = firstOver >= 0 || firstNaN >= 0;
        faults += fault ? 1 : 0;
        std::printf("  [seed %u] 120s hammer : peak=%.5g over1.0=%.2fs NaN=%.2fs%s\n",
                    seed, peak, firstOver, firstNaN, fault ? "   <== FAULT" : "");
        std::printf("           10s-max trace:");
        for (int i = 0; i < 12; ++i) std::printf(" %.3g", bucket[i]);
        std::printf("\n");
    }
    }   // runAll || hammer

    std::printf("\nhost_probe done: %d faulting test(s)\n", faults);
    return faults > 0 ? 1 : 0;
}
