#include "../third_party/LibOVR/Include/OVR.h"
extern "C" {
#include "screenscraper.h"
#include "oculus.h"
}

extern "C" bool run_hardware_latency_test(const char **result) {
  assert(result);
  *result = "Unknown error";
  // Initialize the Oculus library and connect to the latency tester device.
  OVR::System::Init();
  OVR::DeviceManager *manager = OVR::DeviceManager::Create();
  OVR::LatencyTestDevice *latency_device = manager->EnumerateDevices<OVR::LatencyTestDevice>().CreateDevice();
  manager->Release();
  // Check that the latency tester is plugged in.
  if (!latency_device) {
    *result = "Oculus latency tester not found.";
    return false;
  }
  OVR::Util::LatencyTest latency_util;
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
        latency_util.SetDevice(NULL);
        *result = "Unexpected color requested by latency tester.";
        return false;
      }
      displayed_color = color.R;
    }
    const char *oculusResults = latency_util.GetResultsString();
    if (oculusResults != NULL) {
      // Success!
      *result = oculusResults;
      latency_util.SetDevice(NULL);
      return true;
    }
    usleep(1000);
    // TODO: timeout
  }
  // TODO: try this
  //OVR::System::Destroy();
}
