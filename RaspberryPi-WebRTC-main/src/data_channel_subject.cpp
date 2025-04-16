#include "data_channel_subject.h"

#include "common/logging.h"

const int CHUNK_SIZE = 65536;

DataChannelSubject::~DataChannelSubject() {
    UnSubscribe();
    data_channel_->UnregisterObserver();
    data_channel_->Close();
}

void DataChannelSubject::OnStateChange() {
    webrtc::DataChannelInterface::DataState state = data_channel_->state();
    DEBUG_PRINT("OnStateChange => %s", webrtc::DataChannelInterface::DataStateString(state));
}

void DataChannelSubject::OnMessage(const webrtc::DataBuffer &buffer) {
    const uint8_t *data = buffer.data.data<uint8_t>();
    size_t length = buffer.data.size();
    std::string message(reinterpret_cast<const char *>(data), length);

    Next(message);
}

void DataChannelSubject::Next(std::string message) {
    try {
        json jsonObj = json::parse(message.c_str());

        std::string jsonStr = jsonObj.dump();
        DEBUG_PRINT("Receive message => %s", jsonStr.c_str());

        CommandType type = jsonObj["type"];
        std::string content = jsonObj["message"];
        if (content.empty()) {
            return;
        }
        observers_ = observers_map_[type];
        observers_.insert(observers_.end(), observers_map_[CommandType::UNKNOWN].begin(),
                          observers_map_[CommandType::UNKNOWN].end());

        for (auto &observer : observers_) {
            if (observer->subscribed_func_ != nullptr) {
                observer->subscribed_func_(content);
            }
        }
    } catch (const json::parse_error &e) {
        ERROR_PRINT("JSON parse error, %s, occur at position: %lu", e.what(), e.byte);
    }
}

std::shared_ptr<Observable<std::string>> DataChannelSubject::AsObservable() {
    auto observer = std::make_shared<Observable<std::string>>();
    observers_map_[CommandType::UNKNOWN].push_back(observer);
    return observer;
}

std::shared_ptr<Observable<std::string>> DataChannelSubject::AsObservable(CommandType type) {
    auto observer = std::make_shared<Observable<std::string>>();
    observers_map_[type].push_back(observer);
    return observer;
}

void DataChannelSubject::UnSubscribe() {
    for (auto &[type, observers] : observers_map_) {
        observers.clear();
    }
}

void DataChannelSubject::Send(CommandType type, const uint8_t *data, size_t size) {
    int bytes_read = 0;
    const size_t header_size = sizeof(CommandType);

    std::vector<char> data_with_header(CHUNK_SIZE);

    if (size == 0) {
        std::memcpy(data_with_header.data(), &type, header_size);
        Send((uint8_t *)data_with_header.data(), header_size);
        return;
    }

    while (bytes_read < size) {
        if (data_channel_->buffered_amount() + CHUNK_SIZE > data_channel_->MaxSendQueueSize()) {
            usleep(100);
            DEBUG_PRINT("Sleeping for 100 microsecond due to MaxSendQueueSize reached.");
            continue;
        }
        int read_size = std::min(CHUNK_SIZE - header_size, size - bytes_read);

        std::memcpy(data_with_header.data(), &type, header_size);
        std::memcpy(data_with_header.data() + header_size, data + bytes_read, read_size);

        Send((uint8_t *)data_with_header.data(), read_size + header_size);
        bytes_read += read_size;
    }
}

void DataChannelSubject::Send(const uint8_t *data, size_t size) {
    if (data_channel_->state() != webrtc::DataChannelInterface::kOpen) {
        return;
    }
    rtc::CopyOnWriteBuffer buffer(data, size);
    webrtc::DataBuffer data_buffer(buffer, true);
    data_channel_->Send(data_buffer);
}

void DataChannelSubject::Send(MetaMessage metadata) {
    auto type = CommandType::METADATA;
    auto body = metadata.ToString();
    int body_size = body.length();
    auto header = std::to_string(body_size);
    int header_size = header.length();

    Send(type, (uint8_t *)header.c_str(), header_size);
    Send(type, (uint8_t *)body.c_str(), body_size);
    Send(type, nullptr, 0);
}

void DataChannelSubject::Send(Buffer image) {
    const int file_size = image.length;
    auto type = CommandType::SNAPSHOT;
    std::string size_str = std::to_string(file_size);
    Send(type, (uint8_t *)size_str.c_str(), size_str.length());
    Send(type, (uint8_t *)image.start.get(), file_size);
    Send(type, nullptr, 0);

    DEBUG_PRINT("Image sent: %d bytes", file_size);
}

void DataChannelSubject::Send(std::ifstream &file) {
    std::vector<char> buffer(CHUNK_SIZE);
    int bytes_read = 0;
    int count = 0;
    const int header_size = sizeof(CommandType);
    auto type = CommandType::RECORD;

    int file_size = file.tellg();
    std::string size_str = std::to_string(file_size);
    file.seekg(0, std::ios::beg);

    Send(type, (uint8_t *)size_str.c_str(), size_str.length());

    while (bytes_read < file_size) {
        if (data_channel_->buffered_amount() + CHUNK_SIZE > data_channel_->MaxSendQueueSize()) {
            sleep(1);
            DEBUG_PRINT("Sleeping for 1 second due to MaxSendQueueSize reached.");
            continue;
        }
        int read_size = std::min(CHUNK_SIZE - header_size, file_size - bytes_read);
        std::memcpy(buffer.data(), &type, header_size);
        file.read(buffer.data() + header_size, read_size);

        Send((uint8_t *)buffer.data(), read_size + header_size);
        bytes_read += read_size;
    }

    Send(type, nullptr, 0);
}

void DataChannelSubject::SetDataChannel(
    rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) {
    data_channel_ = data_channel;
    data_channel_->RegisterObserver(this);
}
