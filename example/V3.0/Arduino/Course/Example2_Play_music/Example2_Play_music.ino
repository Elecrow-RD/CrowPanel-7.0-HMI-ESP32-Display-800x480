/*---------------------------------------------------------------
 * I2S melody lesson
 * Synthesize a sine wave and send a melody to an I2S audio device.
 *--------------------------------------------------------------*/

#include <driver/i2s.h>
#include <math.h>


/*---------------------------------------------------------------
 * Audio hardware and signal settings
 * Match the ESP32 I2S signals to the audio circuit on the board.
 *--------------------------------------------------------------*/

// GPIO connections for I2S data, bit clock, and word-select clock.
#define I2S_DOUT 17
#define I2S_BCLK 42
#define I2S_LRC  18

// Audio samples generated per second.
const int sampleRate = 44100;

// Peak signed 16-bit value, used here for full-scale output.
const int16_t AMPLITUDE = 32767;


/*---------------------------------------------------------------
 * Musical note definitions
 * Associate each note name with its fundamental frequency in hertz.
 *--------------------------------------------------------------*/

#define NOTE_C4 262
#define NOTE_D4 294
#define NOTE_E4 330
#define NOTE_F4 349
#define NOTE_G4 392
#define NOTE_A4 440
#define NOTE_B4 494
#define NOTE_C5 523
#define NOTE_D5 587
#define NOTE_E5 659
#define NOTE_F5 698
#define NOTE_G5 784

// Describes one note as a pitch and its playback time in milliseconds.
struct Note {
  int freq;
  int durationMs;
};

// Stores the four phrases of "Happy Birthday" in playback order.
Note melody[] = {
  // First phrase: "Happy birthday to you."
  {NOTE_G4, 200}, {NOTE_G4, 200}, {NOTE_A4, 400}, {NOTE_G4, 400}, {NOTE_C5, 400}, {NOTE_B4, 800},

  // Second phrase: "Happy birthday to you."
  {NOTE_G4, 200}, {NOTE_G4, 200}, {NOTE_A4, 400}, {NOTE_G4, 400}, {NOTE_D5, 400}, {NOTE_C5, 800},

  // Third phrase: "Happy birthday dear [name]."
  {NOTE_G4, 200}, {NOTE_G4, 200}, {NOTE_G5, 400}, {NOTE_E5, 400}, {NOTE_C5, 400}, {NOTE_B4, 200}, {NOTE_A4, 600},

  // Fourth phrase: "Happy birthday to you."
  {NOTE_F5, 200}, {NOTE_F5, 200}, {NOTE_E5, 400}, {NOTE_C5, 400}, {NOTE_D5, 400}, {NOTE_C5, 800},

  // A zero duration marks the end without storing a separate note count.
  {0, 0}
};


/*---------------------------------------------------------------
 * Initialize I2S audio output
 * Configure the peripheral, assign its pins, and play the first melody.
 *--------------------------------------------------------------*/

/**
 * @brief Prepare the serial port and I2S transmitter.
 *
 * Stereo 16-bit frames are selected because playTone() writes the
 * same synthesized sample to both left and right channels.
 *
 * @param None.
 * @return Nothing.
 * @note Called once by the Arduino framework after reset.
 */
void setup() {
  Serial.begin(115200);
  Serial.println("Happy Birthday I2S Test - Full Volume");

  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = sampleRate,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 64,
    .use_apll = false,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_BCLK,
    .ws_io_num = I2S_LRC,
    .data_out_num = I2S_DOUT,
    .data_in_num = I2S_PIN_NO_CHANGE
  };

  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config);
  i2s_set_clk(I2S_NUM_0, sampleRate, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_STEREO);

  playMelody();
}


/*---------------------------------------------------------------
 * Synthesize one tone
 * Generate stereo PCM samples in small batches for the I2S driver.
 *--------------------------------------------------------------*/

/**
 * @brief Play one tone or a timed period of silence.
 *
 * A zero frequency represents a rest. Other frequencies advance a
 * sine-wave phase once per sample so pitch remains independent of
 * the batch size used for I2S transfers.
 *
 * @param freq Tone frequency in hertz, or zero for silence.
 * @param durationMs Playback duration in milliseconds.
 * @return Nothing.
 * @note Called by playMelody() for notes and inter-note pauses.
 */
void playTone(int freq, int durationMs) {
  if (freq == 0) {
    int samplesCount = sampleRate * durationMs / 1000;
    int16_t silence[128] = {0};
    size_t bytes_written;

    // Each frame has two samples, one for each stereo channel.
    for (int i = 0; i < samplesCount; i += 64) {
      int batch = min(64, samplesCount - i);
      i2s_write(I2S_NUM_0, silence, batch * sizeof(int16_t) * 2, &bytes_written, portMAX_DELAY);
    }
    return;
  }

  int samplesCount = sampleRate * durationMs / 1000;
  float phase = 0;
  float phaseIncrement = 2.0 * PI * freq / sampleRate;
  size_t bytes_written;

  // Small batches limit stack use while keeping the I2S stream continuous.
  for (int i = 0; i < samplesCount; i += 64) {
    int16_t samples[128];
    int batch = min(64, samplesCount - i);

    for (int j = 0; j < batch; j++) {
      int16_t sample = (int16_t)(sin(phase) * AMPLITUDE);
      samples[j * 2] = sample;
      samples[j * 2 + 1] = sample;
      phase += phaseIncrement;

      // Wrapping the phase prevents its value from growing indefinitely.
      if (phase > 2.0 * PI) phase -= 2.0 * PI;
    }

    i2s_write(I2S_NUM_0, samples, batch * sizeof(int16_t) * 2, &bytes_written, portMAX_DELAY);
  }
}


/*---------------------------------------------------------------
 * Play the stored melody
 * Visit each note until the zero-duration end marker is reached.
 *--------------------------------------------------------------*/

/**
 * @brief Play every note in the melody table.
 *
 * A short silent interval separates adjacent notes so repeated pitches
 * remain perceptible as distinct musical events.
 *
 * @param None.
 * @return Nothing.
 * @note Called during setup() and every pass through loop().
 */
void playMelody() {
  int i = 0;
  while (melody[i].durationMs > 0) {
    playTone(melody[i].freq, melody[i].durationMs);
    playTone(0, 50);
    i++;
  }
}


/*---------------------------------------------------------------
 * Repeat playback
 * Leave a pause before restarting the complete melody.
 *--------------------------------------------------------------*/

/**
 * @brief Replay the melody at regular intervals.
 *
 * @param None.
 * @return Nothing.
 * @note Called repeatedly by the Arduino framework after setup().
 */
void loop() {
  delay(2000);
  playMelody();
}
