#include <openjpeg-2.3/openjpeg.h>
#include <openjpeg-2.3/opj_config.h>

#include "tiff_j2k.h"

#ifdef _MSC_VER
#define __PRETTY_FUNCTION__ __FUNCSIG__
#endif

namespace std {
template <typename T, typename Deleter>
unique_ptr(T* ptr, Deleter deleter) -> unique_ptr<T, Deleter>;
} // namespace std

namespace {

struct Guard {
    std::string name;

    Guard(std::string name) : name{std::move(name)} {
        printf("%s -> in\n", this->name.data());
    }
    ~Guard() { printf("%s -> out\n", name.data()); }
};

struct stream_t {
    OPJ_UINT8* data;
    OPJ_SIZE_T size;
    OPJ_SIZE_T offset = 0;
};

void null_callback(const char*, void*) {}

static void stream_no_op(void*) {}

// this will read from p_buffer to the stream
static OPJ_SIZE_T stream_read(void* buffer, OPJ_SIZE_T bytes, void* data) {
    Guard g{__PRETTY_FUNCTION__};
    auto* stream = static_cast<stream_t*>(data);
    OPJ_SIZE_T data_end_offset = stream->size - 1;

    // check if the current offset is outside our data buffer
    if (stream->offset >= data_end_offset)
        return OPJ_SIZE_T(-1);

    // check if we are reading more than we have
    OPJ_SIZE_T bytes_read = std::min(bytes, data_end_offset - stream->offset);

    // copy the data to the internal buffer
    memcpy(buffer, stream->data + stream->offset, bytes_read);

    // update the pointer to the new location
    stream->offset += bytes_read;
    return bytes_read;
}

// this will write from the stream to p_buffer
static OPJ_SIZE_T stream_write(void* buffer, OPJ_SIZE_T bytes, void* data) {
    Guard g{__PRETTY_FUNCTION__};
    auto* stream = static_cast<stream_t*>(data);
    OPJ_SIZE_T data_end_offset = stream->size - 1;

    // check if the current offset is outside our data buffer
    if (stream->offset >= data_end_offset)
        return OPJ_SIZE_T(-1);

    // amount to move to buffer
    OPJ_SIZE_T bytes_write = std::min(bytes, data_end_offset - stream->offset);

    // copy the data from the internal buffer
    memcpy(stream->data + stream->offset, buffer, bytes_write);

    // update the pointer to the new location
    stream->offset += bytes_write;
    return bytes_write;
}

// moves the current offset forward, but never more than size
static OPJ_OFF_T stream_skip(OPJ_OFF_T bytes, void* data) {
    Guard g{__PRETTY_FUNCTION__};
    if (bytes < 0)
        return -1;

    auto* stream = static_cast<stream_t*>(data);
    auto data_end_offset = stream->size - 1;

    // do not allow moving past the end
    OPJ_SIZE_T bytes_skip = std::min(
        static_cast<OPJ_SIZE_T>(bytes), data_end_offset - stream->offset);

    stream->offset += bytes_skip;  // make the jump
    return bytes_skip;
}

// sets the offset to anywhere in the stream.
static OPJ_BOOL stream_seek(OPJ_OFF_T bytes, void* data) {
    Guard g{__PRETTY_FUNCTION__};
    if (bytes < 0)
        return OPJ_FALSE; // not before the buffer

    auto* stream = static_cast<stream_t*>(data);

    if (bytes > static_cast<OPJ_OFF_T>(stream->size - 1))
        return OPJ_FALSE; // not after the buffer

    stream->offset = static_cast<OPJ_SIZE_T>(bytes); // move to new position
    return OPJ_TRUE;
}

// create a stream to use memory as the input or output
auto make_memory_stream(stream_t& stream, OPJ_BOOL readable) {
    Guard g{__PRETTY_FUNCTION__};
    opj_stream_t* l_stream = opj_stream_default_create(readable);
    if (!l_stream)
        throw std::runtime_error{"stream is not created"};

    // set how to work with the frame buffer
    if (readable)
        opj_stream_set_read_function(l_stream, stream_read);
    else
        opj_stream_set_write_function(l_stream, stream_write);

    opj_stream_set_seek_function(l_stream, stream_seek);
    opj_stream_set_skip_function(l_stream, stream_skip);
    opj_stream_set_user_data(l_stream, &stream, stream_no_op);
    opj_stream_set_user_data_length(l_stream, stream.size);

    return std::unique_ptr(l_stream, opj_stream_destroy);
}

} // namespace

namespace ts::jp2k {

std::vector<uint8_t> decode(const std::vector<uint8_t>& buf) {
    Guard g{__PRETTY_FUNCTION__};
    // set up the input buffer as a stream
    stream_t stream{const_cast<OPJ_UINT8*>(buf.data()), buf.size()};

    // open the memory as a stream
    auto l_stream = make_memory_stream(stream, OPJ_TRUE);

    opj_dparameters_t parameters;
    opj_set_default_decoder_parameters(&parameters);

    auto* decoder = opj_create_decompress(OPJ_CODEC_FORMAT::OPJ_CODEC_J2K);
    std::unique_ptr delete_decoder(decoder, opj_destroy_codec);

    // catch events using our callbacks and give a local context
    opj_set_info_handler(decoder, null_callback, nullptr);
    opj_set_warning_handler(decoder, null_callback, nullptr);
    opj_set_error_handler(decoder, null_callback, nullptr);

    // setup the decoder decoding parameters using user parameters.
    opj_setup_decoder(decoder, &parameters);
#if OPJ_VERSION_MAJOR >= 2 && OPJ_VERSION_MINOR >= 3
    opj_codec_set_threads(decoder, 4);
#endif

    // read the main header of the codestream, if necessary the JP2 boxes, and create decompImage
    opj_image_t* image = nullptr;
    opj_read_header(l_stream.get(), decoder, &image);
    std::unique_ptr delete_image(image, opj_image_destroy);

    opj_decode(decoder, l_stream.get(), image);
    opj_end_decompress(decoder, l_stream.get());

    // get the total uncompressed length
    uint32_t bitdepth = ts::ceil(image->comps[0].prec, 8u);
    uint32_t pixels = (image->comps[0].w * image->comps[0].h);

    std::vector<OPJ_INT32 const*> cmp_iters(image->numcomps, nullptr);
    for (size_t cmp = 0; cmp < image->numcomps; ++cmp)
        cmp_iters[cmp] = image->comps[cmp].data;

    printf("7\n");

    std::vector<uint8_t> result(pixels * image->numcomps * bitdepth / 8, 0);

    auto it = result.begin();
    for (size_t index = 0; index < pixels; ++index) {
        for (OPJ_INT32 const*& cmp_iter : cmp_iters) {
            for (uint32_t bits = 0; bits < bitdepth; bits += 8) {
                printf("1\n");
                auto x = (*cmp_iter);
                printf("2 - %d\n", x);
                auto y = static_cast<uint8_t>((x >> bits) & 0xFF);
                printf("3 - %d\n", y);
                *it |= y;
                printf("4\n");

                // *it |= (uint8_t)(((*cmp_iter) >> bits) & 0xFF);
                ++it;
            }
            ++cmp_iter;
        }
    }
    printf("8\n");
    return result;
}

// std::vector<uint8_t> encode(py::buffer data, size_t rate) {
//     auto info = data.request();

//     auto samples = info.shape[2];
//     if (info.format == py::format_descriptor<float>::format())
//         throw py::type_error{"Float is not supported"};

//     opj_cparameters_t parameters; // compression parameters
//     parameters.tcp_mct = samples > 1 ? 1 : 0;  // decide if MCT should be used
//     opj_set_default_encoder_parameters(&parameters);
//     if (rate < 100) {
//         parameters.tcp_rates[0] = 100.0f / rate;
//         parameters.irreversible = 1; //ICT
//     } else
//         parameters.tcp_rates[0] = 0;

//     parameters.tcp_numlayers = 1;
//     parameters.cp_disto_alloc = 1;

//     // set the image components parameters
//     std::vector<opj_image_cmptparm_t> components(samples);
//     for (auto& component : components) {
//         component.dx = parameters.subsampling_dx;
//         component.dy = parameters.subsampling_dy;
//         component.h = static_cast<OPJ_UINT32>(info.shape[0]);
//         component.w = static_cast<OPJ_UINT32>(info.shape[1]);
//         component.prec = static_cast<OPJ_UINT32>(info.itemsize * 8);
//         component.bpp = static_cast<OPJ_UINT32>(info.itemsize * 8);
//         component.sgnd = 0;
//         component.x0 = 0;
//         component.y0 = 0;
//     }
//     OPJ_COLOR_SPACE color_space = (samples == 3 || samples == 4)
//         ? COLOR_SPACE::OPJ_CLRSPC_SRGB
//         : COLOR_SPACE::OPJ_CLRSPC_GRAY;

//     // get a J2K compressor handle
//     opj_codec_t* encoder = opj_create_compress(CODEC_FORMAT::OPJ_CODEC_J2K);
//     std::unique_ptr delete_encoder(encoder, opj_destroy_codec);

//     // catch events using our callbacks and give a local context
//     opj_set_info_handler(encoder, null_callback, nullptr);
//     opj_set_warning_handler(encoder, null_callback, nullptr);
//     opj_set_error_handler(encoder, null_callback, nullptr);

//     // set the "OpenJpeg like" stream data
//     std::vector<uint8_t> buf(info.size * info.itemsize);
//     stream_t buffer{buf.data(), buf.size()};

//     // create an image struct
//     opj_image* image = opj_image_create(
//         static_cast<OPJ_UINT32>(samples), components.data(), color_space);
//     std::unique_ptr delete_image(image, opj_image_destroy);

//     // set image offset and reference grid
//     image->x0 = 0;
//     image->y0 = 0;
//     image->x1 = (OPJ_UINT32)(components[0].w - 1) * (OPJ_UINT32)parameters.subsampling_dx + 1;
//     image->y1 = (OPJ_UINT32)(components[0].h - 1) * (OPJ_UINT32)parameters.subsampling_dy + 1;

//     // setup the encoder parameters using the current image and user parameters
//     opj_setup_encoder(encoder, &parameters, image);

//     // (re)set the buffer pointers
//     std::vector<OPJ_INT32*> cmp_iters(image->numcomps);
//     for (size_t cmp = 0; cmp < image->numcomps; ++cmp)
//         cmp_iters[cmp] = (OPJ_INT32*)(image->comps[cmp].data);

//     // set the color stuff.
//     auto bitdepth = ts::ceil(components[0].bpp, 8u);
//     auto* it = (uint8_t*)info.ptr;
//     for (ssize_t pixel = 0; pixel < info.size / info.shape[2]; ++pixel)
//         for (OPJ_INT32*& cmp_iter : cmp_iters) {
//             for (size_t bits = 0; bits < bitdepth; bits += 8) {
//                 *cmp_iter |= ((OPJ_INT32)(*it) & 0xFF) << bits;
//                 ++it;
//             }
//             ++cmp_iter;
//         }

//     // open a byte stream for writing and encode image
//     auto l_stream = make_memory_stream(&buffer, OPJ_FALSE);
//     opj_start_compress(encoder, image, l_stream.get());
//     opj_encode(encoder, l_stream.get());
//     opj_end_compress(encoder, l_stream.get());

//     // change to encoded size
//     return std::vector<uint8_t>(buffer.data, buffer.data + buffer.offset + 1);
// }

} // namespace ts::jp2k
