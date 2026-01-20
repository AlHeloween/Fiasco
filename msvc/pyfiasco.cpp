/*
 * pyfiasco.cpp - Python bindings for FIASCO fractal image codec
 * 
 * Provides Python interface to FIASCO encoder/decoder for use with
 * Aurora Fractal-RAG pipeline.
 * 
 * Build requires: pybind11, Python development headers
 */

#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>

#include <vector>
#include <string>
#include <stdexcept>
#include <cstring>
#include <cstdio>

// Include FIASCO headers with C linkage
extern "C" {
#include "config.h"
#include "fiasco.h"
#include "lib/types.h"
#include "lib/image.h"
#include "lib/macros.h"
}

namespace py = pybind11;

// ============================================================================
// Helper functions
// ============================================================================

// Convert numpy array to FIASCO image
static image_t* numpy_to_image(py::array_t<uint8_t> array) {
    auto buf = array.request();
    
    if (buf.ndim < 2 || buf.ndim > 3) {
        throw std::runtime_error("Image must be 2D (grayscale) or 3D (RGB)");
    }
    
    unsigned height = static_cast<unsigned>(buf.shape[0]);
    unsigned width = static_cast<unsigned>(buf.shape[1]);
    bool color = (buf.ndim == 3 && buf.shape[2] >= 3);
    
    image_t* img = alloc_image(width, height, color ? TRUE : FALSE, FORMAT_4_4_4);
    if (!img) {
        throw std::runtime_error("Failed to allocate FIASCO image");
    }
    
    uint8_t* data = static_cast<uint8_t*>(buf.ptr);
    
    if (color) {
        // RGB image
        for (unsigned y = 0; y < height; y++) {
            for (unsigned x = 0; x < width; x++) {
                unsigned idx = y * width + x;
                unsigned src_idx = (y * width + x) * 3;
                
                // Convert RGB to YCbCr
                int R = data[src_idx];
                int G = data[src_idx + 1];
                int B = data[src_idx + 2];
                
                int Y  = (( 66 * R + 129 * G +  25 * B + 128) >> 8) + 16;
                int Cb = ((-38 * R -  74 * G + 112 * B + 128) >> 8) + 128;
                int Cr = ((112 * R -  94 * G -  18 * B + 128) >> 8) + 128;
                
                img->pixels[0][idx] = static_cast<word_t>(Y);
                img->pixels[1][idx] = static_cast<word_t>(Cb);
                img->pixels[2][idx] = static_cast<word_t>(Cr);
            }
        }
    } else {
        // Grayscale image
        for (unsigned y = 0; y < height; y++) {
            for (unsigned x = 0; x < width; x++) {
                unsigned idx = y * width + x;
                img->pixels[0][idx] = static_cast<word_t>(data[idx]);
            }
        }
    }
    
    return img;
}

// Convert FIASCO image to numpy array
static py::array_t<uint8_t> image_to_numpy(image_t* img) {
    if (!img) {
        throw std::runtime_error("Null image pointer");
    }
    
    unsigned width = img->width;
    unsigned height = img->height;
    bool color = img->color == TRUE;
    
    if (color) {
        // RGB output
        py::array_t<uint8_t> result({height, width, 3u});
        auto buf = result.mutable_unchecked<3>();
        
        for (unsigned y = 0; y < height; y++) {
            for (unsigned x = 0; x < width; x++) {
                unsigned idx = y * width + x;
                
                // Convert YCbCr to RGB
                int Y  = img->pixels[0][idx] - 16;
                int Cb = img->pixels[1][idx] - 128;
                int Cr = img->pixels[2][idx] - 128;
                
                int R = (298 * Y + 409 * Cr + 128) >> 8;
                int G = (298 * Y - 100 * Cb - 208 * Cr + 128) >> 8;
                int B = (298 * Y + 516 * Cb + 128) >> 8;
                
                // Clamp to [0, 255]
                buf(y, x, 0) = static_cast<uint8_t>(std::max(0, std::min(255, R)));
                buf(y, x, 1) = static_cast<uint8_t>(std::max(0, std::min(255, G)));
                buf(y, x, 2) = static_cast<uint8_t>(std::max(0, std::min(255, B)));
            }
        }
        
        return result;
    } else {
        // Grayscale output
        py::array_t<uint8_t> result({height, width});
        auto buf = result.mutable_unchecked<2>();
        
        for (unsigned y = 0; y < height; y++) {
            for (unsigned x = 0; x < width; x++) {
                unsigned idx = y * width + x;
                int val = img->pixels[0][idx];
                buf(y, x) = static_cast<uint8_t>(std::max(0, std::min(255, val)));
            }
        }
        
        return result;
    }
}

// ============================================================================
// FiascoEncoder class
// ============================================================================

class FiascoEncoder {
public:
    FiascoEncoder() : options_(nullptr) {
        options_ = fiasco_c_options_new();
        if (!options_) {
            throw std::runtime_error("Failed to create FIASCO encoder options");
        }
    }
    
    ~FiascoEncoder() {
        if (options_) {
            fiasco_c_options_delete(options_);
        }
    }
    
    void set_quality(float quality) {
        quality_ = quality;
    }
    
    void set_smoothing(int smoothing) {
        fiasco_c_options_set_smoothing(options_, smoothing);
    }
    
    void set_tiling(int method, unsigned exponent) {
        fiasco_c_options_set_tiling(options_, 
            static_cast<fiasco_tiling_e>(method), exponent);
    }
    
    void set_title(const std::string& title) {
        fiasco_c_options_set_title(options_, title.c_str());
    }
    
    void set_comment(const std::string& comment) {
        fiasco_c_options_set_comment(options_, comment.c_str());
    }
    
    bool encode(const std::string& input_path, const std::string& output_path) {
        const char* inputs[] = {input_path.c_str(), nullptr};
        
        int result = fiasco_coder(inputs, output_path.c_str(), quality_, options_);
        return result != 0;
    }
    
    // Encode from numpy array
    bool encode_array(py::array_t<uint8_t> image, const std::string& output_path) {
        // Save to temporary PNM file
        std::string temp_path = output_path + ".tmp.ppm";
        
        auto buf = image.request();
        unsigned height = static_cast<unsigned>(buf.shape[0]);
        unsigned width = static_cast<unsigned>(buf.shape[1]);
        bool color = (buf.ndim == 3 && buf.shape[2] >= 3);
        
        FILE* fp = fopen(temp_path.c_str(), "wb");
        if (!fp) {
            throw std::runtime_error("Failed to create temporary file");
        }
        
        uint8_t* data = static_cast<uint8_t*>(buf.ptr);
        
        if (color) {
            fprintf(fp, "P6\n%u %u\n255\n", width, height);
            fwrite(data, 1, width * height * 3, fp);
        } else {
            fprintf(fp, "P5\n%u %u\n255\n", width, height);
            fwrite(data, 1, width * height, fp);
        }
        fclose(fp);
        
        // Encode
        bool success = encode(temp_path, output_path);
        
        // Cleanup temp file
        remove(temp_path.c_str());
        
        return success;
    }

private:
    fiasco_c_options_t* options_;
    float quality_ = 20.0f;
};

// ============================================================================
// FiascoDecoder class
// ============================================================================

class FiascoDecoder {
public:
    FiascoDecoder() : decoder_(nullptr), options_(nullptr) {
        options_ = fiasco_d_options_new();
        if (!options_) {
            throw std::runtime_error("Failed to create FIASCO decoder options");
        }
    }
    
    ~FiascoDecoder() {
        close();
        if (options_) {
            fiasco_d_options_delete(options_);
        }
    }
    
    void set_smoothing(int smoothing) {
        fiasco_d_options_set_smoothing(options_, smoothing);
    }
    
    void set_magnification(int level) {
        fiasco_d_options_set_magnification(options_, level);
    }
    
    bool open(const std::string& filename) {
        close();
        
        decoder_ = fiasco_decoder_new(filename.c_str(), options_);
        return decoder_ != nullptr;
    }
    
    void close() {
        if (decoder_) {
            fiasco_decoder_delete(decoder_);
            decoder_ = nullptr;
        }
    }
    
    unsigned get_width() const {
        if (!decoder_) return 0;
        return fiasco_decoder_get_width(decoder_);
    }
    
    unsigned get_height() const {
        if (!decoder_) return 0;
        return fiasco_decoder_get_height(decoder_);
    }
    
    unsigned get_length() const {
        if (!decoder_) return 0;
        return fiasco_decoder_get_length(decoder_);
    }
    
    unsigned get_rate() const {
        if (!decoder_) return 0;
        return fiasco_decoder_get_rate(decoder_);
    }
    
    bool is_color() const {
        if (!decoder_) return false;
        return fiasco_decoder_is_color(decoder_) != 0;
    }
    
    std::string get_title() const {
        if (!decoder_) return "";
        const char* title = fiasco_decoder_get_title(decoder_);
        return title ? std::string(title) : "";
    }
    
    std::string get_comment() const {
        if (!decoder_) return "";
        const char* comment = fiasco_decoder_get_comment(decoder_);
        return comment ? std::string(comment) : "";
    }
    
    py::array_t<uint8_t> get_frame() {
        if (!decoder_) {
            throw std::runtime_error("No file opened");
        }
        
        fiasco_image_t* fimg = fiasco_decoder_get_frame(decoder_);
        if (!fimg) {
            throw std::runtime_error("Failed to decode frame");
        }
        
        image_t* img = cast_image(fimg);
        if (!img) {
            fiasco_image_delete(fimg);
            throw std::runtime_error("Failed to cast image");
        }
        
        py::array_t<uint8_t> result = image_to_numpy(img);
        
        fiasco_image_delete(fimg);
        
        return result;
    }
    
    std::vector<py::array_t<uint8_t>> decode_all() {
        std::vector<py::array_t<uint8_t>> frames;
        unsigned length = get_length();
        
        for (unsigned i = 0; i < length; i++) {
            frames.push_back(get_frame());
        }
        
        return frames;
    }

private:
    fiasco_decoder_t* decoder_;
    fiasco_d_options_t* options_;
};

// ============================================================================
// Module-level functions
// ============================================================================

std::string get_error_message() {
    const char* msg = fiasco_get_error_message();
    return msg ? std::string(msg) : "";
}

void set_verbosity(int level) {
    fiasco_set_verbosity(static_cast<fiasco_verbosity_e>(level));
}

int get_verbosity() {
    return static_cast<int>(fiasco_get_verbosity());
}

// Quick encode function
bool encode_image(const std::string& input_path, 
                  const std::string& output_path,
                  float quality = 20.0f) {
    FiascoEncoder encoder;
    encoder.set_quality(quality);
    return encoder.encode(input_path, output_path);
}

// Quick decode function
py::array_t<uint8_t> decode_image(const std::string& input_path) {
    FiascoDecoder decoder;
    if (!decoder.open(input_path)) {
        throw std::runtime_error("Failed to open FIASCO file: " + get_error_message());
    }
    return decoder.get_frame();
}

// Get compression stats
py::dict get_compression_stats(const std::string& fiasco_path,
                               const std::string& original_path) {
    py::dict stats;
    
    FiascoDecoder decoder;
    if (!decoder.open(fiasco_path)) {
        throw std::runtime_error("Failed to open FIASCO file");
    }
    
    stats["width"] = decoder.get_width();
    stats["height"] = decoder.get_height();
    stats["frames"] = decoder.get_length();
    stats["is_color"] = decoder.is_color();
    stats["title"] = decoder.get_title();
    stats["comment"] = decoder.get_comment();
    
    // Get file sizes
    FILE* fp = fopen(fiasco_path.c_str(), "rb");
    if (fp) {
        fseek(fp, 0, SEEK_END);
        long fiasco_size = ftell(fp);
        fclose(fp);
        stats["compressed_size"] = fiasco_size;
    }
    
    fp = fopen(original_path.c_str(), "rb");
    if (fp) {
        fseek(fp, 0, SEEK_END);
        long original_size = ftell(fp);
        fclose(fp);
        stats["original_size"] = original_size;
        
        if (stats.contains("compressed_size")) {
            long comp_size = stats["compressed_size"].cast<long>();
            stats["compression_ratio"] = static_cast<double>(original_size) / comp_size;
        }
    }
    
    return stats;
}

// ============================================================================
// Python module definition
// ============================================================================

PYBIND11_MODULE(pyfiasco, m) {
    m.doc() = R"pbdoc(
        FIASCO - Fractal Image And Sequence COdec
        
        Python bindings for the FIASCO fractal compression library.
        Used by Aurora Fractal-RAG for efficient image/video encoding.
        
        Example usage:
            import pyfiasco
            
            # Encode
            encoder = pyfiasco.Encoder()
            encoder.set_quality(25.0)
            encoder.encode("input.ppm", "output.fco")
            
            # Decode
            decoder = pyfiasco.Decoder()
            decoder.open("output.fco")
            frame = decoder.get_frame()  # numpy array
    )pbdoc";
    
    // Verbosity levels
    m.attr("VERBOSITY_NONE") = static_cast<int>(FIASCO_NO_VERBOSITY);
    m.attr("VERBOSITY_SOME") = static_cast<int>(FIASCO_SOME_VERBOSITY);
    m.attr("VERBOSITY_ULTIMATE") = static_cast<int>(FIASCO_ULTIMATE_VERBOSITY);
    
    // Tiling methods
    m.attr("TILING_SPIRAL_ASC") = static_cast<int>(FIASCO_TILING_SPIRAL_ASC);
    m.attr("TILING_SPIRAL_DSC") = static_cast<int>(FIASCO_TILING_SPIRAL_DSC);
    m.attr("TILING_VARIANCE_ASC") = static_cast<int>(FIASCO_TILING_VARIANCE_ASC);
    m.attr("TILING_VARIANCE_DSC") = static_cast<int>(FIASCO_TILING_VARIANCE_DSC);
    
    // Module-level functions
    m.def("get_error_message", &get_error_message,
          "Get the last error message from FIASCO library");
    
    m.def("set_verbosity", &set_verbosity,
          py::arg("level"),
          "Set verbosity level (VERBOSITY_NONE, VERBOSITY_SOME, VERBOSITY_ULTIMATE)");
    
    m.def("get_verbosity", &get_verbosity,
          "Get current verbosity level");
    
    m.def("encode_image", &encode_image,
          py::arg("input_path"),
          py::arg("output_path"),
          py::arg("quality") = 20.0f,
          "Quick encode an image file to FIASCO format");
    
    m.def("decode_image", &decode_image,
          py::arg("input_path"),
          "Quick decode a FIASCO file to numpy array");
    
    m.def("get_compression_stats", &get_compression_stats,
          py::arg("fiasco_path"),
          py::arg("original_path"),
          "Get compression statistics for a FIASCO file");
    
    // Encoder class
    py::class_<FiascoEncoder>(m, "Encoder", "FIASCO image/video encoder")
        .def(py::init<>())
        .def("set_quality", &FiascoEncoder::set_quality,
             py::arg("quality"),
             "Set compression quality (higher = better quality, larger file)")
        .def("set_smoothing", &FiascoEncoder::set_smoothing,
             py::arg("smoothing"),
             "Set smoothing percentage along partitioning borders")
        .def("set_tiling", &FiascoEncoder::set_tiling,
             py::arg("method"), py::arg("exponent"),
             "Set tiling method and exponent")
        .def("set_title", &FiascoEncoder::set_title,
             py::arg("title"),
             "Set title metadata")
        .def("set_comment", &FiascoEncoder::set_comment,
             py::arg("comment"),
             "Set comment metadata")
        .def("encode", &FiascoEncoder::encode,
             py::arg("input_path"), py::arg("output_path"),
             "Encode image file to FIASCO format")
        .def("encode_array", &FiascoEncoder::encode_array,
             py::arg("image"), py::arg("output_path"),
             "Encode numpy array to FIASCO format");
    
    // Decoder class
    py::class_<FiascoDecoder>(m, "Decoder", "FIASCO image/video decoder")
        .def(py::init<>())
        .def("set_smoothing", &FiascoDecoder::set_smoothing,
             py::arg("smoothing"),
             "Set smoothing percentage for decoding")
        .def("set_magnification", &FiascoDecoder::set_magnification,
             py::arg("level"),
             "Set magnification level for decoding")
        .def("open", &FiascoDecoder::open,
             py::arg("filename"),
             "Open a FIASCO file for decoding")
        .def("close", &FiascoDecoder::close,
             "Close the current file")
        .def_property_readonly("width", &FiascoDecoder::get_width,
             "Image width in pixels")
        .def_property_readonly("height", &FiascoDecoder::get_height,
             "Image height in pixels")
        .def_property_readonly("length", &FiascoDecoder::get_length,
             "Number of frames (1 for still image)")
        .def_property_readonly("rate", &FiascoDecoder::get_rate,
             "Frame rate for video sequences")
        .def_property_readonly("is_color", &FiascoDecoder::is_color,
             "True if color image/video")
        .def_property_readonly("title", &FiascoDecoder::get_title,
             "Title metadata")
        .def_property_readonly("comment", &FiascoDecoder::get_comment,
             "Comment metadata")
        .def("get_frame", &FiascoDecoder::get_frame,
             "Decode next frame as numpy array")
        .def("decode_all", &FiascoDecoder::decode_all,
             "Decode all frames as list of numpy arrays");
    
    // Version info
    m.attr("__version__") = "1.0.0";
}
