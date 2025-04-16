#include "args.h"
#include "capturer/v4l2_capturer.h"
#include "codecs/v4l2/v4l2_scaler.h"

#include <condition_variable>
#include <fcntl.h>
#include <mutex>
#include <string>
#include <unistd.h>

void WriteImage(V4L2Buffer buffer, int index) {
    printf("Dequeued buffer index: %d\n"
           "  bytesused: %d\n",
           index, buffer.length);

    std::string filename = "img" + std::to_string(index) + ".yuv";
    int outfd = open(filename.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
    if ((outfd == -1) && (EEXIST == errno)) {
        /* open the existing file with write flag */
        outfd = open(filename.c_str(), O_WRONLY);
    }

    write(outfd, buffer.start, buffer.length);
}

int main(int argc, char *argv[]) {
    std::mutex mtx;
    std::condition_variable cond_var;
    bool is_finished = false;
    int images_nb = 0;
    int record_sec = 1;
    Args args{.fps = 15, .width = 640, .height = 480, .format = V4L2_PIX_FMT_YUV420};

    auto scaler = V4L2Scaler::Create(args.width, args.height, 320, 240, false, false);

    auto capturer = V4L2Capturer::Create(args);
    auto observer = capturer->AsRawBufferObservable();
    observer->Subscribe([&](V4L2Buffer buffer) {
        scaler->EmplaceBuffer(buffer, [&](V4L2Buffer scaled_buffer) {
            if (is_finished) {
                return;
            }

            if (images_nb++ < args.fps * record_sec) {
                WriteImage(scaled_buffer, images_nb);
            } else {
                is_finished = true;
                cond_var.notify_all();
            }
        });
    });

    std::unique_lock<std::mutex> lock(mtx);
    cond_var.wait(lock, [&] {
        return is_finished;
    });

    scaler.reset();
    observer->UnSubscribe();

    return 0;
}
