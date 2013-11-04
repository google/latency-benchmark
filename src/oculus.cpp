#include "../third_party/LibOVR/Include/OVR.h"

#ifndef _WINDOWS
// On all platforms except Windows, the rest of the code is compiled as C.
extern "C" {
#endif
#include "screenscraper.h"
#include "oculus.h"
#ifndef _WINDOWS
}
#endif

static char result_buffer[2048];
static OVR::DeviceManager *manager = NULL;
static OVR::LatencyTestDevice *global_latency_device = NULL;

class Handler : public OVR::MessageHandler {
  virtual void OnMessage(const OVR::Message &message) {
    if (message.Type == OVR::Message_DeviceRemoved) {
      OVR::LatencyTestDevice *local_device = global_latency_device;
      global_latency_device = NULL;
      // Ugh, this isn't guaranteed to be thread safe but it's easier than
      // implementing a cross-platform lock primitive. Another alternative
      // would be to just never release the device.
      usleep(1000);
      local_device->Release();
    }
    if (message.Type == OVR::Message_LatencyTestButton) {
      send_keystroke_t();
    }
  }
};
static Handler handler;

// Installs an event handler on any created device so we can detect when the
// device's button is pressed.
static OVR::LatencyTestDevice *get_device() {
  OVR::LatencyTestDevice *local_device = global_latency_device;
  if(local_device == NULL) {
    local_device =
      manager->EnumerateDevices<OVR::LatencyTestDevice>().CreateDevice();
    if (local_device != NULL) {
      local_device->SetMessageHandler(&handler);
      global_latency_device = local_device;
    }
  }
  if (local_device) {
    local_device->AddRef();
  }
  return local_device;
}

// Must be called before all other functions in this file.
extern "C" void init_oculus() {
  OVR::System::Init();
  manager = OVR::DeviceManager::Create();
  OVR::LatencyTestDevice *latency_device = get_device();
}

// Returns true if an Oculus Latency Tester is attached.
extern "C" bool latency_tester_available() {
  assert(manager);
  OVR::LatencyTestDevice *latency_device = get_device();
  if (latency_device) {
    latency_device->Release();
    return true;
  }
  return false;
}

// Drives the Oculus Latency Tester to run a latency test. Works together
// with hardware-latency-test.html, communicating using keystrokes ('B' means
// draw black, 'W' means draw white.
extern "C" bool run_hardware_latency_test(const char **result) {
  assert(manager);
  assert(result);
  *result = "Unknown error";
  OVR::LatencyTestDevice *latency_device = get_device();
  // Check that the latency tester is plugged in.
  if (!latency_device) {
    *result = "Oculus latency tester not found.";
    return false;
  }
  OVR::Util::LatencyTest latency_util;
  // LatencyTest needs to take over handling of messages for the device.
  latency_device->SetMessageHandler(NULL);
  latency_util.SetDevice(latency_device);
  latency_device->Release();
  // Main test loop.
  latency_util.BeginTest();
  int displayed_color = -1;
  while (true) {
    latency_util.ProcessInputs();
    OVR::Color color;
    latency_util.DisplayScreenColor(color);
    if (color.R != displayed_color) {
      if (color.R == 255 && color.G == 255 && color.B == 255) {
        // Display white.
        send_keystroke_w();
      } else if (color.R == 0 && color.G == 0 && color.B == 0) {
        // Display black.
        send_keystroke_b();
      } else {
        // We can only display white or black.
        *result = "Unexpected color requested by latency tester.";
        latency_util.SetDevice(NULL);
        latency_device->SetMessageHandler(&handler);
        return false;
      }
      displayed_color = color.R;
    }
    const char *oculusResults = latency_util.GetResultsString();
    if (oculusResults != NULL) {
      // Success! Copy the string into a buffer because it will be deallocated
      // when the LatencyTest instance goes away.
      strncpy(result_buffer, oculusResults, sizeof(result_buffer));
      result_buffer[sizeof(result_buffer) - 1] = '\0';
      *result = result_buffer;
      latency_util.SetDevice(NULL);
      latency_device->SetMessageHandler(&handler);
      return true;
    }
    usleep(1000);
    // TODO: timeout
  }
}
