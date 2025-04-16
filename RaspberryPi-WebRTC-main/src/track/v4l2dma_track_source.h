#ifndef V4L2DMA_TRACK_SOURCE_H_
#define V4L2DMA_TRACK_SOURCE_H_

#include "codecs/v4l2/v4l2_scaler.h"
#include "track/scale_track_source.h"

class V4L2DmaTrackSource : public ScaleTrackSource {
  public:
    static rtc::scoped_refptr<V4L2DmaTrackSource> Create(std::shared_ptr<VideoCapturer> capturer);
    V4L2DmaTrackSource(std::shared_ptr<VideoCapturer> capturer);
    ~V4L2DmaTrackSource();
    void StartTrack() override;

  private:
    bool is_dma_src_;
    int config_width_;
    int config_height_;
    std::unique_ptr<V4L2Scaler> scaler;

    void OnFrameCaptured(V4L2Buffer buffer);
};

#endif
