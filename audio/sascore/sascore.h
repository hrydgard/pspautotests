#ifndef __SASCORE_H
	#define __SASCORE_H

	typedef struct {
		unsigned int data[512];
	} SasCore;

	#define PSP_SAS_EFFECT_TYPE_OFF   -1
	#define PSP_SAS_EFFECT_TYPE_ROOM   0
	#define PSP_SAS_EFFECT_TYPE_UNK1   1
	#define PSP_SAS_EFFECT_TYPE_UNK2   2
	#define PSP_SAS_EFFECT_TYPE_UNK3   3
	#define PSP_SAS_EFFECT_TYPE_HALL   4
	#define PSP_SAS_EFFECT_TYPE_SPACE  5
	#define PSP_SAS_EFFECT_TYPE_ECHO   6
	#define PSP_SAS_EFFECT_TYPE_DELAY  7
	#define PSP_SAS_EFFECT_TYPE_PIPE   8

	#define PSP_SAS_VOICES_MAX          32
	#define PSP_SAS_GRAIN_SAMPLES       256
	#define PSP_SAS_VOL_MAX             0x1000
	#define PSP_SAS_LOOP_MODE_OFF       0
	#define PSP_SAS_LOOP_MODE_ON        1
	#define PSP_SAS_PITCH_MIN           0x1
	#define PSP_SAS_PITCH_BASE          0x1000
	#define PSP_SAS_PITCH_MAX           0x4000
	#define PSP_SAS_NOISE_FREQ_MAX      0x3F;
	#define PSP_SAS_ENVELOPE_HEIGHT_MAX 0x40000000
	#define PSP_SAS_ENVELOPE_FREQ_MAX   0x7FFFFFFF;

	#define PSP_SAS_ADSR_CURVE_MODE_LINEAR_INCREASE 0
	#define PSP_SAS_ADSR_CURVE_MODE_LINEAR_DECREASE 1
	#define PSP_SAS_ADSR_CURVE_MODE_LINEAR_BENT     2
	#define PSP_SAS_ADSR_CURVE_MODE_EXPONENT_REV    3
	#define PSP_SAS_ADSR_CURVE_MODE_EXPONENT        4
	#define PSP_SAS_ADSR_CURVE_MODE_DIRECT          5

	#define PSP_SAS_ADSR_ATTACK  1
	#define PSP_SAS_ADSR_DECAY   2
	#define PSP_SAS_ADSR_SUSTAIN 4
	#define PSP_SAS_ADSR_RELEASE 8

	#define PSP_SAS_OUTPUTMODE_STEREO       0
	#define PSP_SAS_OUTPUTMODE_MULTICHANNEL 1

	/**
	 * Initialized a sasCore structure.
	 * Note: PSP can only handle one at a time.
	 *
	 * @example __sceSasInit(&sasCore, PSP_SAS_GRAIN_SAMPLES, PSP_SAS_VOICES_MAX, OutputMode.PSP_SAS_OUTPUTMODE_STEREO, 44100);
	 *
	 * @param   sasCore       Pointer to a SasCore structure that will contain information.
	 * @param   grainSamples  Number of grainSamples
	 * @param   maxVoices     Max number of voices
	 * @param   outMode       Out Mode
	 * @param   sampleRate    Sample Rate
	 *
	 * @return  0 on success
	 */
	int __sceSasInit(SasCore* sasCore, int grainSamples, int maxVoices, int outMode, int sampleRate);
#endif