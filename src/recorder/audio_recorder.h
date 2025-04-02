#ifndef AUDIO_RECORDER_H_
#define AUDIO_RECORDER_H_

#include <condition_variable>
#include <mutex>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/audio_fifo.h>
}

#include "capturer/pa_capturer.h"
#include "common/logging.h"
#include "recorder/recorder.h"

class ThreadSafeAudioFifo {
  public:
    void alloc(enum AVSampleFormat sample_fmt, int channels, int nb_samples) {
        fifo_ = av_audio_fifo_alloc(sample_fmt, channels, nb_samples);
        if (fifo_ == nullptr) {
            DEBUG_PRINT("Failed to initialize audio fifo buffer.");
        }
    }

    int write(void **data, int nb_samples) {
        std::lock_guard<std::mutex> lock(mutex_);
        return av_audio_fifo_write(fifo_, data, nb_samples);
    }

    int read(void **data, int nb_samples) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (av_audio_fifo_size(fifo_) < nb_samples) {
            return 0;
        }
        return av_audio_fifo_read(fifo_, data, nb_samples);
    }

    int size() {
        std::lock_guard<std::mutex> lock(mutex_);
        return av_audio_fifo_size(fifo_);
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        av_audio_fifo_reset(fifo_);
    }

  private:
    AVAudioFifo *fifo_;
    std::mutex mutex_;
};

class AudioRecorder : public Recorder<PaBuffer> {
  public:
    static std::unique_ptr<AudioRecorder> Create(Args config);
    AudioRecorder(Args config);
    ~AudioRecorder();
    void OnBuffer(PaBuffer &buffer) override;
    void PreStart() override;

  private:
    int sample_rate;
    int channels = 2;
    int frame_size;
    unsigned int frame_count;
    std::string encoder_name;
    ThreadSafeAudioFifo fifo_buffer;
    AVSampleFormat sample_fmt;
    AVFrame *frame;

    void Encode();
    void InitializeFrame();
    void InitializeFifoBuffer();
    void InitializeEncoderCtx(AVCodecContext *&encoder) override;
    bool ConsumeBuffer() override;
};

#endif // AUDIO_RECORDER_H_
