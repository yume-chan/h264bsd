
#include <emscripten/bind.h>

extern "C"
{
#include "inc/H264SwDecApi.h"
#include "source/h264bsd_container.h"
#include "source/h264bsd_decoder.h"
}

#include <bit>

using namespace emscripten;

// `EMSCRIPTEN_DECLARE_VAL_TYPE(Uint8Array)` don't have a default constructor
// so can't be used in `value_object`
struct Uint8Array : val
{
    Uint8Array() : val() {}
    Uint8Array(val const &other) : val(other) {}
};

struct Picture
{
    H264SwDecInfo info;
    Uint8Array bytes;
    uint32_t picId;
    bool isIdr;
    uint32_t nbrOfErrMBs;
};

enum class DecodeResultCode
{
    Ready = H264BSD_RDY,
    PictureReady = H264BSD_PIC_RDY,
    HeadersReady = H264BSD_HDRS_RDY,
    Error = H264BSD_ERROR,
    ParamSetError = H264BSD_PARAM_SET_ERROR,
    MemoryError = H264BSD_MEMALLOC_ERROR,
};

struct DecodeResult
{
    DecodeResultCode code;
    uint32_t read;
    std::optional<Picture> picture;
    uint32_t extraPictureCount;
};

struct FlushResult
{
    std::optional<Picture> picture;
    uint32_t extraPictureCount;
};

constexpr uint32_t INITIAL_BUFFER_SIZE = 1024;

class Decoder
{
private:
    union
    {
        H264SwDecInst instance;
        decContainer_t *container;
    };

    uint8_t *buffer;
    size_t bufferSize;

    H264SwDecInfo info;
    size_t pictureSize;

    void getInfo()
    {
        auto result = H264SwDecGetInfo(instance, &info);
        if (result != H264SWDEC_OK)
        {
            val Error = val::global("Error");
            Error.new_(val::u8string("Can't get info")).throw_();
        }

        pictureSize = info.picWidth * info.picHeight * 3 / 2;

        if (!info.croppingFlag)
        {
            info.cropParams.cropOutWidth = info.picWidth;
            info.cropParams.cropOutHeight = info.picHeight;
        }
    }

    uint32_t getPictureCount()
    {
        auto *dpb = container->storage.dpb;
        return dpb->numOut - dpb->outIndex;
    }

    std::pair<DecodeResultCode, uint32_t> decodeInternal(
        uint8_t *data,
        uint32_t length,
        uint32_t picId)
    {
        DecodeResultCode code = DecodeResultCode::Error;
        uint32_t totalReadBytes = 0;

        while (length > 0)
        {
            uint32_t readBytes;
            code = static_cast<DecodeResultCode>(
                h264bsdDecode(
                    &container->storage,
                    data,
                    length,
                    picId,
                    &readBytes));

            totalReadBytes += readBytes;

            switch (code)
            {
            case DecodeResultCode::Ready:
                break;
            case DecodeResultCode::PictureReady:
                if (getPictureCount() != 0)
                {
                    goto finish;
                }
                break;
            case DecodeResultCode::HeadersReady:
                if (container->storage.dpb->flushed && getPictureCount() != 0)
                {
                    container->decStat = decContainer_t::NEW_HEADERS;
                    goto finish;
                }

                getInfo();
                break;
            default:
                goto finish;
            }

            data += readBytes;
            length -= readBytes;
        }

    finish:
        return {code, totalReadBytes};
    }

public:
    Decoder()
        : buffer(new uint8_t[INITIAL_BUFFER_SIZE]),
          bufferSize(INITIAL_BUFFER_SIZE),
          info{},
          pictureSize(0)
    {
        H264SwDecInit(&instance, 0);
    }

    ~Decoder()
    {
        delete[] buffer;
        H264SwDecRelease(instance);
    }

    void setIntraConcealmentMethod(uint32_t intraConcealmentMethod)
    {
        container->storage.intraConcealmentFlag = intraConcealmentMethod;
    }

    DecodeResult decode(Uint8Array data, uint32_t picId)
    {
        if (container->decStat == decContainer_t::NEW_HEADERS)
        {
            getInfo();
            container->decStat = decContainer_t::INITIALIZED;
        }

        auto length = data["length"].as<uint32_t>();
        if (length > bufferSize)
        {
            delete[] buffer;

            auto capacity = std::bit_ceil(length);
            buffer = new uint8_t[capacity];
            bufferSize = capacity;
        }

        val bufferView = val(typed_memory_view(length, buffer));
        bufferView.call<void>("set", data);

        auto [result, readBytes] = decodeInternal(buffer, length, picId);
        return {
            result,
            readBytes,
            getNextPicture(),
            getPictureCount(),
        };
    }

    std::optional<Picture> getNextPicture()
    {
        auto result = h264bsdDpbOutputPicture(container->storage.dpb);
        if (result != nullptr)
        {
            return Picture{
                .info = info,
                .bytes = Uint8Array(val(typed_memory_view(pictureSize, result->data))),
                .picId = result->picId,
                .isIdr = result->isIdr != 0,
                .nbrOfErrMBs = result->numErrMbs,
            };
        }
        return std::nullopt;
    }

    FlushResult flush()
    {
        h264bsdFlushBuffer(&container->storage);
        return {getNextPicture(), getPictureCount()};
    }
};

EMSCRIPTEN_BINDINGS(h264bsd)
{
    register_type<Uint8Array>("Uint8Array");

    value_object<H264SwDecApiVersion>("Version")
        .field("major", &H264SwDecApiVersion::major)
        .field("minor", &H264SwDecApiVersion::minor);

    function("getApiVersion", &H264SwDecGetAPIVersion);

    value_object<CropParams>("CropParams")
        .field("cropLeftOffset", &CropParams::cropLeftOffset)
        .field("cropOutWidth", &CropParams::cropOutWidth)
        .field("cropTopOffset", &CropParams::cropTopOffset)
        .field("cropOutHeight", &CropParams::cropOutHeight);

    value_object<H264SwDecInfo>("Info")
        .field("profile", &H264SwDecInfo::profile)
        .field("picWidth", &H264SwDecInfo::picWidth)
        .field("picHeight", &H264SwDecInfo::picHeight)
        .field("videoRange", &H264SwDecInfo::videoRange)
        .field("matrixCoefficients", &H264SwDecInfo::matrixCoefficients)
        .field("parWidth", &H264SwDecInfo::parWidth)
        .field("parHeight", &H264SwDecInfo::parHeight)
        .field("croppingFlag", &H264SwDecInfo::croppingFlag)
        .field("cropParams", &H264SwDecInfo::cropParams);

    value_object<Picture>("Picture")
        .field("info", &Picture::info)
        .field("bytes", &Picture::bytes)
        .field("picId", &Picture::picId)
        .field("isIdr", &Picture::isIdr)
        .field("nbrOfErrMBs", &Picture::nbrOfErrMBs);

    register_optional<Picture>();

    enum_<DecodeResultCode>("DecodeResultCode", enum_value_type::number)
        .value("Ready", DecodeResultCode::Ready)
        .value("PictureReady", DecodeResultCode::PictureReady)
        .value("HeadersReady", DecodeResultCode::HeadersReady)
        .value("Error", DecodeResultCode::Error)
        .value("ParamSetError", DecodeResultCode::ParamSetError)
        .value("MemoryError", DecodeResultCode::MemoryError);

    value_object<DecodeResult>("DecodeResult")
        .field("code", &DecodeResult::code)
        .field("read", &DecodeResult::read)
        .field("picture", &DecodeResult::picture)
        .field("extraPictureCount", &DecodeResult::extraPictureCount);

    value_object<FlushResult>("FlushResult")
        .field("picture", &FlushResult::picture)
        .field("extraPictureCount", &FlushResult::extraPictureCount);

    class_<Decoder>("Decoder")
        .constructor()
        .function("setIntraConcealmentMethod(method)", &Decoder::setIntraConcealmentMethod)
        .function("decode(data, picId)", &Decoder::decode)
        .function("getNextPicture", &Decoder::getNextPicture)
        .function("flush", &Decoder::flush);
}
