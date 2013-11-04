{
  'targets': [
    {
      'target_name': 'latency-benchmark',
      'type': 'executable',
      'sources': [
        'src/latency-benchmark.c',
        'src/latency-benchmark.h',
        'src/screenscraper.h',
        'src/server.c',
        'src/oculus.cpp',
        'src/oculus.h',
        '<(INTERMEDIATE_DIR)/packaged-html-files.c',
      ],
      'dependencies': [
        'mongoose',
        'libovr',
      ],
      'actions': [
        {
          'action_name': 'package html files',
          'inputs': [
            'files-to-c-arrays.py',
            'html/2048.png',
            'html/compatibility.js',
            'html/draw-pattern.js',
            'html/gradient.png',
            'html/hardware-latency-test.html',
            'html/index.html',
            'html/keep-server-alive.js',
            'html/latency-benchmark.css',
            'html/latency-benchmark.html',
            'html/latency-benchmark.js',
            'html/worker.js',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/packaged-html-files.c',
          ],
          'action': ['python', 'files-to-c-arrays.py', '<@(_outputs)', '<@(_inputs)'],
          'msvs_cygwin_shell': 0,
        },
      ],
      'conditions': [
        ['OS=="linux"', {
          'sources': [
            'src/x11/screenscraper.c',
            'src/x11/main.c',
          ],
        }],
        ['OS=="win"', {
          'sources': [
            'src/win/main.cpp',
            'src/win/screenscraper.cpp',
			'src/win/stdafx.h',
          ],
        }],
        ['OS=="mac"', {
          'sources': [
            'src/mac/main.m',
            'src/mac/screenscraper.m',
          ],
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/Cocoa.framework',
              '$(SDKROOT)/System/Library/Frameworks/CoreVideo.framework',
              '$(SDKROOT)/System/Library/Frameworks/IOKit.framework',
              '$(SDKROOT)/System/Library/Frameworks/OpenGL.framework',
            ],
          },
        }],
      ],
      'msvs_settings': {
        'VCCLCompilerTool': {
          'CompileAs': 2, # Compile C as C++, since msvs doesn't support C99
        },
        'VCLinkerTool': {
          'AdditionalDependencies': [
            'winmm.lib',
            'setupapi.lib',
          ],
        },
      },
    },
    {
      'target_name': 'mongoose',
      'type': 'static_library',
      'sources': [
        'third_party/mongoose/mongoose.c',
        'third_party/mongoose/mongoose.h',
      ],
    },
    {
      'target_name': 'libovr',
      'type': 'static_library',
      'sources': [
        'third_party/LibOVR/Include/OVR.h',
        'third_party/LibOVR/Include/OVRVersion.h',
        'third_party/LibOVR/Src/Kernel/OVR_Alg.cpp',
        'third_party/LibOVR/Src/Kernel/OVR_Alg.h',
        'third_party/LibOVR/Src/Kernel/OVR_Allocator.cpp',
        'third_party/LibOVR/Src/Kernel/OVR_Allocator.h',
        'third_party/LibOVR/Src/Kernel/OVR_Array.h',
        'third_party/LibOVR/Src/Kernel/OVR_Atomic.cpp',
        'third_party/LibOVR/Src/Kernel/OVR_Atomic.h',
        'third_party/LibOVR/Src/Kernel/OVR_Color.h',
        'third_party/LibOVR/Src/Kernel/OVR_ContainerAllocator.h',
        'third_party/LibOVR/Src/Kernel/OVR_File.cpp',
        'third_party/LibOVR/Src/Kernel/OVR_File.h',
        'third_party/LibOVR/Src/Kernel/OVR_FileFILE.cpp',
        'third_party/LibOVR/Src/Kernel/OVR_Hash.h',
        'third_party/LibOVR/Src/Kernel/OVR_KeyCodes.h',
        'third_party/LibOVR/Src/Kernel/OVR_List.h',
        'third_party/LibOVR/Src/Kernel/OVR_Log.cpp',
        'third_party/LibOVR/Src/Kernel/OVR_Log.h',
        'third_party/LibOVR/Src/Kernel/OVR_Math.cpp',
        'third_party/LibOVR/Src/Kernel/OVR_Math.h',
        'third_party/LibOVR/Src/Kernel/OVR_RefCount.cpp',
        'third_party/LibOVR/Src/Kernel/OVR_RefCount.h',
        'third_party/LibOVR/Src/Kernel/OVR_Std.cpp',
        'third_party/LibOVR/Src/Kernel/OVR_Std.h',
        'third_party/LibOVR/Src/Kernel/OVR_String.cpp',
        'third_party/LibOVR/Src/Kernel/OVR_String.h',
        'third_party/LibOVR/Src/Kernel/OVR_String_FormatUtil.cpp',
        'third_party/LibOVR/Src/Kernel/OVR_String_PathUtil.cpp',
        'third_party/LibOVR/Src/Kernel/OVR_StringHash.h',
        'third_party/LibOVR/Src/Kernel/OVR_SysFile.cpp',
        'third_party/LibOVR/Src/Kernel/OVR_SysFile.h',
        'third_party/LibOVR/Src/Kernel/OVR_System.cpp',
        'third_party/LibOVR/Src/Kernel/OVR_System.h',
        'third_party/LibOVR/Src/Kernel/OVR_Threads.h',
        'third_party/LibOVR/Src/Kernel/OVR_Timer.cpp',
        'third_party/LibOVR/Src/Kernel/OVR_Timer.h',
        'third_party/LibOVR/Src/Kernel/OVR_Types.h',
        'third_party/LibOVR/Src/Kernel/OVR_UTF8Util.cpp',
        'third_party/LibOVR/Src/Kernel/OVR_UTF8Util.h',
        'third_party/LibOVR/Src/OVR_Device.h',
        'third_party/LibOVR/Src/OVR_DeviceConstants.h',
        'third_party/LibOVR/Src/OVR_DeviceHandle.cpp',
        'third_party/LibOVR/Src/OVR_DeviceHandle.h',
        'third_party/LibOVR/Src/OVR_DeviceImpl.cpp',
        'third_party/LibOVR/Src/OVR_DeviceImpl.h',
        'third_party/LibOVR/Src/OVR_DeviceMessages.h',
        'third_party/LibOVR/Src/OVR_HIDDevice.h',
        'third_party/LibOVR/Src/OVR_HIDDeviceBase.h',
        'third_party/LibOVR/Src/OVR_HIDDeviceImpl.h',
        'third_party/LibOVR/Src/OVR_JSON.cpp',
        'third_party/LibOVR/Src/OVR_JSON.h',
        'third_party/LibOVR/Src/OVR_LatencyTestImpl.cpp',
        'third_party/LibOVR/Src/OVR_LatencyTestImpl.h',
        'third_party/LibOVR/Src/OVR_Profile.cpp',
        'third_party/LibOVR/Src/OVR_Profile.h',
        'third_party/LibOVR/Src/OVR_SensorFilter.cpp',
        'third_party/LibOVR/Src/OVR_SensorFilter.h',
        'third_party/LibOVR/Src/OVR_SensorFusion.cpp',
        'third_party/LibOVR/Src/OVR_SensorFusion.h',
        'third_party/LibOVR/Src/OVR_SensorImpl.cpp',
        'third_party/LibOVR/Src/OVR_SensorImpl.h',
        'third_party/LibOVR/Src/OVR_ThreadCommandQueue.cpp',
        'third_party/LibOVR/Src/OVR_ThreadCommandQueue.h',
        'third_party/LibOVR/Src/Util/Util_LatencyTest.cpp',
        'third_party/LibOVR/Src/Util/Util_LatencyTest.h',
        'third_party/LibOVR/Src/Util/Util_Render_Stereo.cpp',
        'third_party/LibOVR/Src/Util/Util_Render_Stereo.h',
      ],
      'msvs_configuration_attributes': {
        # Oculus code assumes Unicode mode.
        'CharacterSet': '1',
      },
      'conditions': [
        ['OS=="linux"', {
          'sources': [
            'third_party/LibOVR/Src/OVR_Linux_DeviceManager.cpp',
            'third_party/LibOVR/Src/OVR_Linux_DeviceManager.h',
            'third_party/LibOVR/Src/OVR_Linux_HIDDevice.cpp',
            'third_party/LibOVR/Src/OVR_Linux_HIDDevice.h',
            'third_party/LibOVR/Src/OVR_Linux_HMDDevice.cpp',
            'third_party/LibOVR/Src/OVR_Linux_HMDDevice.h',
            'third_party/LibOVR/Src/OVR_Linux_SensorDevice.cpp',
            'third_party/LibOVR/Src/Kernel/OVR_ThreadsPthread.cpp',
          ],
        }],
        ['OS=="win"', {
          'sources': [
            'third_party/LibOVR/Src/OVR_Win32_DeviceManager.cpp',
            'third_party/LibOVR/Src/OVR_Win32_DeviceManager.h',
            'third_party/LibOVR/Src/OVR_Win32_DeviceStatus.cpp',
            'third_party/LibOVR/Src/OVR_Win32_DeviceStatus.h',
            'third_party/LibOVR/Src/OVR_Win32_HIDDevice.cpp',
            'third_party/LibOVR/Src/OVR_Win32_HIDDevice.h',
            'third_party/LibOVR/Src/OVR_Win32_HMDDevice.cpp',
            'third_party/LibOVR/Src/OVR_Win32_HMDDevice.h',
            'third_party/LibOVR/Src/OVR_Win32_SensorDevice.cpp',
            'third_party/LibOVR/Src/OVR_Win32_SensorDevice.h',
            'third_party/LibOVR/Src/Kernel/OVR_ThreadsWinAPI.cpp',
          ],
        }],
        ['OS=="mac"', {
          'sources': [
            'third_party/LibOVR/Src/OVR_OSX_DeviceManager.cpp',
            'third_party/LibOVR/Src/OVR_OSX_DeviceManager.h',
            'third_party/LibOVR/Src/OVR_OSX_HIDDevice.cpp',
            'third_party/LibOVR/Src/OVR_OSX_HIDDevice.h',
            'third_party/LibOVR/Src/OVR_OSX_HMDDevice.cpp',
            'third_party/LibOVR/Src/OVR_OSX_HMDDevice.h',
            'third_party/LibOVR/Src/OVR_OSX_SensorDevice.cpp',
            'third_party/LibOVR/Src/Kernel/OVR_ThreadsPthread.cpp',
          ],
        }],
      ],
    }
  ],

  'target_defaults': {
    'default_configuration': 'Debug',
    'configurations': {
      'Debug': {
        'defines': [ 'DEBUG', '_DEBUG' ],
        'cflags': [ '-g', '-O0' ],
        'msvs_settings': {
          'VCCLCompilerTool': {
            'RuntimeLibrary': 1, # static debug
            'Optimization': 0, # optimizeDisabled (/Od)
          },
        },
        'xcode_settings': {'GCC_OPTIMIZATION_LEVEL': 0 } # -O0 No optimization
      },
      'Release': {
        'defines': [ 'NDEBUG' ],
        'cflags': [ '-Os' ],
        'msvs_settings': {
          'VCCLCompilerTool': {
            'RuntimeLibrary': 0, # static release
            'Optimization': 3, # full optimization (/Ox)
          },
        },
        'xcode_settings': { 'GCC_OPTIMIZATION_LEVEL': 's' }, # -Os optimize for size+speed
      }
    },
    'conditions': [
      ['OS=="linux"', {
        'ldflags': [ '-pthread' ],
        'link_settings': {
          'libraries' : [
          '-ldl',
          '-lX11',
          '-lXtst',
          '-lGL',
          '-ludev',
          '-lXinerama',
          ],
        },
      }],
      ['OS=="win"', {
        'defines': [ '_WINDOWS', 'WIN32' ],
      }],
      ['OS=="mac"', {
        'xcode_settings': {
          'ARCHS': '$(ARCHS_STANDARD_64_BIT)',
        },
      }],
    ],
    'msvs_settings': {
      'VCLinkerTool': {
        'GenerateDebugInformation': 'true',
        'SubSystem': 2, # Windows
      },
    },
    'xcode_settings': {
      # This prevents a silly warning in XCode's UI ("Update project to recommended settings"). Unfortunately there's no way to prevent the warning on 'action' targets; oh well.
      'COMBINE_HIDPI_IMAGES': 'YES',
    },
  },

  'conditions': [
    ['OS=="mac"',
      # The XCode project generator automatically adds a bogus "All" target with bad xcode_settings unless we define one here.
      {
        'targets': [
          {
            'target_name': 'All',
            'type': 'static_library',
          },
        ],
      },
    ],
  ],
}
