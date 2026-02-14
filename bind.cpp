#include <format>

#include <emscripten/bind.h>

#include "inc/H264SwDecApi.h"

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
    uint32_t isIdrPicture;
    uint32_t nbrOfErrMBs;
};

struct DecodeResult
{
    H264SwDecRet result;
    intptr_t read;
    std::optional<Picture> picture;
};

class Decoder
{
private:
    H264SwDecInst instance;

    H264SwDecInfo info;
    uintptr_t picture_size;

    void get_info()
    {
        auto result = H264SwDecGetInfo(instance, &info);
        if (result != H264SWDEC_OK)
        {
            val Error = val::global("Error");
            Error.new_(val::u8string("Can't get info")).throw_();
        }

        picture_size = info.picWidth * info.picHeight * 3 / 2;

        if (!info.croppingFlag)
        {
            info.cropParams.cropOutWidth = info.picWidth;
            info.cropParams.cropOutHeight = info.picHeight;
        }
    }

public:
    Decoder() : info{}, picture_size(0)
    {
        H264SwDecInit(&instance, 0);
    }

    ~Decoder()
    {
        H264SwDecRelease(instance);
    }

    DecodeResult decode(std::string data, uint32_t picId, uint32_t intraConcealmentMethod)
    {
        H264SwDecInput input{
            (uint8_t *)data.data(),
            data.size(),
            picId,
            intraConcealmentMethod};
        H264SwDecRet result;
        H264SwDecOutput output;
        while (input.dataLen > 0)
        {
            result = H264SwDecDecode(instance, &input, &output);

            if (result == H264SWDEC_HDRS_RDY_BUFF_NOT_EMPTY)
            {
                get_info();
            }
            if (result == H264SWDEC_PIC_RDY || result == H264SWDEC_PIC_RDY_BUFF_NOT_EMPTY)
            {
                H264SwDecPicture picture;
                H264SwDecNextPicture(instance, &picture, false);

                return {
                    .result = result,
                    .read = output.pStrmCurrPos - (uint8_t *)data.data(),
                    .picture = Picture{
                        .info = info,
                        .bytes = Uint8Array(val(typed_memory_view(picture_size, (uint8_t *)picture.pOutputPicture))),
                        .picId = picture.picId,
                        .isIdrPicture = picture.isIdrPicture,
                        .nbrOfErrMBs = picture.nbrOfErrMBs}};
            }
            else if (result < 0)
            {
                break;
            }

            input.dataLen -= output.pStrmCurrPos - input.pStream;
            input.pStream = output.pStrmCurrPos;
        }

        return {result, output.pStrmCurrPos - (uint8_t *)data.data(), std::nullopt};
    }

    std::vector<Picture> flush()
    {
        std::vector<Picture> pictures;
        H264SwDecPicture picture;
        H264SwDecRet result;
        while ((result = H264SwDecNextPicture(instance, &picture, true)) == H264SWDEC_PIC_RDY)
        {
            pictures.push_back(Picture{
                .info = info,
                .bytes = Uint8Array(val(typed_memory_view(picture_size, (uint8_t *)picture.pOutputPicture))),
                .picId = picture.picId,
                .isIdrPicture = picture.isIdrPicture,
                .nbrOfErrMBs = picture.nbrOfErrMBs});
        }
        return pictures;
    }
};

EMSCRIPTEN_BINDINGS(h264bsd)
{
    register_type<Uint8Array>("Uint8Array");

    value_object<H264SwDecApiVersion>("H264SwDecApiVersion")
        .field("major", &H264SwDecApiVersion::major)
        .field("minor", &H264SwDecApiVersion::minor);

    function("get_api_version", &H264SwDecGetAPIVersion);

    enum_<H264SwDecRet>("Result", enum_value_type::number)
        .value("OK", H264SWDEC_OK)
        .value("STRM_PROCESSED", H264SWDEC_STRM_PROCESSED)
        .value("PIC_RDY", H264SWDEC_PIC_RDY)
        .value("PIC_RDY_BUFF_NOT_EMPTY", H264SWDEC_PIC_RDY_BUFF_NOT_EMPTY)
        .value("HDRS_RDY_BUFF_NOT_EMPTY", H264SWDEC_HDRS_RDY_BUFF_NOT_EMPTY)
        .value("PARAM_ERR", H264SWDEC_PARAM_ERR)
        .value("STRM_ERR", H264SWDEC_STRM_ERR)
        .value("NOT_INITIALIZED", H264SWDEC_NOT_INITIALIZED)
        .value("MEMFAIL", H264SWDEC_MEMFAIL)
        .value("INITFAIL", H264SWDEC_INITFAIL)
        .value("HDRS_NOT_RDY", H264SWDEC_HDRS_NOT_RDY);

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
        .field("isIdrPicture", &Picture::isIdrPicture)
        .field("nbrOfErrMBs", &Picture::nbrOfErrMBs);

    register_optional<Picture>();
    register_vector<Picture>("PictureArray");

    value_object<DecodeResult>("DecodeResult")
        .field("result", &DecodeResult::result)
        .field("read", &DecodeResult::read)
        .field("picture", &DecodeResult::picture);

    class_<Decoder>("Decoder")
        .constructor()
        .function("decode(data, picId, intraConcealmentMethod)", &Decoder::decode)
        .function("flush", &Decoder::flush);
}
