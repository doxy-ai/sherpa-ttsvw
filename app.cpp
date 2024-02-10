#undef ASIO_NO_DEPRECATED
#include "crow.h"
#include "sherpa-onnx/c-api/c-api.h"
#include "argparse/argparse.hpp"
#include "base64.h"

enum Provider {
	cpu,
	cuda,
	coreml,
};

std::string to_string(Provider p) {
	switch(p) {
		case cpu: return "cpu";
		case cuda: return "cuda";
		case coreml: return "coreml";
		default: return "<invalid>";
	}
}

struct Arguments : public argparse::Args {
    std::string& vits_model = kwarg("m,vits-model", "Path to VITS model");
    std::optional<std::string>& vits_lexicon = kwarg("l,vits-lexicon", "Path to lexicon.txt for VITS models");
    std::string& vits_tokens = kwarg("t,vits-tokens", "Path to tokens.txt for VITS models");
	std::optional<std::string>& vits_data_dir = kwarg("d,vits-data-dir", "Path to espeak-ng-data. If it is given, --vits-lexicon is ignored");
	float& vits_noise_scale = kwarg("vits-noise-scale", "noise_scale for VITS models").set_default(.667);
	float& vits_noise_scale_w = kwarg("vits-noise-scale-w", "noise_scale_w for VITS models").set_default(.8);
	float& vits_length_scale = kwarg("vits-length-scale", "length_scale for VITS models. Default to 1. You can tune it to change the speech speed. small -> faster; large -> slower. ").set_default(1);
	int32_t& num_threads = kwarg("j,num-threads", "Number of threads", std::to_string(std::thread::hardware_concurrency())).set_default(1);
	Provider& provider = kwarg("provider", "Where the generation should be run").set_default(Provider::cpu);
	uint32_t& speaker_id = kwarg("sid,speaker-id", "Speaker ID. Note it is not used for single-speaker models.").set_default(0);
	std::optional<std::string>& tts_rule_fsts = kwarg("tts-rule-fsts", "It not empty, it contains a list of rule FST filenames. Multiple filenames are separated by a comma and they are applied from left to right. An example value: ule1.fst,rule2,fst,rule3.fst");
	int32_t& max_sentences = kwarg("max-sentences,max-num-sentences", "Maximum number of sentences that we process at a time. This is to avoid OOM for very long input text. If you set it to -1, then we process all sentences in a single batch.").set_default(2);

	bool& base64_encode = flag("b,base-64,tts-voice-wizard", "Should the generated audio be base64 encoded? This is needed by tts-voice-wizard!");
	std::string& listen_address = kwarg("listen-address", "IP address to bind to").set_default("0.0.0.0");
	uint16_t& port = kwarg("port", "Port to bind to").set_default(8124);

	void welcome() {
        std::cout << "Offline text-to-speech webserver powered by sherpa-onnx\n"
			"\n"
			"You can download a test model from\n"
			"https://huggingface.co/csukuangfj/vits-ljs\n"
			"\n"
			"For instance, you can use:\n"
			"wget "
			"https://huggingface.co/csukuangfj/vits-ljs/resolve/main/vits-ljs.onnx\n"
			"wget "
			"https://huggingface.co/csukuangfj/vits-ljs/resolve/main/lexicon.txt\n"
			"wget "
			"https://huggingface.co/csukuangfj/vits-ljs/resolve/main/tokens.txt\n"
			"\n"
			"./app \\\n"
			"  --vits-model=./vits-ljs.onnx \\\n"
			"  --vits-lexicon=./lexicon.txt \\\n"
			"  --vits-tokens=./tokens.txt \\\n"
			"\n"
			"Then navigate to\n"
			R"( http://localhost:8124/synthesize?text=This%20is%20text%20generated%20by%20a%20web%20server%21)"
			"\n\n"
			"Please see\n"
			" https://k2-fsa.github.io/sherpa/onnx/tts/index.html\n"
			"for details.\n\n" << std::endl;
    }
};

struct GeneratedAudio: public SherpaOnnxGeneratedAudio, public crow::returnable {
	bool base64_encode = false;

	GeneratedAudio(bool base64_encode = false) : base64_encode(base64_encode), crow::returnable(base64_encode ? "text/plain" : "audio/wav") {}
	GeneratedAudio(const SherpaOnnxGeneratedAudio* audio, bool base64_encode = false) : base64_encode(base64_encode), SherpaOnnxGeneratedAudio(*audio), crow::returnable(base64_encode ? "text/plain" : "audio/wav") {}
	
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
		if(base64_encode)
			return macaron::Base64::Encode(wavData.str());
		return wavData.str();
	}
};

// From: https://stackoverflow.com/questions/154536/encode-decode-urls-in-c
std::string url_decode(const std::string& SRC) {
    std::string ret;
    char ch;
    int i, ii;
    for (i=0; i<SRC.length(); i++) {
        if (SRC[i]=='%') {
            sscanf(SRC.substr(i+1,2).c_str(), "%x", &ii);
            ch=static_cast<char>(ii);
            ret+=ch;
            i=i+2;
        } else {
            ret+=SRC[i];
        }
    }
    return (ret);
}

int main(int argc, const char *const *argv) {
	auto args = argparse::parse<Arguments>(argc, argv);
	if(!args.vits_data_dir && !args.vits_lexicon) {
		std::cerr << "Argument missing: one of --vits-lexicon (Path to lexicon.txt for VITS models) or --vits-data-dir (Path to espeak-ng-data) required" << std::endl;
		return 1;
	}

	crow::SimpleApp app;

	auto provider = to_string(args.provider);
	SherpaOnnxOfflineTtsConfig config = {
		.model = {
			.vits = {
				.model = args.vits_model.c_str(),
				.lexicon = args.vits_lexicon.has_value() ? args.vits_lexicon->c_str() : nullptr,
				.tokens = args.vits_tokens.c_str(),
				.data_dir = args.vits_data_dir.has_value() ? args.vits_data_dir->c_str() : nullptr,
				.noise_scale = args.vits_noise_scale,
				.noise_scale_w = args.vits_noise_scale_w,
				.length_scale = args.vits_length_scale
			},
			.num_threads = args.num_threads,
			.debug = true,
			.provider = provider.c_str()
		},
		.rule_fsts = args.tts_rule_fsts.has_value() ? args.tts_rule_fsts->c_str() : nullptr,
		.max_num_sentences = args.max_sentences
	};

	bool base64_encode = args.base64_encode;
	int32_t sid = 0;
	SherpaOnnxOfflineTts *tts = SherpaOnnxCreateOfflineTts(&config);

	CROW_ROUTE(app, "/synthesize/")([&tts, &sid, base64_encode](const crow::request& req) -> crow::response {
		std::string _text;
		auto text = req.url_params.get("text");
		if(!text) {
			// std::cout << req.raw_url << std::endl;
			// std::cout << qs_k2v(req.url_params.keys()[0]) << std::endl;
			_text = url_decode(req.raw_url.substr(13, req.raw_url.size()));
			std::cout << _text << std::endl;
			text = (char*)_text.c_str();
		}
		if(!text) return {400, "Please provide text to generate audio for ex: " + req.url + "?text=TextHere!"};
		std::cout << "Generating: " << text << std::endl;

		// Generate audio
		const SherpaOnnxGeneratedAudio *audio = SherpaOnnxOfflineTtsGenerate(tts, text, sid, 1.0);
		// Convert the generated audio into an HTTP response
		crow::response response = GeneratedAudio(audio, base64_encode);
		// Free the original audio data
		SherpaOnnxDestroyOfflineTtsGeneratedAudio(audio); 
		return response;		
	});

	app.bindaddr(args.listen_address).port(args.port).run();

	SherpaOnnxDestroyOfflineTts(tts);
}