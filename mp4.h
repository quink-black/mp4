#include <assert.h>

#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <sstream>
#include <vector>
#include <utility>

namespace mov {

static uint32_t buf2UInt32(uint8_t *buf) {
    return (uint32_t)buf[0] << 24 | (uint32_t)buf[1] << 16 | (uint32_t)buf[2] << 8 | buf[3];
}

static uint64_t buf2UInt64(uint8_t *buf) {
    uint64_t high = buf2UInt32(buf);
    uint64_t low = buf2UInt32(buf + 4);
    return high + low;
}

class FileOp {
public:
    FileOp(const std::string &path) : path_(path) { }

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

    off_t tell() {
        return ftello(file_);
    }

    int seek(off_t offset, int whence) {
        return fseeko(file_, offset, whence);
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

class Box {
public:
    using BaseType = std::array<char, 4>;
    using ExtendedType = std::array<char, 16>;
    using Boxs = std::vector<std::shared_ptr<Box>>;

    class BoxBuilder {
    public:
        uint64_t size_ = 0;
        uint64_t offset_ = 0;
        BaseType type_{};
        ExtendedType extended_type_{};

        std::unique_ptr<Box> build() {
            return std::make_unique<Box>(size_, offset_, type_, extended_type_);
        }
    };

    Box(uint64_t size, uint64_t offset,
        BaseType type, ExtendedType extended_type)
            : size_(size), offset_(offset),
              type_(type), extended_type_(extended_type) {
    }

    static constexpr BaseType str2BaseType(const char (&str)[5]) {
        return BaseType{str[0], str[1], str[2], str[3]};
    }

    static std::string baseType2str(BaseType baseType) {
        return std::string(baseType.begin(), baseType.end());
    }

    std::string baseTypeStr() {
        return baseType2str(type_);
    }

    static std::unique_ptr<Box> parseBasic(FileOp &in) {
        BoxBuilder builder;
        builder.offset_ = in.tell();

        auto size = in.readAsBigU32();
        if (size.has_value()) {
            builder.size_ = size.value();
        } else {
            return nullptr;
        }

        if (!in.read(builder.type_.data(), 4, 1)) {
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
        if (builder.type_ == str2BaseType("uuid")) {
            if (!in.read(builder.extended_type_.data(), 16, 1)) {
                return nullptr;
            }
        }
        return builder.build();
    }

    virtual void parseInternal(FileOp &file) {
    }

    virtual std::string detail() {
        return std::string();
    }

    uint64_t size() const {
        return size_;
    }

    uint64_t offset() const {
        return offset_;
    }

    BaseType baseType() const {
        return type_;
    }

    ExtendedType extendedType() const {
        return extended_type_;
    }

protected:
    uint64_t size_ = 0;
    uint64_t offset_ = 0;
    BaseType type_{};
    ExtendedType extended_type_{};

    Boxs children_;
};

class Ftyp : public Box {
public:
    static const BaseType &type() {
        static const auto t = str2BaseType("ftyp");
        return t;
    }

    Ftyp(Box box) : Box(box) { }

    void parseInternal(FileOp &file) override {
        auto remain = size_ - (file.tell() - offset_);
        if (remain >= major_brand_.size()) {
            file.read(major_brand_.data(), major_brand_.size(), 1);
            remain -= major_brand_.size();
        }
        if (remain >= 4) {
            minor_version = file.readAsBigU32().value();
            remain -= 4;
        }
        remain /= 4;
        if (remain > 0) {
            compatible_brands_.resize(remain);
            for (int i = 0; i < remain; i++) {
                file.read(compatible_brands_[i].data(), 4, 1);
            }
        }
    }

    std::string detail() override {
        std::ostringstream ss;
        ss << "major brand: " << baseType2str(major_brand_)
            << ", minor version: " << minor_version
            << ", compatible_brands_: ";
        for (const auto &n : compatible_brands_) {
            ss << baseType2str(n) << " ";
        }
        return ss.str();
    }

private:
    BaseType major_brand_{};
    uint32_t minor_version = 0;
    std::vector<BaseType> compatible_brands_{};
};

class Moov : public Box {
public:
    static const BaseType &type() {
        static const auto t = str2BaseType("moov");
        return t;
    }

    Moov(Box box) : Box(box) {}

    void parseInternal(FileOp &file) override {
    }
};

class Trak : public Box {
public:
    static const BaseType &type() {
        static const auto t = str2BaseType("trak");
        return t;
    }

    void parseInternal(FileOp &file) override {
    }
};

class Mp4Paser {
public:
    static Box::Boxs parse(const char *path) {
        Box::Boxs boxes;
        FileOp file(path);
        if (!file.open("r"))
            return boxes;
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

    static std::shared_ptr<Box> toDetailType(std::unique_ptr<Box> base) {
        if (base->baseType() == Moov::type()) {
            return std::static_pointer_cast<Box>(std::make_shared<Moov>(*base));
        } if (base->baseType() == Ftyp::type()) {
            return std::static_pointer_cast<Box>(std::make_shared<Ftyp>(*base));
        } else {
            return std::move(base);
        }
    }
};

} // namespace mov
