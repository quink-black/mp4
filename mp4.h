#include <assert.h>

#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace mov {

static constexpr uint16_t buf2UInt16(uint8_t *buf) {
    return (uint16_t)buf[0] << 8U | buf[1];
}

static constexpr uint32_t buf2UInt32(uint8_t *buf) {
    return (uint32_t)buf[0] << 24U | (uint32_t)buf[1] << 16U |
           (uint32_t)buf[2] << 8U | buf[3];
}

static constexpr uint64_t buf2UInt64(uint8_t *buf) {
    uint64_t high = buf2UInt32(buf);
    int64_t low = buf2UInt32(buf + 4);
    return high + low;
}

static std::string sec2Str(int64_t sec) {
    std::ostringstream ss;
    if (sec > 3600) {
        ss << sec / 3600 << " hour, ";
    }
    if (sec > 60) {
        ss << (sec % 3600) / 60 << " min, ";
    }
    ss << sec % 60 << " sec";
    return ss.str();
}

class FileOp {
public:
    FileOp(const std::string &path) : path_(path) {}

    ~FileOp() {
        if (file_) {
            fclose(file_);
        }
    }

    bool open(const std::string &mode) {
        file_ = fopen(path_.c_str(), mode.c_str());
        return file_ != nullptr;
    }

    size_t read(void *ptr, size_t size, size_t nitems) {
        return fread(ptr, size, nitems, file_);
    }

    off_t tell() { return ftello(file_); }

    int seek(off_t offset, int whence) { return fseeko(file_, offset, whence); }

    std::optional<uint16_t> readAsBigU16() {
        uint8_t buf[2];
        if (!read(buf, sizeof(buf), 1)) {
            return std::optional<uint16_t>();
        }
        return std::optional<uint16_t>(buf2UInt16(buf));
    }

    std::optional<uint32_t> readAsBigU32() {
        uint8_t buf[4];
        if (!read(buf, sizeof(buf), 1)) {
            return std::optional<uint32_t>();
        }
        return std::optional<uint32_t>(buf2UInt32(buf));
    }

    std::optional<uint64_t> readAsBigU64() {
        uint8_t buf[8];
        if (!read(buf, sizeof(buf), 1)) {
            return std::optional<uint64_t>();
        }
        return std::optional<uint64_t>(buf2UInt64(buf));
    }

private:
    std::string path_;
    FILE *file_;
};

class Box;

std::shared_ptr<Box> toDetailType(std::unique_ptr<Box> base);

class Box {
public:
    using ExtendedType = std::array<char, 16>;
    using Boxes = std::vector<std::shared_ptr<Box>>;

    class BoxBuilder {
    public:
        uint64_t size_ = 0;
        uint64_t offset_ = 0;
        uint32_t type_{};
        ExtendedType extended_type_{};

        std::unique_ptr<Box> build() {
            return std::make_unique<Box>(size_, offset_, type_, extended_type_);
        }
    };

    Box(uint64_t size, uint64_t offset, uint32_t type,
        ExtendedType extended_type)
        : size_(size),
          offset_(offset),
          type_(type),
          extended_type_(extended_type) {}

    static constexpr uint32_t str2BoxType(const char (&buf)[5]) {
        return (uint32_t)buf[3] << 24U | (uint32_t)buf[2] << 16U |
               (uint32_t)buf[1] << 8U | buf[0];
    }

    static std::string boxType2Str(uint32_t baseType) {
        char *p = reinterpret_cast<char *>(&baseType);
        return std::string(p, 4);
    }

    std::string boxTypeStr() { return boxType2Str(type_); }

    static std::unique_ptr<Box> parseBasic(FileOp &in) {
        BoxBuilder builder;
        builder.offset_ = in.tell();

        auto size = in.readAsBigU32();
        if (size.has_value()) {
            builder.size_ = size.value();
        } else {
            return nullptr;
        }

        if (!in.read(&builder.type_, sizeof(builder.type_), 1)) {
            return nullptr;
        }
        if (builder.size_ == 1) {
            auto large_size = in.readAsBigU64();
            if (large_size.has_value()) {
                builder.size_ = large_size.value();
            } else {
                return nullptr;
            }
        }
        if (builder.type_ == str2BoxType("uuid")) {
            if (!in.read(builder.extended_type_.data(), 16, 1)) {
                return nullptr;
            }
        }
        return builder.build();
    }

    bool parseFullBox(FileOp &file) {
        auto data = file.readAsBigU32();
        if (data.has_value()) {
            fullbox_version_ = data.value() >> 24U;
            fullbox_flag_ = data.value() & 0xFFFFFFU;
            return true;
        } else {
            return false;
        }
    }

    virtual void parseInternal(FileOp &file) {}

    void parseChild(FileOp &file) {
        auto end = offset_ + size_;
        while (file.tell() < end) {
            auto box = parseBasic(file);
            if (box == nullptr) {
                return;
            }
            box->parent_ = this;
            auto detailBox = toDetailType(std::move(box));
            detailBox->parseInternal(file);
            children_.push_back(detailBox);
            file.seek(detailBox->offset() + detailBox->size(), SEEK_SET);
        }
    }

    virtual std::string detail() { return std::string(); }

    uint64_t size() const { return size_; }

    uint64_t offset() const { return offset_; }

    uint32_t baseType() const { return type_; }

    ExtendedType extendedType() const { return extended_type_; }

    const Boxes &children() const { return children_; }

    bool hasChild() { return !children_.empty(); }

    Box *getAncestor(uint32_t type) {
        auto p = parent_;
        while (p) {
            if (p->type_ == type) return p;
            p = p->parent_;
        }
        return nullptr;
    }

protected:
    uint64_t size_ = 0;
    uint64_t offset_ = 0;
    uint32_t type_{};
    ExtendedType extended_type_{};

    uint8_t fullbox_version_ = 0;
    uint32_t fullbox_flag_ = 0;

    Boxes children_;
    Box *parent_ = nullptr;
    friend class Stsd;
};

template <typename T>
static std::shared_ptr<Box> toDetail(Box base) {
    return std::static_pointer_cast<Box>(std::make_shared<T>(std::move(base)));
}

class Ftyp : public Box {
public:
    static const uint32_t tag_ = str2BoxType("ftyp");

    Ftyp(const Box &box) : Box(box) {}

    void parseInternal(FileOp &file) override {
        auto remain = size_ - (file.tell() - offset_);
        if (remain >= 4) {
            file.read(&major_brand_, 4, 1);
            remain -= 4;
        }
        if (remain >= 4) {
            minor_version = file.readAsBigU32().value();
            remain -= 4;
        }
        remain /= 4;
        if (remain > 0) {
            compatible_brands_.resize(remain);
            file.read(compatible_brands_.data(), 4 * remain, 1);
        }
    }

    std::string detail() override {
        std::ostringstream ss;
        ss << "major brand: " << boxType2Str(major_brand_)
           << ", minor version: " << minor_version << ", compatible_brands_: ";
        for (const auto n : compatible_brands_) {
            ss << boxType2Str(n) << " ";
        }
        return ss.str();
    }

private:
    uint32_t major_brand_;
    uint32_t minor_version = 0;
    std::vector<uint32_t> compatible_brands_{};
};

class Mvhd : public Box {
public:
    static const uint32_t tag_ = str2BoxType("mvhd");

    Mvhd(const Box &box) : Box(box) {}

    void parseInternal(FileOp &file) override {
        if (!parseFullBox(file)) {
            std::cerr << "parse mvhd failed\n";
            return;
        }
        if (fullbox_version_ == 1) {
            creation_time_ = file.readAsBigU64().value();
            modification_time_ = file.readAsBigU64().value();
            timescale_ = file.readAsBigU32().value();
            duration_ = file.readAsBigU64().value();
        } else {
            creation_time_ = file.readAsBigU32().value();
            modification_time_ = file.readAsBigU32().value();
            timescale_ = file.readAsBigU32().value();
            duration_ = file.readAsBigU32().value();
        }
        rate_ = (double)file.readAsBigU32().value() / (1U << 16U);
        volume_ = (float)file.readAsBigU16().value() / (1U << 8U);
    }

    std::string detail() override {
        std::ostringstream ss;
        ss << "creation_time: " << creation_time_
           << ", modification_time: " << modification_time_
           << ", timescale: " << timescale_ << ", duration: " << duration_
           << ", " << sec2Str(duration_ / timescale_) << ", rate: " << rate_
           << ", volume: " << volume_;
        return ss.str();
    }

private:
    uint64_t creation_time_ = 0;
    uint64_t modification_time_ = 0;
    uint32_t timescale_ = 0;
    uint64_t duration_ = 0;
    float rate_ = 1.0;
    float volume_ = 1.0;
};

class Tkhd : public Box {
public:
    static const uint32_t tag_ = str2BoxType("tkhd");

    Tkhd(const Box &box) : Box(box) {}

    void parseInternal(FileOp &file) override {
        parseFullBox(file);
        if (fullbox_version_ == 1) {
            creation_time_ = file.readAsBigU64().value();
            modification_time_ = file.readAsBigU64().value();
            track_id_ = file.readAsBigU32().value();
            file.readAsBigU32();
            duration_ = file.readAsBigU64().value();
        } else {
            creation_time_ = file.readAsBigU32().value();
            modification_time_ = file.readAsBigU32().value();
            track_id_ = file.readAsBigU32().value();
            file.readAsBigU32();
            duration_ = file.readAsBigU32().value();
        }
        file.readAsBigU64();
        layer_ = file.readAsBigU16().value();
        alternate_group_ = file.readAsBigU16().value();
        volume_ = file.readAsBigU16().value() / (1 << 8);
        file.seek(2 + 36, SEEK_CUR);
        width_ = file.readAsBigU32().value() / (1 << 16);
        height_ = file.readAsBigU32().value() / (1 << 16);
    }

    std::string detail() override {
        std::ostringstream ss;
        ss << "create: " << creation_time_ << ", modify: " << modification_time_
           << ", id: " << track_id_ << ", dura: " << duration_
           << ", layer: " << layer_ << ", alternate: " << alternate_group_
           << ", volume: " << volume_ << ", width x height: " << width_ << " "
           << height_;
        return ss.str();
    }

private:
    uint64_t creation_time_ = 0;
    uint64_t modification_time_ = 0;
    uint32_t track_id_ = 0;
    uint64_t duration_ = 0;
    int16_t layer_ = 0;
    int16_t alternate_group_ = 0;
    int16_t volume_ = 0;
    uint32_t width_ = 0;
    uint32_t height_ = 0;
};

// Media Header
class Mdhd : public Box {
public:
    static const uint32_t tag_ = str2BoxType("mdhd");

    Mdhd(const Box &box) : Box(box) {}

    void parseInternal(FileOp &file) override {
        parseFullBox(file);
        if (fullbox_version_ == 1) {
            creation_time_ = file.readAsBigU64().value();
            modification_time_ = file.readAsBigU64().value();
            timescale_ = file.readAsBigU32().value();
            duration_ = file.readAsBigU64().value();
        } else {
            creation_time_ = file.readAsBigU32().value();
            modification_time_ = file.readAsBigU32().value();
            timescale_ = file.readAsBigU32().value();
            duration_ = file.readAsBigU32().value();
        }
        uint16_t lang = file.readAsBigU16().value();
        lang_[0] = static_cast<char>((lang >> 10U) + 0x60);
        lang_[1] = static_cast<char>(((lang >> 5U) & 0x1F) + 0x60);
        lang_[2] = static_cast<char>((lang & 0x1F) + 0x60);
    }

    std::string detail() override {
        std::ostringstream ss;
        ss << "create: " << creation_time_ << ", modify: " << modification_time_
           << ", timescale: " << timescale_ << ", dura: " << duration_ << ", "
           << sec2Str(duration_ / timescale_) << ", lang: " << lang_;
        return ss.str();
    }

    uint32_t timescale() { return timescale_; }

private:
    uint64_t creation_time_ = 0;
    uint64_t modification_time_ = 0;
    uint32_t timescale_ = 0;
    uint64_t duration_ = 0;
    char lang_[4] = {};
};

// Handler reference box
class Hdlr : public Box {
public:
    static const uint32_t tag_ = str2BoxType("hdlr");

    Hdlr(const Box &box) : Box(box) {}

    void parseInternal(FileOp &file) override {
        parseFullBox(file);
        file.readAsBigU32();
        file.read(&handler_type_, sizeof(handler_type_), 1);
        file.readAsBigU32();
        file.readAsBigU64();
        auto strSize = offset_ + size_ - file.tell();
        name_.resize(strSize);
        file.read(name_.data(), strSize, 1);
        name_.back() = '\0';
    }

    std::string detail() override {
        std::ostringstream ss;
        ss << "handler type: " << boxType2Str(handler_type_)
           << ", name: " << name_.data();
        return ss.str();
    }

    uint32_t handleType() { return handler_type_; }

private:
    uint32_t handler_type_{};
    std::vector<char> name_;
};

/**
 * Media Information Header: Video
 */
class Vmhd : public Box {
public:
    static const uint32_t tag_ = str2BoxType("vmhd");

    Vmhd(const Box &box) : Box(box) {}

    void parseInternal(FileOp &file) override {
        parseFullBox(file);
        graphics_mode_ = file.readAsBigU16().value();
        opcolor_[0] = file.readAsBigU16().value();
        opcolor_[1] = file.readAsBigU16().value();
        opcolor_[2] = file.readAsBigU16().value();
    }

    std::string detail() override {
        std::ostringstream ss;
        ss << "graphics_mode: " << graphics_mode_
           << ", opcolor: " << opcolor_[0] << ", " << opcolor_[1] << ", "
           << opcolor_[2];
        return ss.str();
    }

private:
    uint16_t graphics_mode_ = 0;
    uint16_t opcolor_[3]{};
};

/**
 * Media Information Header: Sound
 */
class Smhd : public Box {
public:
    static const uint32_t tag_ = str2BoxType("smhd");

    Smhd(const Box &box) : Box(box) {}

    void parseInternal(FileOp &file) override {
        parseFullBox(file);
        balance_ = file.readAsBigU16().value();
    }

    std::string detail() override {
        std::ostringstream ss;
        ss << "balance: " << balance_;
        return ss.str();
    }

private:
    uint16_t balance_ = 0;
};

/**
 * Media Information Header: Hint
 */
class Hmhd : public Box {
public:
    static const uint32_t tag_ = str2BoxType("hmhd");

    Hmhd(const Box &box) : Box(box) {}

    void parseInternal(FileOp &file) override {
        parseFullBox(file);
        max_pdu_size_ = file.readAsBigU16().value();
        avg_pdu_size_ = file.readAsBigU16().value();
        max_bitrate_ = file.readAsBigU32().value();
        avg_bitrate_ = file.readAsBigU32().value();
    }

    std::string detail() override {
        std::ostringstream ss;
        ss << "max pdu: " << max_pdu_size_ << ", avg pdu: " << avg_pdu_size_
           << ", max bitrate: " << max_bitrate_
           << ", avg bitrate: " << avg_bitrate_;
        return ss.str();
    }

private:
    uint16_t max_pdu_size_ = 0;
    uint16_t avg_pdu_size_ = 0;
    uint32_t max_bitrate_ = 0;
    uint32_t avg_bitrate_ = 0;
};

class Durl : public Box {
public:
    static const uint32_t tag_ = str2BoxType("url ");

    Durl(const Box &box) : Box(box) {}

    void parseInternal(FileOp &file) override {
        parseFullBox(file);
        auto strSize = offset_ + size_ - file.tell();
        if (strSize > 0) {
            location_.resize(strSize);
            file.read(location_.data(), strSize, 1);
            location_.back() = '\0';
        }
    }

    std::string detail() override {
        std::ostringstream ss;
        ss << "location: ";
        if (location_.empty()) {
            ss << "null";
        } else {
            ss << location_.data();
        }
        return ss.str();
    }

private:
    std::vector<char> location_;
};

class Dref : public Box {
public:
    static const uint32_t tag_ = str2BoxType("dref");

    Dref(const Box &box) : Box(box) {}

    void parseInternal(FileOp &file) override {
        parseFullBox(file);
        entry_count_ = file.readAsBigU32().value();
        parseChild(file);
    }

    std::string detail() override {
        std::ostringstream ss;
        ss << "entry count: " << entry_count_;
        return ss.str();
    }

private:
    uint32_t entry_count_ = 0;
};

class Dinf : public Box {
public:
    static const uint32_t tag_ = str2BoxType("dinf");

    Dinf(const Box &box) : Box(box) {}

    void parseInternal(FileOp &file) override { parseChild(file); }
};

class Mdia : public Box {
public:
    static const uint32_t tag_ = str2BoxType("mdia");

    Mdia(Box box) : Box(std::move(box)) {}

    void parseInternal(FileOp &file) override { parseChild(file); }

    uint32_t getTimeScale() {
        for (const auto &item : children_) {
            if (item->baseType() == str2BoxType("mdhd")) {
                return dynamic_cast<Mdhd *>(item.get())->timescale();
            }
        }
        return 1;
    }

    uint32_t handleType() {
        for (const auto &item : children_) {
            if (item->baseType() == str2BoxType("hdlr")) {
                return dynamic_cast<Hdlr *>(item.get())->handleType();
            }
        }
        return str2BoxType("und ");
    }
};

/**
 * Decoding time to sample box
 *
 * The decoding time is defined in decoding time to sample box, giving time
 * deltas between successive decoding times.
 *
 * The time to sample boxes must give non-zero durations for all samples with
 * the possible exception of the last one. Durations in the 'stts' box are
 * strictly positive (non-zero), except for the very last entry, which may be
 * zero.
 *
 * indexing from decoding time => sample number
 *
 */
class Stts : public Box {
public:
    static const uint32_t tag_ = str2BoxType("stts");

    Stts(const Box &box) : Box(box) {}

    void parseInternal(FileOp &file) override {
        parseFullBox(file);
        entry_count_ = file.readAsBigU32().value();
        for (int i = 0; i < entry_count_; i++) {
            auto count = file.readAsBigU32().value();
            auto delta = file.readAsBigU32().value();
            time_to_sample_table_.emplace_back(count, delta);
        }
    }

    std::string detail() override {
        std::ostringstream ss;
        ss << "entry: " << entry_count_ << '\n';
        auto mdia = getAncestor(str2BoxType("mdia"));
        uint32_t timescale = 1;
        if (mdia != nullptr) {
            timescale = dynamic_cast<Mdia *>(mdia)->getTimeScale();
        }
        for (const auto &item : time_to_sample_table_) {
            ss << "*** sample count: " << item.first << " -> "
               << "delta: " << item.second << ", timescale: " << timescale
               << '\n';
        }
        return ss.str();
    }

private:
    uint32_t entry_count_ = 0;
    std::vector<std::pair<uint32_t, uint32_t>> time_to_sample_table_;
};

/**
 * Composition time to sample box
 *
 * composition time offsets from decoding time.
 */
class Ctts : public Box {
public:
    static const uint32_t tag_ = str2BoxType("ctts");

    Ctts(const Box &box) : Box(box) {}

    void parseInternal(FileOp &file) override {
        parseFullBox(file);
        entry_count_ = file.readAsBigU32().value();
        for (int i = 0; i < entry_count_; i++) {
            auto count = file.readAsBigU32().value();
            auto delta = file.readAsBigU32().value();
            time_to_sample_table_.emplace_back(count, delta);
        }
    }

    std::string detail() override {
        std::ostringstream ss;
        ss << "entry: " << entry_count_ << '\n';
        auto mdia = getAncestor(str2BoxType("mdia"));
        uint32_t timescale = 1;
        if (mdia != nullptr) {
            timescale = dynamic_cast<Mdia *>(mdia)->getTimeScale();
        }
        for (const auto &item : time_to_sample_table_) {
            ss << "*** sample count: " << item.first << " -> "
               << "sample offset: " << item.second
               << ", timescale: " << timescale << '\n';
        }
        return ss.str();
    }

private:
    uint32_t entry_count_ = 0;
    std::vector<std::pair<uint32_t, uint32_t>> time_to_sample_table_;
};

class SampleEntry : public Box {
public:
    SampleEntry(const Box &box) : Box(box) {}

    void parseInternal(FileOp &file) override {
        file.seek(6, SEEK_CUR);
        data_reference_index_ = file.readAsBigU16().value();
    }

protected:
    uint16_t data_reference_index_ = 0;
};

class VideoSampleEntry : public SampleEntry {
public:
    VideoSampleEntry(const Box &box) : SampleEntry(box) {}

    void parseInternal(FileOp &file) override {
        SampleEntry::parseInternal(file);
        file.seek(2 + 2 + 12, SEEK_CUR);
        width_ = file.readAsBigU16().value();
        height_ = file.readAsBigU16().value();
        horizresolution_ = file.readAsBigU32().value() / (1U << 16U);
        vertresolution_ = file.readAsBigU32().value() / (1U << 16U);
        file.readAsBigU32();
        frame_count_ = file.readAsBigU16().value();
        file.read(&compressor_name_len_, 1, 1);
        compressor_name_.resize(31);
        file.read(compressor_name_.data(), 31, 1);
        depth_ = file.readAsBigU16().value();
        file.readAsBigU16();
    }

    std::string detail() override {
        std::ostringstream ss;
        ss << "width: " << width_ << ", height: " << height_
           << ", horiz resolu: " << horizresolution_
           << ", vert resolu: " << vertresolution_
           << ", frame cnt: " << frame_count_
           << ", compressor name len: " << (int)compressor_name_len_;
        if (compressor_name_len_ > 0) {
            ss << ", compressor name: " << compressor_name_;
        }
        ss << ", depth: " << depth_;
        return ss.str();
    }

private:
    uint16_t width_ = 0;
    uint16_t height_ = 0;
    float horizresolution_ = 0;
    float vertresolution_ = 0;
    uint16_t frame_count_ = 1;
    uint8_t compressor_name_len_ = 0;
    std::string compressor_name_;
    uint16_t depth_ = 0;
};

class AudioSampleEntry : public SampleEntry {
public:
    AudioSampleEntry(const Box &box) : SampleEntry(box) {}

    void parseInternal(FileOp &file) override {
        SampleEntry::parseInternal(file);
        file.readAsBigU64();
        channel_ = file.readAsBigU16().value();
        samplesize_ = file.readAsBigU16().value();
        file.readAsBigU32();
        samplerate_ = (double)file.readAsBigU32().value() / (1U << 16U);
    }

    std::string detail() override {
        std::ostringstream ss;
        ss << "channel: " << channel_ << ", samplesize: " << samplesize_
           << ", samplerate: " << samplerate_;
        return ss.str();
    }

private:
    uint16_t channel_ = 2;
    uint16_t samplesize_ = 16;
    float samplerate_ = 0;
};

/**
 * Sample description box
 *
 * The sample description table gives detailed information about the coding
 * type used, and any initialization information used for that coding.
 */
class Stsd : public Box {
public:
    static const uint32_t tag_ = str2BoxType("stsd");

    Stsd(const Box &box) : Box(box) {}

    void parseInternal(FileOp &file) override {
        parseFullBox(file);
        entry_count_ = file.readAsBigU32().value();
        auto end = offset_ + size_;
        auto mdia = getAncestor(str2BoxType("mdia"));
        auto handleType = dynamic_cast<Mdia *>(mdia)->handleType();
        while (file.tell() < end) {
            auto box = parseBasic(file);
            if (box == nullptr) {
                return;
            }
            box->parent_ = this;
            std::shared_ptr<Box> detailBox;
            if (handleType == str2BoxType("vide")) {
                detailBox = toDetail<VideoSampleEntry>(*box);
            } else if (handleType == str2BoxType("soun")) {
                detailBox = toDetail<AudioSampleEntry>(*box);
            } else {
                detailBox = std::move(box);
            }
            detailBox->parseInternal(file);
            children_.push_back(detailBox);
            file.seek(detailBox->offset() + detailBox->size(), SEEK_SET);
        }
    }

    std::string detail() override {
        return std::string("entry: ") + std::to_string(entry_count_);
    }

private:
    uint32_t entry_count_ = 0;
};

class Stsz : public Box {
public:
    static const uint32_t tag_ = str2BoxType("stsz");

    Stsz(const Box &box) : Box(box) {}

    void parseInternal(FileOp &file) override {
        parseFullBox(file);
        sample_size_ = file.readAsBigU32().value();
        sample_count_ = file.readAsBigU32().value();
        if (sample_size_ == 0) {
            for (uint32_t i = 0; i < sample_count_; i++) {
                entry_size_.push_back(file.readAsBigU32().value());
            }
        }
    }

    std::string detail() override {
        std::ostringstream ss;
        ss << "sample size: " << sample_size_ << ", count: " << sample_count_;
        if (!entry_size_.empty()) {
            for (auto n : entry_size_) {
                ss << "\nentry size: " << n;
            }
        }
        return ss.str();
    }

private:
    uint32_t sample_size_ = 0;
    uint32_t sample_count_ = 0;
    std::vector<uint32_t> entry_size_;
};

class Stsc : public Box {
public:
    static const uint32_t tag_ = str2BoxType("stsc");

    Stsc(const Box &box) : Box(box) {}

    void parseInternal(FileOp &file) override {
        parseFullBox(file);
        entry_count_ = file.readAsBigU32().value();
        for (uint32_t i = 0; i < entry_count_; i++) {
            entrys_.push_back(Entry{file.readAsBigU32().value(),
                                    file.readAsBigU32().value(),
                                    file.readAsBigU32().value()});
        }
    }

    std::string detail() override {
        std::ostringstream ss;
        ss << "entry count: " << entry_count_;
        int i = 0;
        for (const auto &entry : entrys_) {
            ss << "\nentry " << i << ", first chunk: " << entry.first_chunk_
               << ", sample per chunk: " << entry.samples_per_chunk_
               << ", sample description index: "
               << entry.sample_description_index_;
            ++i;
        }
        return ss.str();
    }

    struct Entry {
        uint32_t first_chunk_;
        uint32_t samples_per_chunk_;
        uint32_t sample_description_index_;
    };

private:
    uint32_t entry_count_ = 0;
    std::vector<Entry> entrys_;
};

/**
 * Chunk offset box
 */
class Stco : public Box {
public:
    static const uint32_t tag_ = str2BoxType("stco");

    Stco(const Box &box) : Box(box) {}

    void parseInternal(FileOp &file) override {
        parseFullBox(file);
        entry_count_ = file.readAsBigU32().value();
        for (uint32_t i = 0; i < entry_count_; i++) {
            chunk_offsets_.push_back(file.readAsBigU32().value());
        }
    }

    std::string detail() override {
        std::ostringstream ss;
        ss << "entry count: " << entry_count_;
        for (uint32_t i = 0, len = chunk_offsets_.size(); i < len; i++) {
            ss << "\nentry " << i << ", offset " << chunk_offsets_[i];
        }
        return ss.str();
    }

private:
    uint32_t entry_count_ = 0;
    std::vector<uint32_t> chunk_offsets_;
};

/**
 * Sync sample box
 */
class Stss : public Box {
public:
    static const uint32_t tag_ = str2BoxType("stss");

    Stss(const Box &box) : Box(box) {}

    void parseInternal(FileOp &file) override {
        parseFullBox(file);
        entry_count_ = file.readAsBigU32().value();
        for (unsigned i = 0; i < entry_count_; i++) {
            sample_numbers_.push_back(file.readAsBigU32().value());
        }
    }

    std::string detail() override {
        std::ostringstream ss;
        ss << "entry count: " << entry_count_;
        for (unsigned i = 0, n = sample_numbers_.size(); i < n; i++) {
            ss << "\nsync sample box, entry " << i
               << ", sample: " << sample_numbers_[i];
        }

        return ss.str();
    }

private:
    uint32_t entry_count_ = 0;
    std::vector<uint32_t> sample_numbers_;
};

/**
 * Chunk offset box
 */
class Co64 : public Box {
public:
    static const uint32_t tag_ = str2BoxType("co64");

    Co64(const Box &box) : Box(box) {}

    void parseInternal(FileOp &file) override {
        parseFullBox(file);
        entry_count_ = file.readAsBigU32().value();
        for (uint32_t i = 0; i < entry_count_; i++) {
            chunk_offsets_.push_back(file.readAsBigU64().value());
        }
    }

    std::string detail() override {
        std::ostringstream ss;
        ss << "entry count: " << entry_count_;
        for (uint32_t i = 0, len = chunk_offsets_.size(); i < len; i++) {
            ss << "\nentry " << i << ", offset " << chunk_offsets_[i];
        }
        return ss.str();
    }

private:
    uint32_t entry_count_ = 0;
    std::vector<uint64_t> chunk_offsets_;
};

/**
 * Sample table box
 */
class Stbl : public Box {
public:
    static const uint32_t tag_ = str2BoxType("stbl");

    Stbl(const Box &box) : Box(box) {}

    void parseInternal(FileOp &file) override { parseChild(file); }
};

/**
 * Media Information Box
 *
 * Container: Media Box(mdia)
 */
class Minf : public Box {
public:
    static const uint32_t tag_ = str2BoxType("minf");

    Minf(const Box &box) : Box(box) {}

    void parseInternal(FileOp &file) override { parseChild(file); }
};

class Trak : public Box {
public:
    static const uint32_t tag_ = str2BoxType("trak");

    Trak(Box box) : Box(std::move(box)) {}

    void parseInternal(FileOp &file) override { parseChild(file); }
};

class Moov : public Box {
public:
    static const uint32_t tag_ = str2BoxType("moov");

    Moov(Box box) : Box(box) {}

    void parseInternal(FileOp &file) override { parseChild(file); }
};

class Mdat : public Box {
public:
    static const uint32_t tag_ = str2BoxType("mdat");

    Mdat(Box box) : Box(box) {}

    void parseInternal(FileOp &file) override {}
};

class Mp4Paser {
public:
    static Box::Boxes parse(const char *path) {
        Box::Boxes boxes;
        FileOp file(path);
        if (!file.open("r")) return boxes;
        while (true) {
            auto box = Box::parseBasic(file);
            if (box != nullptr) {
                auto detailBox = toDetailType(std::move(box));
                detailBox->parseInternal(file);
                boxes.push_back(detailBox);
                file.seek(detailBox->offset() + detailBox->size(), SEEK_SET);
            } else {
                return boxes;
            }
        }
    }
};

std::shared_ptr<Box> toDetailType(std::unique_ptr<Box> base) {
    switch (base->baseType()) {
        case Ctts::tag_:
            return toDetail<Ctts>(*base);
        case Dinf::tag_:
            return toDetail<Dinf>(*base);
        case Dref::tag_:
            return toDetail<Dref>(*base);
        case Durl::tag_:
            return toDetail<Durl>(*base);
        case Hmhd::tag_:
            return toDetail<Hmhd>(*base);
        case Mdhd::tag_:
            return toDetail<Mdhd>(*base);
        case Hdlr::tag_:
            return toDetail<Hdlr>(*base);
        case Minf::tag_:
            return toDetail<Minf>(*base);
        case Smhd::tag_:
            return toDetail<Smhd>(*base);
        case Stbl::tag_:
            return toDetail<Stbl>(*base);
        case Stsc::tag_:
            return toDetail<Stsc>(*base);
        case Stsd::tag_:
            return toDetail<Stsd>(*base);
        case Stsz::tag_:
            return toDetail<Stsz>(*base);
        case Stts::tag_:
            return toDetail<Stts>(*base);
        case Vmhd::tag_:
            return toDetail<Vmhd>(*base);
        case Tkhd::tag_:
            return toDetail<Tkhd>(*base);
        case Mdia::tag_:
            return toDetail<Mdia>(*base);
        case Mvhd::tag_:
            return toDetail<Mvhd>(*base);
        case Trak::tag_:
            return toDetail<Trak>(*base);
        case Moov::tag_:
            return toDetail<Moov>(*base);
        case Ftyp::tag_:
            return toDetail<Ftyp>(*base);
        case Mdat::tag_:
            return toDetail<Mdat>(*base);
        case Stco::tag_:
            return toDetail<Stco>(*base);
        case Co64::tag_:
            return toDetail<Co64>(*base);
        case Stss::tag_:
            return toDetail<Stss>(*base);
        default:
            return base;
    }
}

}  // namespace mov
