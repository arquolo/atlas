#include <openjpeg-2.3/openjpeg.h>

#include "codec_j2k.h"
#include "core/traits.h"

namespace ts::j2k {

struct opj_memstream_t {
    OPJ_UINT8 const* data;
    OPJ_UINT64 size;
    OPJ_UINT64 offset;
    OPJ_UINT64 written;
};

OPJ_SIZE_T opj_mem_read(void* dst, OPJ_SIZE_T size, void* data) {
    auto* memstream = static_cast<opj_memstream_t*>(data);
    if (memstream->offset >= memstream->size)
        return std::numeric_limits<OPJ_SIZE_T>::max();

    OPJ_SIZE_T count = std::min(size, memstream->size - memstream->offset);
    memcpy(dst, static_cast<const void*>(memstream->data + memstream->offset), count);
    memstream->offset += count;
    return count;
}

OPJ_BOOL opj_mem_seek(OPJ_OFF_T size, void* data) {
    auto* memstream = static_cast<opj_memstream_t*>(data);
    if (size < 0 || size >= static_cast<OPJ_OFF_T>(memstream->size))
        return OPJ_FALSE;

    memstream->offset = static_cast<OPJ_SIZE_T>(size);
    return OPJ_TRUE;
}

OPJ_OFF_T opj_mem_skip(OPJ_OFF_T size, void* data) {
    auto* memstream = static_cast<opj_memstream_t*>(data);
    if (size < 0)
        return -1;

    OPJ_SIZE_T count = std::min(static_cast<OPJ_SIZE_T>(size), memstream->size - memstream->offset);
    memstream->offset += count;
    return count;
}

void opj_mem_nop(void*) {}

// TODO: fuck this, rewrite in Rust

std::optional<OPJ_CODEC_FORMAT> _check_signature(Bitstream const& src) noexcept {
    std::string_view const sig = {src.data(), 12};
    if (sig.substr(0, 4) == "\xff\x4f\xff\x51")
        return OPJ_CODEC_J2K;

    if (sig.substr(0, 4) == "\x0d\x0a\x87\x0a"
        || sig == "\x00\x00\x00\x0c\x6a\x50\x20\x20\x0d\x0a\x87\x0a")
        return OPJ_CODEC_JP2;

    return {};
}

opj_stream_t* _make_obj_stream(opj_memstream_t* memstream) {
    opj_stream_t* stream = opj_stream_default_create(OPJ_TRUE);
    if (!stream)
        return nullptr;

    opj_stream_set_read_function(stream, opj_mem_read);
    opj_stream_set_seek_function(stream, opj_mem_seek);
    opj_stream_set_skip_function(stream, opj_mem_skip);
    opj_stream_set_user_data(stream, memstream, opj_mem_nop);
    opj_stream_set_user_data_length(stream, memstream->size);
    return stream;
}

Result<_RType> decode(Bitstream const& src) {
    auto codecformat = _check_signature(src);
    if (!codecformat)
        return Error{"not a J2K or JP2 data stream"};

    opj_memstream_t memstream = {
        reinterpret_cast<OPJ_UINT8 const*>(src.data()), src.size(), 0, 0};
    auto* stream = _make_obj_stream(&memstream);
    if (!stream)
        return Error{"opj_memstream_create failed"};
    auto _stream = make_owner(stream, opj_stream_destroy);

    auto* codec = opj_create_decompress(codecformat.value());
    if (!codec)
        return Error{"opj_create_decompress failed"};
    auto _codec = make_owner(codec, opj_destroy_codec);

    opj_dparameters_t parameters;
    opj_set_default_decoder_parameters(&parameters);

    if (!opj_setup_decoder(codec, &parameters))
        return Error{"opj_setup_decoder failed"};

    opj_image_t* image = nullptr;
    if (!opj_read_header(stream, codec, &image))
        return Error{"opj_read_header failed"};
    auto _image = make_owner(image, opj_image_destroy);

    if (!opj_set_decode_area(
            codec, image,
            parameters.DA_x0, parameters.DA_y0, parameters.DA_x1, parameters.DA_y1))
        return Error{"opj_set_decode_area failed"};

    if (!opj_decode(codec, stream, image))
        return Error{"opj_decode failed"};
    if (!opj_end_decompress(codec, stream))
        return Error{"opj_end_decompress failed"};

    // handle subsampling and color profiles
    if (image->color_space != OPJ_CLRSPC_SRGB)
        return Error{"unusupported colorspace"};

    opj_image_comp_t* comp = &image->comps[0];
    if (comp->sgnd)
        return Error{"signed is not supported"};

    auto prec = comp->prec;
    auto height = comp->h * comp->dy;
    auto width = comp->w * comp->dx;
    auto samples = image->numcomps;
    auto itemsize = (prec + 7) / 8;

    for (ssize_t i = 0; i < samples; ++i) {
        comp = &image->comps[i];
        if (comp->sgnd || comp->prec != prec)
            return Error{"components dtype mismatch"};
        if (comp->w != width || comp->h != height)
            return Error{"subsampling not supported"};
    }
    if (itemsize == 3)
        itemsize = 4;
    else if (itemsize < 1 || itemsize > 4)
        return Error{"unsupported itemsize"};

    auto dtype = make_variant_if([itemsize](auto v){
        return std::is_unsigned_v<decltype(v)> && (sizeof(v) == itemsize);
    }, DType{});
    if (!dtype)
        return Error{"Can't get dtype"};

        Bitstream dst(height * width * samples * itemsize);
    std::visit(
        [&, samples, total = height * width](auto v){
            using Type = std::decay_t<decltype(v)>;
            for (ssize_t i = 0; i < samples; ++i) {
                auto* ix = reinterpret_cast<Type*>(dst.data()) + i;
                auto* band = image->comps[i].data;
                for (ssize_t j = 0; j < total; ++j)
                    ix[j * samples] = static_cast<Type>(band[j]);
            }
        },
        dtype.value()
    );
    return _RType{dst, {height, width, samples}, dtype.value()};
}

} // namespace ts::j2k
