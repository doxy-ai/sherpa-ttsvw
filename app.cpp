#undef ASIO_NO_DEPRECATED
#include "crow.h"
#include "sherpa-onnx/c-api/c-api.h"

struct GeneratedAudio: public SherpaOnnxGeneratedAudio, public crow::returnable {
	GeneratedAudio() : crow::returnable("audio/wav") {}
	GeneratedAudio(const SherpaOnnxGeneratedAudio* audio) : SherpaOnnxGeneratedAudio(*audio), crow::returnable("audio/wav") {}
	
	// from: thirdparty/sherpa-onnx-build/sherpa-onnx/sherpa-onnx/csrc/wave-writer.cc
	struct WaveHeader {
		int32_t chunk_id;
		int32_t chunk_size;
		int32_t format;
		int32_t subchunk1_id;
		int32_t subchunk1_size;
		int16_t audio_format;
		int16_t num_channels;
		int32_t sample_rate;
		int32_t byte_rate;
		int16_t block_align;
		int16_t bits_per_sample;
		int32_t subchunk2_id;    // a tag of this chunk
		int32_t subchunk2_size;  // size of subchunk2
	};

	// from: thirdparty/sherpa-onnx-build/sherpa-onnx/sherpa-onnx/csrc/wave-writer.cc
	bool write_wav(std::ostream& os) const {
		WaveHeader header;
		header.chunk_id = 0x46464952;      // FFIR
		header.format = 0x45564157;        // EVAW
		header.subchunk1_id = 0x20746d66;  // "fmt "
		header.subchunk1_size = 16;        // 16 for PCM
		header.audio_format = 1;           // PCM =1

		int32_t num_channels = 1;
		int32_t bits_per_sample = 16;  // int16_t
		header.num_channels = num_channels;
		header.sample_rate = sample_rate;
		header.byte_rate = sample_rate * num_channels * bits_per_sample / 8;
		header.block_align = num_channels * bits_per_sample / 8;
		header.bits_per_sample = bits_per_sample;
		header.subchunk2_id = 0x61746164;  // atad
		header.subchunk2_size = n * num_channels * bits_per_sample / 8;

		header.chunk_size = 36 + header.subchunk2_size;

		std::vector<int16_t> samples_int16(n);
		for (int32_t i = 0; i != n; ++i)
			samples_int16[i] = samples[i] * 32676;

		if (!os) {
			std::cerr << "Invalid output stream provided!" << std::endl;
			return false;
		}

		os.write(reinterpret_cast<const char *>(&header), sizeof(header));
		os.write(reinterpret_cast<const char *>(samples_int16.data()),samples_int16.size() * sizeof(int16_t));

		if (!os) {
			std::cerr << "Saving wav failed!" << std::endl;
			return false;
		}

		return true;
	}

	std::string dump() const override {
		std::ostringstream wavData;
		if(!write_wav(wavData)) 
			return "Failure!";
		return wavData.str();
	}
};



int main() {
	// TODO: Add argument parser library to read the nessicary configuration arguments in from the command line!

	crow::SimpleApp app;

	SherpaOnnxOfflineTtsConfig config = {};

	// TODO: Configure

	int32_t sid = 0;
	SherpaOnnxOfflineTts *tts = SherpaOnnxCreateOfflineTts(&config);

	CROW_ROUTE(app, "/synthesize")([&tts, &sid](const crow::request& req) -> crow::response {
		auto text = req.url_params.get("text");
		if(!text) return {400, "Please provide text to generate audio for ex: " + req.url + "?text=TextHere!"};

		// Generate audio
		const SherpaOnnxGeneratedAudio *audio = SherpaOnnxOfflineTtsGenerate(tts, text, sid++, 1.0);
		// SherpaOnnxWriteWave(audio->samples, audio->n, audio->sample_rate, filename);
		// Convert the generated audio into an HTTP response
		crow::response response = GeneratedAudio(audio);

		// Free the original audio data
		SherpaOnnxDestroyOfflineTtsGeneratedAudio(audio); 

		return response;		
		
		// std::cout << "AUDIO FOR '" << text << "' REQUESTED" << std::endl;
		// return "No Audio ;(";
	});

	app.port(8124).run();

	SherpaOnnxDestroyOfflineTts(tts);
}