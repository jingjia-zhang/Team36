#ifndef H264_RECORDER_H_
#define H264_RECORDER_H_

#include <atomic>

#include "codecs/h264/openh264_encoder.h"
#include "codecs/v4l2/v4l2_decoder.h"
#include "codecs/v4l2/v4l2_encoder.h"
#include "recorder/video_recorder.h"

class H264Recorder : public VideoRecorder {
  public:
    static std::unique_ptr<H264Recorder> Create(Args config);
    H264Recorder(Args config, std::string encoder_name);
    ~H264Recorder();
    void PreStart() override;
    void PostStop() override;

  protected:
    void Encode(rtc::scoped_refptr<V4L2FrameBuffer> frame_buffer) override;

  private:
    std::mutex mutex_;
    std::unique_ptr<V4L2Decoder> decoder_;
    std::unique_ptr<V4L2Encoder> encoder_;
    std::unique_ptr<Openh264Encoder> sw_encoder_;

    void InitCodecs();
};

#endif // H264_RECORDER_H_
