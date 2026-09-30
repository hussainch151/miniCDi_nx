#include "cdi/common.hpp"

#define SDL2 1
#define ASND 2

#ifndef MINICDI_AUDIO
#define MINICDI_AUDIO SDL2
#endif

#define SAMPLE_COUNT 2048
#define MAX_SAMPLE_QUEUE (16128 * 4)

#if MINICDI_AUDIO == SDL2
#include <SDL2/SDL.h>
static uint32_t SDL_audio_id = 0;
static bool SDL_audio_valid = false;
#elif MINICDI_AUDIO == ASND
#include <gccore.h>
#include <asndlib.h>
#else
#pragma message "note: No audio driver specified (AdpcmDecoder.cpp)"
#endif

static std::vector<int16_t> left, right;
static int16_t *output;
#define OUTPUT_MAX_SIZE 16128
static int output_size = 0;

template <size_t max_units, int gain>
void AdpcmDecoder::decode_adpcm(bool stereo, bool low_freq)
{
	for (size_t sample_unit = 0; sample_unit < max_units; sample_unit++)
	{
		const uint16_t cur_gain = 2 << (gain - ranges[sample_unit]);
		for (uint8_t sample_data = 0; sample_data < 28; sample_data++)
		{
			if (stereo && (sample_unit & 1))
			{
				const int16_t sample = std::clamp((sound_data[sample_unit][sample_data] * cur_gain)
				+ ((rk0 * K0[filters[sample_unit]] + rk1 * K1[filters[sample_unit]]) / 256), INT16_MIN, INT16_MAX);
				rk1 = rk0;
				rk0 = sample;
				right.push_back(sample);
				if (low_freq) right.push_back(sample);
			}
			else
			{
				const int16_t sample = std::clamp((sound_data[sample_unit][sample_data] * cur_gain)
				+ ((lk0 * K0[filters[sample_unit]] + lk1 * K1[filters[sample_unit]]) / 256), INT16_MIN, INT16_MAX);
				lk1 = lk0;
				lk0 = sample;
				left.push_back(sample);
				if (low_freq) left.push_back(sample);
			}
		}
	}

	if (output != NULL)
	{
		output_size = stereo ? std::min(left.size(), right.size()) : left.size();
		for (int i = 0; i < output_size; i++)
		{
			output[i*2] = left[i];
			output[i*2+1] = stereo ? right[i] : left[i];
		}
		output_size = std::min(output_size * 2, OUTPUT_MAX_SIZE);
	}
}

bool AdpcmDecoder::decode_sector(uint8_t *buffer)
{
	uint8_t coding = buffer[11];
	if (coding & 0b10'10'10)
		return false;

	left.clear();
	right.clear();

	int sample_bits = (coding & 0b01'00'00) != 0 ? 8 : 4;
	int sample_freq = (coding & 0b00'01'00) != 0 ? 18900 : 37800;
	int sample_chan = (coding & 0b00'00'01) != 0 ? 2 : 1;
	enum SoundQualityLevel level = sample_freq != 37800 ? CDI_C : sample_bits == 8 ? CDI_A : CDI_B;

	for (size_t SG = 0; SG < 18; SG++)
	{
		uint8_t *data = buffer+(SG*128)+12;
		switch (level)
		{
			default:
				return false;

			case CDI_A:
			{
				for (int i = 0; i < 4; i++)
				{
					ranges[i] = static_cast<uint8_t>(data[i] & 0x0F);
					filters[i] = static_cast<uint8_t>(data[i] >> 4);
				}

				uint8_t index = 16;
				for (uint8_t sample_data = 0; sample_data < 28; sample_data++) {
					for (uint8_t sample_unit = 0; sample_unit < 4; sample_unit++) {
						sound_data[sample_unit][sample_data] = data[index++];
					}
				}

				decode_adpcm<4, 8>(sample_chan == 2, (coding & 0b00'01'00) != 0);
			}
			break;

			case CDI_B:
			case CDI_C:
			{
				for (int i = 0; i < 8; i++)
				{
					ranges[i] = static_cast<uint8_t>(data[i+4] & 0x0F);
					filters[i] = static_cast<uint8_t>(data[i+4] >> 4);
				}

				uint8_t index = 16;
				for (uint8_t sample_data = 0; sample_data < 28; sample_data++) {
					for (uint8_t sample_unit = 0; sample_unit < 8;) {
						const uint8_t SB = data[index++];

						int8_t SD0 = SB & 0x0F;
						if (SD0 >= 8) SD0 -= 16;

						int8_t SD1 = SB >> 4 & 0x0F;
						if (SD1 >= 8) SD1 -= 16;

						sound_data[sample_unit++][sample_data] = SD0;
						sound_data[sample_unit++][sample_data] = SD1;
					}
				}

				decode_adpcm<8, 12>(sample_chan == 2, (coding & 0b00'01'00) != 0);
			}
			break;
		}
	}

	return true;
}

void AdpcmDecoder::play()
{
	#if MINICDI_AUDIO == SDL2
	#if MAX_SAMPLE_QUEUE > 0
	if (SDL_audio_valid && SDL_GetQueuedAudioSize(SDL_audio_id) < MAX_SAMPLE_QUEUE)
		#endif
		SDL_QueueAudio(SDL_audio_id, &output[0], output_size * sizeof(int16_t));
	#endif

	#if MINICDI_AUDIO == ASND
	if (ASND_StatusVoice(1) == SND_WAITING || ASND_StatusVoice(1) == SND_UNUSED)
		ASND_SetVoice(1, VOICE_STEREO_16BIT, 37800, 0, (uint8_t *)output, output_size, 255, 255, nullptr);
	#endif
}

AdpcmDecoder::AdpcmDecoder()
{
	#if MINICDI_AUDIO == SDL2
	if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
	{
		MiniCDI::Log("[Audio:SDL2] failed to init audio subsystem");
		SDL_audio_valid = false;
		return;
	}
	else
	{
		SDL_AudioSpec desired, received;
		SDL_zero(desired);
		desired.freq = 37800;
		desired.format = AUDIO_S16SYS;
		desired.channels = 2;
		desired.samples = SAMPLE_COUNT;

		SDL_audio_id = SDL_OpenAudioDevice(NULL, 0, &desired, &received, 0);
		SDL_audio_valid = SDL_audio_id > 0;
		if (SDL_audio_valid)
		{
			MiniCDI::Log("[Audio:SDL2] initialized audio device #%d", SDL_audio_id);
			SDL_PauseAudioDevice(SDL_audio_id, 0);
		}
	}
	#endif

	#if MINICDI_AUDIO == ASND
	ASND_Pause(0);
	#endif

	MINICDI_MEMALIGN(int16_t, output, OUTPUT_MAX_SIZE);
	MiniCDI::Log("[Audio] Initialized audio buffer");
}

AdpcmDecoder::~AdpcmDecoder()
{
	#if MINICDI_AUDIO == SDL2
	if (SDL_audio_valid)
	{
		SDL_CloseAudioDevice(SDL_audio_id);
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
		SDL_audio_valid = false;
	}
	#endif

	#if MINICDI_AUDIO == ASND
	ASND_Pause(1);
	#endif

	if (output != NULL)
	{
		MINICDI_MEMFREE(output);
		MiniCDI::Log("[Audio] Closed audio buffer");
	}
}
