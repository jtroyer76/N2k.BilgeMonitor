// ---------------------------------------------------------------------------
// Nootka bilge and tank monitoring system
// ---------------------------------------------------------------------------
#include "sensesp/sensors/digital_input.h"
#include "sensesp/signalk/signalk_output.h"
#include "sensesp/transforms/debounce.h"
#include "sensesp_app_builder.h"

using namespace sensesp;

reactesp::ReactESP app;

// Global variables for bilge run count, run duration, and state
unsigned int bilge_run_count = 0;
unsigned long last_run_start_time = 0;
float last_run_duration_seconds = 0.0;
bool bilge_pump_running = false;

// Signal K output objects for bilge state, run count, and duration
SKOutputBool* sk_bilge_pump_state;
SKOutputInt* sk_bilge_run_count;
SKOutputFloat* sk_bilge_run_duration;

// The setup function performs one-time application initialization.
void setup() {
  SetupSerialDebug(115200);

  // Construct the global SensESPApp() object
  SensESPAppBuilder builder;
  sensesp_app = (&builder)
                    // Set a custom hostname for the app.
                    ->set_hostname("nootka_bilgetank_mon")
                    ->get_app();

  // Read Optical input from GPIO 35
  const uint8_t kDigitalInput1Pin = 35;
  auto* bilge_input =
      new DigitalInputChange(kDigitalInput1Pin, INPUT_PULLUP, CHANGE);

  // Connect bilge input to Signal K output.
  sk_bilge_pump_state = new SKOutputBool(
      "environment.bilge.main.state",         // Signal K path
      "/Environment/Bilge/Main/State",        // configuration path
      new SKMetadata("",                      // No units for boolean values
                     "Bilge pump run state")  // Value description
  );

  // Create Signal K outputs for run count and last run duration
  sk_bilge_run_count = new SKOutputInt(
      "environment.bilge.main.runCount",      // Signal K path for run count
      "/Environment/Bilge/Main/RunCount",     // Configuration path
      new SKMetadata("count",                 // Unit: number of runs
                     "Bilge pump run count")  // Description
  );

  sk_bilge_run_duration = new SKOutputFloat(
      "environment.bilge.main.lastRunDuration",       // Signal K path for run
                                                      // duration
      "/Environment/Bilge/Main/LastRunDuration",      // Configuration path
      new SKMetadata("s",                             // Unit: seconds
                     "Last bilge pump run duration")  // Description
  );

  // Track bilge run count and duration
  bilge_input->connect_to(new DebounceBool(250))
      ->connect_to(new LambdaConsumer<bool>([](bool pump_state) {
        if (pump_state == true) {
          // Pump turned on
          bilge_pump_running = true;
          bilge_run_count++;
          last_run_start_time = millis();  // Record start time
          Serial.println("Bilge pump started.");
        } else {
          // Pump turned off
          bilge_pump_running = false;
          if (last_run_start_time != 0) {
            last_run_duration_seconds = (millis() - last_run_start_time) /
                                        1000.0;  // Calculate duration
            last_run_start_time = 0;
            Serial.println("Bilge pump stopped.");
            Serial.print("Last run duration (s): ");
            Serial.println(last_run_duration_seconds);  // Print duration
          }
        }
      }));

  // Set up a repeating timer to report the current state to Signal K every
  // second
  app.onRepeat(1000, []() {
    sk_bilge_pump_state->set_input(
        bilge_pump_running);  // Report the current pump state
    sk_bilge_run_count->set_input(
        bilge_run_count);  // Report the current run count
    sk_bilge_run_duration->set_input(
        last_run_duration_seconds);  // Report last run duration
  });

  // Start the SensESP application
  sensesp_app->start();
}

void loop() { app.tick(); }
